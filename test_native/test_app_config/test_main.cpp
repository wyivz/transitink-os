#include "ConfigStore.h"
#include "PortalConfigCodec.h"
#include "core/PortalRequestAuth.h"

#include <ArduinoJson.h>
#include <unity.h>

#include <cstdio>
#include <string>

namespace {

std::string repeated(char value, std::size_t size) {
    return std::string(size, value);
}

transitink::WidgetConfig validTtc(const std::string& route = "506") {
    transitink::WidgetConfig widget;
    widget.type = transitink::WidgetType::TtcEta;
    widget.ttc.routeId = route;
    widget.ttc.directionId = "0";
    widget.ttc.stopId = "8431";
    widget.ttc.routeLabel = route;
    widget.ttc.stopLabel = "College St at University Ave";
    widget.ttc.destinationLabel = "Eastbound";
    return widget;
}

DeviceConfig roundTripConfig() {
    DeviceConfig config;
    config.wifiSsid = "TransitInk-Test";
    config.wifiPassword = "password";
    config.scheduledWakeEnabled = true;
    config.scheduledWakeStartMinutes = 8 * 60;
    config.scheduledWakeEndMinutes = 9 * 60;
    config.widgets[0] = validTtc("501");
    config.widgets[1] = validTtc("504");
    config.widgets[2] = validTtc("506");
    config.widgets[3] = validTtc("511");
    return config;
}

DeviceConfig sentinelConfig() {
    DeviceConfig config = roundTripConfig();
    config.wifiSsid = "sentinel";
    return config;
}

String portalBody(const DeviceConfig& config, const String& submittedPassword) {
    StaticJsonDocument<transitink::kConfigJsonCapacity> doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, serializeDeviceConfigJson(config)));
    doc["wifi_password"] = submittedPassword;
    String body;
    serializeJson(doc, body);
    return body;
}

void assertSentinelPreserved(const DeviceConfig& config) {
    TEST_ASSERT_EQUAL_STRING("sentinel", config.wifiSsid.c_str());
    TEST_ASSERT_EQUAL_UINT16(transitink::kConfigSchemaVersion, config.schemaVersion);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::TtcEta),
                          static_cast<int>(config.widgets[0].type));
    TEST_ASSERT_EQUAL_STRING("501", config.widgets[0].ttc.routeId.c_str());
}

void assertParseFailurePreserves(const std::string& json) {
    DeviceConfig config = sentinelConfig();
    String error;
    TEST_ASSERT_FALSE(parseDeviceConfigJson(json.c_str(), config, error));
    TEST_ASSERT_FALSE(error.empty());
    assertSentinelPreserved(config);
}

std::string disabledWidgets(std::size_t count) {
    std::string json = "[";
    for (std::size_t index = 0; index < count; ++index) {
        if (index > 0) {
            json += ',';
        }
        json += R"({"type":"disabled"})";
    }
    json += ']';
    return json;
}

std::string v3Json(const std::string& widgets, const std::string& wifiSsid = "wifi") {
    return R"({"schema_version":3,"wifi_ssid":")" + wifiSsid + R"(","widgets":)" +
           widgets + '}';
}

void test_has_usable_config_requires_wifi_valid_widgets_and_wake_window() {
    DeviceConfig config;
    config.wifiSsid = "wifi";
    TEST_ASSERT_TRUE(hasUsableConfig(config));

    config.wifiSsid = "";
    TEST_ASSERT_FALSE(hasUsableConfig(config));

    config.wifiSsid = "wifi";
    config.widgets[0] = validTtc();
    TEST_ASSERT_TRUE(hasUsableConfig(config));

    config.widgets[0].ttc.stopId.clear();
    TEST_ASSERT_FALSE(hasUsableConfig(config));

    config.widgets[0] = transitink::WidgetConfig{};
    config.scheduledWakeEnabled = true;
    config.scheduledWakeStartMinutes = 480;
    config.scheduledWakeEndMinutes = 480;
    TEST_ASSERT_FALSE(hasUsableConfig(config));
}

