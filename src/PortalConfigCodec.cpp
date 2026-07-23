#include "PortalConfigCodec.h"

#include <ArduinoJson.h>

#include <memory>
#include <new>

namespace {

void writeWidget(JsonObject item, const transitink::WidgetConfig& widget) {
    item["type"] = transitink::widgetTypeId(widget.type);
    if (widget.type == transitink::WidgetType::TtcEta) {
        JsonObject ttc = item.createNestedObject("ttc");
        ttc["route_id"] = widget.ttc.routeId.c_str();
        ttc["direction_id"] = widget.ttc.directionId.c_str();
        ttc["stop_id"] = widget.ttc.stopId.c_str();
        ttc["route_label"] = widget.ttc.routeLabel.c_str();
        ttc["stop_label"] = widget.ttc.stopLabel.c_str();
        ttc["destination_label"] = widget.ttc.destinationLabel.c_str();
    }
}

}  // namespace

bool encodePortalConfig(const DeviceConfig& config,
                        const bus_eta::BatterySnapshot& battery,
                        const String& firmwareVersion,
                        const String& csrfToken,
                        String& outJson,
                        String& error) {
    outJson = "";
    DynamicJsonDocument doc(transitink::kConfigJsonCapacity);
    doc["schema_version"] = transitink::kConfigSchemaVersion;
    doc["wifi_ssid"] = config.wifiSsid;
    doc["wifi_password_set"] = config.wifiPassword.length() > 0;
    doc["sleep_enabled"] = config.sleepEnabled;
    doc["wake_duration_minutes"] = config.wakeDurationMinutes;
    doc["sleep_maintenance_hours"] = config.sleepMaintenanceHours;
    doc["scheduled_wake_enabled"] = config.scheduledWakeEnabled;
    doc["scheduled_wake_start_minutes"] = config.scheduledWakeStartMinutes;
    doc["scheduled_wake_end_minutes"] = config.scheduledWakeEndMinutes;
    doc["firmware_version"] = firmwareVersion;
    doc["csrf_token"] = csrfToken;

    JsonArray widgets = doc.createNestedArray("widgets");
    for (const auto& widget : config.widgets) {
        writeWidget(widgets.createNestedObject(), widget);
    }

    JsonObject batteryJson = doc.createNestedObject("battery");
    batteryJson["valid"] = battery.valid;
    batteryJson["percent"] = battery.percent;
    batteryJson["voltage_mv"] = battery.voltageMv;
    batteryJson["charging"] = battery.charging;
    batteryJson["full"] = battery.full;
    batteryJson["power_present"] = battery.powerPresent;

    if (doc.overflowed() || measureJson(doc) > transitink::kConfigJsonSafeBytes) {
        error = "Settings payload too large";
        return false;
    }
    if (serializeJson(doc, outJson) == 0) {
        error = "Failed to build settings JSON";
        return false;
    }
    error = "";
    return true;
}

bool decodePortalSave(const String& body,
                      const DeviceConfig& current,
                      DeviceConfig& outConfig,
                      String& error) {
    if (body.length() > transitink::kConfigJsonCapacity) {
        error = "Settings payload too large";
        return false;
    }

    DynamicJsonDocument doc(transitink::kConfigJsonCapacity);
    const DeserializationError jsonError = deserializeJson(doc, body);
    if (jsonError || doc.overflowed() || !doc.is<JsonObject>()) {
        error = "Invalid settings format";
        return false;
    }

    JsonVariant password = doc["wifi_password"];
    if (!password.isNull() && !password.is<const char*>()) {
        error = "Invalid Wi-Fi password format";
        return false;
    }
    const char* submittedPassword = password | "";
    if (submittedPassword[0] == '\0') {
        doc["wifi_password"] = current.wifiPassword;
    }

    String merged;
    if (serializeJson(doc, merged) == 0 || merged.length() > transitink::kConfigJsonCapacity) {
        error = "Settings payload too large";
        return false;
    }
    return parseDeviceConfigJson(merged, outConfig, error);
}

bool savePortalConfig(const String& body,
                      DeviceConfig& liveConfig,
                      ConfigStore& store,
                      String& error) {
    std::unique_ptr<DeviceConfig> parsed(new (std::nothrow) DeviceConfig());
    if (!parsed) {
        error = "Out of memory";
        return false;
    }
    if (!decodePortalSave(body, liveConfig, *parsed, error)) {
        return false;
    }
    if (!store.save(*parsed)) {
        error = "Save failed";
        return false;
    }
    liveConfig = *parsed;
    error = "";
    return true;
}
