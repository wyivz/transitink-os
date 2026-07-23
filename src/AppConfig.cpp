#include "AppConfig.h"

#include <ArduinoJson.h>

namespace {

String asString(JsonVariantConst value) {
    if (value.isNull()) {
        return "";
    }
    return String(value.as<const char*>());
}

std::string asStdString(JsonVariantConst value) {
    return asString(value).c_str();
}

bool isStringWithinLimit(const String& value, std::size_t limit) {
    return value.length() <= limit;
}

bool areCommonFieldsWithinLimits(const DeviceConfig& config) {
    return isStringWithinLimit(config.wifiSsid, transitink::kMaxWifiSsidBytes) &&
           isStringWithinLimit(config.wifiPassword, transitink::kMaxWifiCredentialBytes);
}

bool isScheduledWakeValid(const DeviceConfig& config) {
    constexpr uint16_t kMinutesPerDay = 24 * 60;
    return config.scheduledWakeStartMinutes < kMinutesPerDay &&
           config.scheduledWakeEndMinutes < kMinutesPerDay &&
           (!config.scheduledWakeEnabled ||
            config.scheduledWakeStartMinutes != config.scheduledWakeEndMinutes);
}

bool areWidgetsValid(const DeviceConfig& config) {
    for (const auto& widget : config.widgets) {
        if (!transitink::isWidgetConfigValid(widget)) {
            return false;
        }
    }
    return true;
}

void copyCommonFields(const JsonDocument& doc, DeviceConfig& parsed) {
    parsed.wifiSsid = asString(doc["wifi_ssid"]);
    parsed.wifiPassword = asString(doc["wifi_password"]);
    parsed.sleepEnabled = doc["sleep_enabled"] | static_cast<bool>(SLEEP_ENABLED_DEFAULT);
    parsed.wakeDurationMinutes = doc["wake_duration_minutes"] | SLEEP_WAKE_DEFAULT_MINUTES;
    if (parsed.wakeDurationMinutes < 1) {
        parsed.wakeDurationMinutes = 1;
    }
    if (parsed.wakeDurationMinutes > 60) {
        parsed.wakeDurationMinutes = 60;
    }
    parsed.sleepMaintenanceHours = doc["sleep_maintenance_hours"] | SLEEP_MAINTENANCE_DEFAULT_HOURS;
    if (parsed.sleepMaintenanceHours > 24) {
        parsed.sleepMaintenanceHours = 24;
    }
    parsed.scheduledWakeEnabled = doc.containsKey("scheduled_wake_enabled")
                                      ? doc["scheduled_wake_enabled"].as<bool>()
                                      : static_cast<bool>(SCHEDULED_WAKE_ENABLED_DEFAULT);
    parsed.scheduledWakeStartMinutes =
        doc["scheduled_wake_start_minutes"] | SCHEDULED_WAKE_START_DEFAULT_MINUTES;
    parsed.scheduledWakeEndMinutes =
        doc["scheduled_wake_end_minutes"] | SCHEDULED_WAKE_END_DEFAULT_MINUTES;
}

bool parseWidget(JsonObjectConst item, transitink::WidgetConfig& widget) {
    if (!transitink::parseWidgetTypeId(asStdString(item["type"]), widget.type)) {
        return false;
    }

    switch (widget.type) {
        case transitink::WidgetType::Disabled:
            break;
        case transitink::WidgetType::TtcEta: {
            JsonObjectConst ttc = item["ttc"].as<JsonObjectConst>();
            if (ttc.isNull()) {
                return false;
            }
            widget.ttc.routeId = asStdString(ttc["route_id"]);
            widget.ttc.directionId = asStdString(ttc["direction_id"]);
            widget.ttc.stopId = asStdString(ttc["stop_id"]);
            widget.ttc.routeLabel = asStdString(ttc["route_label"]);
            widget.ttc.stopLabel = asStdString(ttc["stop_label"]);
            widget.ttc.destinationLabel = asStdString(ttc["destination_label"]);
            break;
        }
    }

    return transitink::isWidgetConfigValid(widget);
}

void writeWidget(JsonObject item, const transitink::WidgetConfig& widget) {
    item["type"] = transitink::widgetTypeId(widget.type);
    switch (widget.type) {
        case transitink::WidgetType::Disabled:
            break;
        case transitink::WidgetType::TtcEta: {
            JsonObject ttc = item.createNestedObject("ttc");
            ttc["route_id"] = widget.ttc.routeId.c_str();
            ttc["direction_id"] = widget.ttc.directionId.c_str();
            ttc["stop_id"] = widget.ttc.stopId.c_str();
            ttc["route_label"] = widget.ttc.routeLabel.c_str();
            ttc["stop_label"] = widget.ttc.stopLabel.c_str();
            ttc["destination_label"] = widget.ttc.destinationLabel.c_str();
            break;
        }
    }
}

}  // namespace