void test_v3_round_trip_uses_exact_ttc_payloads() {
    const DeviceConfig original = roundTripConfig();
    DeviceConfigSerializationMetrics metrics;
    String json;
    String error;
    TEST_ASSERT_TRUE(serializeDeviceConfigJsonChecked(original, json, metrics, error));
    TEST_ASSERT_TRUE(error.empty());

    StaticJsonDocument<transitink::kConfigJsonCapacity> doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, json));
    TEST_ASSERT_EQUAL_UINT16(3, doc["schema_version"].as<uint16_t>());
    JsonArrayConst widgets = doc["widgets"].as<JsonArrayConst>();
    TEST_ASSERT_EQUAL_UINT32(transitink::kWidgetSlotCount, widgets.size());

    for (JsonObjectConst widget : widgets) {
        TEST_ASSERT_EQUAL_STRING("ttc_eta", widget["type"].as<const char*>());
        TEST_ASSERT_TRUE(widget.containsKey("ttc"));
        TEST_ASSERT_FALSE(widget.containsKey("bus"));
        TEST_ASSERT_FALSE(widget.containsKey("gmb"));
        TEST_ASSERT_FALSE(widget.containsKey("mtr"));
        TEST_ASSERT_FALSE(widget.containsKey("journey_time"));
    }
    TEST_ASSERT_EQUAL_STRING("501", widgets[0]["ttc"]["route_id"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("8431", widgets[0]["ttc"]["stop_id"].as<const char*>());

    DeviceConfig parsed;
    TEST_ASSERT_TRUE(parseDeviceConfigJson(json, parsed, error));
    TEST_ASSERT_EQUAL_STRING("501", parsed.widgets[0].ttc.routeId.c_str());
    TEST_ASSERT_EQUAL_STRING("0", parsed.widgets[0].ttc.directionId.c_str());
    TEST_ASSERT_EQUAL_STRING("8431", parsed.widgets[0].ttc.stopId.c_str());
    TEST_ASSERT_TRUE(doc["scheduled_wake_enabled"].as<bool>());
    TEST_ASSERT_EQUAL_UINT16(8 * 60, doc["scheduled_wake_start_minutes"].as<uint16_t>());
    TEST_ASSERT_EQUAL_UINT16(9 * 60, doc["scheduled_wake_end_minutes"].as<uint16_t>());
    TEST_ASSERT_TRUE(parsed.scheduledWakeEnabled);
}

void test_scheduled_wake_rejects_invalid_windows_without_mutation() {
    DeviceConfig parsed = sentinelConfig();
    String error;
    std::string invalidJson = v3Json(disabledWidgets(4));
    invalidJson.pop_back();
    invalidJson += R"(,"scheduled_wake_enabled":true,"scheduled_wake_start_minutes":480,"scheduled_wake_end_minutes":480})";
    TEST_ASSERT_FALSE(parseDeviceConfigJson(invalidJson.c_str(), parsed, error));
    TEST_ASSERT_FALSE(error.empty());
    assertSentinelPreserved(parsed);

    DeviceConfig invalid = roundTripConfig();
    invalid.scheduledWakeEndMinutes = invalid.scheduledWakeStartMinutes;
    DeviceConfigSerializationMetrics metrics;
    String json;
    TEST_ASSERT_FALSE(serializeDeviceConfigJsonChecked(invalid, json, metrics, error));
    TEST_ASSERT_TRUE(json.empty());
}

void test_parse_failures_preserve_the_original_config() {
    assertParseFailurePreserves("{");
    assertParseFailurePreserves(v3Json(
        R"([{"type":"bus_eta"},{"type":"disabled"},{"type":"disabled"},{"type":"disabled"}])"));
    assertParseFailurePreserves(v3Json(
        R"([{"type":"ttc_eta","ttc":{"route_id":"506","direction_id":"0","stop_id":""}},{"type":"disabled"},{"type":"disabled"},{"type":"disabled"}])"));
    assertParseFailurePreserves(v3Json(
        R"([{"type":"ttc_eta","ttc":{"route_id":"506","direction_id":"0","stop_id":"8431","route_label":")" +
        repeated('R', transitink::kMaxConfigLabelBytes + 1) +
        R"("}},{"type":"disabled"},{"type":"disabled"},{"type":"disabled"}])"));
    assertParseFailurePreserves(v3Json(disabledWidgets(5)));
    assertParseFailurePreserves(v3Json(disabledWidgets(4),
                                       repeated('S', transitink::kMaxWifiSsidBytes + 1)));
    assertParseFailurePreserves(v3Json(disabledWidgets(4),
                                       repeated('X', transitink::kConfigJsonCapacity + 1)));
}

void test_maximum_valid_config_stays_within_capacity_headroom() {
    DeviceConfig config;
    config.wifiSsid = repeated('S', transitink::kMaxWifiSsidBytes).c_str();
    config.wifiPassword = repeated('P', transitink::kMaxWifiCredentialBytes).c_str();
    for (auto& widget : config.widgets) {
        widget = validTtc();
        widget.ttc.routeId = repeated('R', transitink::kMaxStableIdBytes);
        widget.ttc.directionId = repeated('D', transitink::kMaxStableIdBytes);
        widget.ttc.stopId = repeated('S', transitink::kMaxStableIdBytes);
        widget.ttc.routeLabel = repeated('A', transitink::kMaxConfigLabelBytes);
        widget.ttc.stopLabel = repeated('B', transitink::kMaxConfigLabelBytes);
        widget.ttc.destinationLabel = repeated('C', transitink::kMaxConfigLabelBytes);
    }

    DeviceConfigSerializationMetrics metrics;
    String json;
    String error;
    TEST_ASSERT_TRUE(serializeDeviceConfigJsonChecked(config, json, metrics, error));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(transitink::kConfigJsonSafeBytes, metrics.documentBytes);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(transitink::kConfigJsonSafeBytes, metrics.jsonBytes);
    TEST_ASSERT_EQUAL_UINT32(metrics.jsonBytes, json.size());
    std::printf("CONFIG_CAPACITY document=%zu json=%zu safe=%zu capacity=%zu\n",
                metrics.documentBytes,
                metrics.jsonBytes,
                transitink::kConfigJsonSafeBytes,
                transitink::kConfigJsonCapacity);
}

void test_checked_serializer_rejects_over_limit_input() {
    DeviceConfig config = roundTripConfig();
    config.widgets[0].ttc.stopLabel = repeated('L', transitink::kMaxConfigLabelBytes + 1);
    DeviceConfigSerializationMetrics metrics;
    String json = "sentinel";
    String error;
    TEST_ASSERT_FALSE(serializeDeviceConfigJsonChecked(config, json, metrics, error));
    TEST_ASSERT_TRUE(json.empty());
    TEST_ASSERT_FALSE(error.empty());
    TEST_ASSERT_TRUE(serializeDeviceConfigJson(config).empty());
}

void test_invalid_config_save_preserves_last_valid_json() {
    ConfigStore store;
    TEST_ASSERT_TRUE(store.begin());
    const String validJson = serializeDeviceConfigJson(roundTripConfig());
    TEST_ASSERT_FALSE(validJson.empty());
    Preferences::seedTestValue(validJson);

    DeviceConfig invalid = roundTripConfig();
    invalid.widgets[0].ttc.stopLabel = repeated('L', transitink::kMaxConfigLabelBytes + 1);
    TEST_ASSERT_FALSE(store.save(invalid));
    TEST_ASSERT_EQUAL_STRING(validJson.c_str(), Preferences::testValue().c_str());
    TEST_ASSERT_EQUAL_UINT32(0, Preferences::testWriteCount());
}

void test_sleep_resume_marker_is_persistent_and_idempotent() {
    ConfigStore store;
    TEST_ASSERT_TRUE(store.begin());
    Preferences::seedTestBool(false);
    TEST_ASSERT_FALSE(store.sleepResumePending());
    TEST_ASSERT_TRUE(store.setSleepResumePending(true));
    TEST_ASSERT_TRUE(store.sleepResumePending());
    TEST_ASSERT_EQUAL_UINT32(1, Preferences::testBoolWriteCount());
    TEST_ASSERT_TRUE(store.setSleepResumePending(true));
    TEST_ASSERT_EQUAL_UINT32(1, Preferences::testBoolWriteCount());
    TEST_ASSERT_TRUE(store.setSleepResumePending(false));
    TEST_ASSERT_FALSE(store.sleepResumePending());
    TEST_ASSERT_EQUAL_UINT32(2, Preferences::testBoolWriteCount());
}