bool parseDeviceConfigJson(const String& json, DeviceConfig& config, String& error) {
    DynamicJsonDocument doc(transitink::kConfigJsonCapacity);
    const DeserializationError status = deserializeJson(doc, json);
    if (status) {
        error = "Invalid config JSON";
        return false;
    }

    DeviceConfig parsed;
    parsed.schemaVersion = doc["schema_version"] | transitink::kConfigSchemaVersion;
    copyCommonFields(doc, parsed);

    JsonArrayConst widgets = doc["widgets"].as<JsonArrayConst>();
    if (!widgets.isNull()) {
        std::size_t index = 0;
        for (JsonObjectConst item : widgets) {
            if (index >= transitink::kWidgetSlotCount) {
                error = "Too many widgets";
                return false;
            }
            if (!parseWidget(item, parsed.widgets[index])) {
                error = "Invalid widget settings";
                return false;
            }
            ++index;
        }
    }

    if (!areCommonFieldsWithinLimits(parsed) || !isScheduledWakeValid(parsed) ||
        !areWidgetsValid(parsed)) {
        error = "Config validation failed";
        return false;
    }

    config = parsed;
    error = "";
    return true;
}

String serializeDeviceConfigJson(const DeviceConfig& config) {
    String json;
    DeviceConfigSerializationMetrics metrics;
    String error;
    serializeDeviceConfigJsonChecked(config, json, metrics, error);
    return json;
}

bool serializeDeviceConfigJsonChecked(const DeviceConfig& config,
                                      String& json,
                                      DeviceConfigSerializationMetrics& metrics,
                                      String& error) {
    if (!areCommonFieldsWithinLimits(config) || !isScheduledWakeValid(config) ||
        !areWidgetsValid(config)) {
        json = "";
        error = "Config validation failed";
        return false;
    }

    DynamicJsonDocument doc(transitink::kConfigJsonCapacity);
    doc["schema_version"] = transitink::kConfigSchemaVersion;
    doc["wifi_ssid"] = config.wifiSsid;
    doc["wifi_password"] = config.wifiPassword;
    doc["sleep_enabled"] = config.sleepEnabled;
    doc["wake_duration_minutes"] = config.wakeDurationMinutes;
    doc["sleep_maintenance_hours"] = config.sleepMaintenanceHours;
    doc["scheduled_wake_enabled"] = config.scheduledWakeEnabled;
    doc["scheduled_wake_start_minutes"] = config.scheduledWakeStartMinutes;
    doc["scheduled_wake_end_minutes"] = config.scheduledWakeEndMinutes;

    JsonArray widgets = doc.createNestedArray("widgets");
    for (const auto& widget : config.widgets) {
        writeWidget(widgets.createNestedObject(), widget);
    }

    metrics.documentBytes = doc.memoryUsage();
    if (doc.overflowed() || measureJson(doc) > transitink::kConfigJsonSafeBytes) {
        error = "Config too large";
        return false;
    }
    json = "";
    if (serializeJson(doc, json) == 0) {
        error = "Failed to serialize config";
        return false;
    }
    metrics.jsonBytes = json.length();
    error = "";
    return true;
}

bool hasUsableConfig(const DeviceConfig& config) {
    return config.wifiSsid.length() > 0 && areWidgetsValid(config) &&
           isScheduledWakeValid(config);
}