void test_portal_get_redacts_password_and_round_trips_four_slots() {
    DeviceConfig config = roundTripConfig();
    config.widgets[3] = transitink::WidgetConfig{};
    config.wifiPassword = "never-return-this";
    bus_eta::BatterySnapshot battery;
    battery.valid = true;
    battery.percent = 73;
    String json;
    String error;

    TEST_ASSERT_TRUE(encodePortalConfig(config, battery, "2.0.0",
                                        "0123456789abcdef0123456789abcdef",
                                        json, error));
    StaticJsonDocument<transitink::kConfigJsonCapacity> doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, json));
    TEST_ASSERT_FALSE(doc.containsKey("wifi_password"));
    TEST_ASSERT_FALSE(doc.containsKey("weather_location_tc"));
    TEST_ASSERT_TRUE(doc["wifi_password_set"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(4, doc["widgets"].size());
    TEST_ASSERT_EQUAL_STRING("ttc_eta", doc["widgets"][0]["type"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("disabled", doc["widgets"][3]["type"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("2.0.0", doc["firmware_version"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef0123456789abcdef",
                             doc["csrf_token"].as<const char*>());
    TEST_ASSERT_EQUAL_UINT8(73, doc["battery"]["percent"].as<uint8_t>());
    TEST_ASSERT_TRUE(doc["scheduled_wake_enabled"].as<bool>());
    TEST_ASSERT_EQUAL(std::string::npos, json.find("never-return-this"));
}

void test_portal_post_preserves_empty_password_and_replaces_non_empty_password() {
    DeviceConfig current = roundTripConfig();
    current.wifiPassword = "stored-secret";
    DeviceConfig parsed;
    String error;

    TEST_ASSERT_TRUE(decodePortalSave(portalBody(current, ""), current, parsed, error));
    TEST_ASSERT_EQUAL_STRING("stored-secret", parsed.wifiPassword.c_str());
    TEST_ASSERT_EQUAL_STRING("501", parsed.widgets[0].ttc.routeId.c_str());

    TEST_ASSERT_TRUE(decodePortalSave(portalBody(current, "replacement"), current, parsed, error));
    TEST_ASSERT_EQUAL_STRING("replacement", parsed.wifiPassword.c_str());
}

void test_portal_post_rejects_malformed_oversized_and_wrong_type() {
    DeviceConfig current = roundTripConfig();
    DeviceConfig parsed = sentinelConfig();
    String error;
    TEST_ASSERT_FALSE(decodePortalSave("{", current, parsed, error));
    TEST_ASSERT_FALSE(error.empty());
    TEST_ASSERT_EQUAL_STRING("sentinel", parsed.wifiSsid.c_str());

    const String oversized(transitink::kConfigJsonCapacity + 1, 'x');
    TEST_ASSERT_FALSE(decodePortalSave(oversized, current, parsed, error));
    TEST_ASSERT_FALSE(error.empty());

    TEST_ASSERT_FALSE(decodePortalSave(
        R"({"schema_version":3,"wifi_ssid":"wifi","wifi_password":"","widgets":[{"type":"bus_eta"}]})",
        current, parsed, error));
    TEST_ASSERT_FALSE(error.empty());
}

void test_portal_failed_save_does_not_mutate_live_config() {
    DeviceConfig live = roundTripConfig();
    live.wifiSsid = "live-before-save";
    DeviceConfig submitted = live;
    submitted.wifiSsid = "candidate";
    ConfigStore store;
    TEST_ASSERT_TRUE(store.begin());
    Preferences::setTestWriteFailure(true);
    String error;
    TEST_ASSERT_FALSE(savePortalConfig(portalBody(submitted, ""), live, store, error));
    Preferences::setTestWriteFailure(false);
    TEST_ASSERT_EQUAL_STRING("live-before-save", live.wifiSsid.c_str());
    TEST_ASSERT_FALSE(error.empty());
}

void test_portal_successful_four_widget_save_survives_reboot_load() {
    DeviceConfig live;
    live.wifiSsid = "TransitInk-Test";
    live.wifiPassword = "stored-secret";
    live.widgets[0] = validTtc("501");
    live.widgets[1] = validTtc("504");

    DeviceConfig submitted = live;
    submitted.widgets[2] = validTtc("506");
    submitted.widgets[3] = transitink::WidgetConfig{};

    ConfigStore store;
    TEST_ASSERT_TRUE(store.begin());
    Preferences::seedTestValue(serializeDeviceConfigJson(live));
    String error;
    TEST_ASSERT_TRUE(savePortalConfig(portalBody(submitted, ""), live, store, error));
    TEST_ASSERT_TRUE(error.empty());

    ConfigStore rebootedStore;
    DeviceConfig rebooted;
    TEST_ASSERT_TRUE(rebootedStore.begin());
    TEST_ASSERT_TRUE(rebootedStore.load(rebooted));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::TtcEta),
                          static_cast<int>(rebooted.widgets[0].type));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::TtcEta),
                          static_cast<int>(rebooted.widgets[1].type));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::TtcEta),
                          static_cast<int>(rebooted.widgets[2].type));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::Disabled),
                          static_cast<int>(rebooted.widgets[3].type));
    TEST_ASSERT_EQUAL_STRING("506", rebooted.widgets[2].ttc.routeId.c_str());
    TEST_ASSERT_EQUAL_STRING("stored-secret", rebooted.wifiPassword.c_str());
}

void test_portal_save_auth_rejects_cross_site_and_invalid_tokens() {
    const std::string expected = "0123456789abcdef0123456789abcdef";
    TEST_ASSERT_TRUE(transitink::isPortalSaveAuthorized(
        "application/json", expected, expected));
    TEST_ASSERT_TRUE(transitink::isPortalSaveAuthorized(
        "application/json; charset=utf-8", expected, expected));
    TEST_ASSERT_FALSE(transitink::isPortalSaveAuthorized(
        "text/plain", expected, expected));
    TEST_ASSERT_FALSE(transitink::isPortalSaveAuthorized(
        "application/x-www-form-urlencoded", expected, expected));
    TEST_ASSERT_FALSE(transitink::isPortalSaveAuthorized(
        "application/json", "", expected));
    TEST_ASSERT_FALSE(transitink::isPortalSaveAuthorized(
        "application/json", "wrong", expected));
    TEST_ASSERT_FALSE(transitink::isPortalSaveAuthorized(
        "application/json", expected, ""));
}

void test_portal_ap_password_and_request_source_are_restricted() {
    const std::string password = transitink::generatePortalApPassword(
        0x01234567U, 0x89abcdefU, 0xfedcba98U);
    TEST_ASSERT_EQUAL_UINT32(12, password.size());
    TEST_ASSERT_TRUE(
        password.find_first_not_of("23456789ABCDEFGHJKLMNPQRSTUVWXYZ") ==
        std::string::npos);
    TEST_ASSERT_TRUE(password != transitink::generatePortalApPassword(1U, 2U, 3U));

    TEST_ASSERT_TRUE(transitink::isPortalRequestSourceAllowed(
        "192.168.4.1", "", "192.168.4.1", false));
    TEST_ASSERT_TRUE(transitink::isPortalRequestSourceAllowed(
        "192.168.4.1:80", "http://192.168.4.1", "192.168.4.1", true));
    TEST_ASSERT_FALSE(transitink::isPortalRequestSourceAllowed(
        "192.168.4.1", "", "192.168.4.1", true));
    TEST_ASSERT_FALSE(transitink::isPortalRequestSourceAllowed(
        "attacker.example", "", "192.168.4.1", false));
    TEST_ASSERT_FALSE(transitink::isPortalRequestSourceAllowed(
        "192.168.4.1", "http://attacker.example", "192.168.4.1", true));
    TEST_ASSERT_FALSE(transitink::isPortalRequestSourceAllowed(
        "192.168.4.1:invalid", "", "192.168.4.1", false));
    TEST_ASSERT_TRUE(transitink::isPortalAccessTokenAuthorized(
        "SESSIONTOKEN", "SESSIONTOKEN"));
    TEST_ASSERT_FALSE(transitink::isPortalAccessTokenAuthorized(
        "WRONGTOKEN", "SESSIONTOKEN"));
    TEST_ASSERT_FALSE(transitink::isPortalAccessTokenAuthorized(
        "", "SESSIONTOKEN"));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_has_usable_config_requires_wifi_valid_widgets_and_wake_window);
    RUN_TEST(test_v3_round_trip_uses_exact_ttc_payloads);
    RUN_TEST(test_scheduled_wake_rejects_invalid_windows_without_mutation);
    RUN_TEST(test_parse_failures_preserve_the_original_config);
    RUN_TEST(test_maximum_valid_config_stays_within_capacity_headroom);
    RUN_TEST(test_checked_serializer_rejects_over_limit_input);
    RUN_TEST(test_invalid_config_save_preserves_last_valid_json);
    RUN_TEST(test_sleep_resume_marker_is_persistent_and_idempotent);
    RUN_TEST(test_portal_get_redacts_password_and_round_trips_four_slots);
    RUN_TEST(test_portal_post_preserves_empty_password_and_replaces_non_empty_password);
    RUN_TEST(test_portal_post_rejects_malformed_oversized_and_wrong_type);
    RUN_TEST(test_portal_failed_save_does_not_mutate_live_config);
    RUN_TEST(test_portal_successful_four_widget_save_survives_reboot_load);
    RUN_TEST(test_portal_save_auth_rejects_cross_site_and_invalid_tokens);
    RUN_TEST(test_portal_ap_password_and_request_source_are_restricted);
    return UNITY_END();
}
