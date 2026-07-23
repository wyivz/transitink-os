import hashlib
import importlib.util
import re
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read_text(path):
    return (ROOT / path).read_text(encoding="utf-8")


def cpp_function_body(source, signature):
    signature_start = source.index(signature)
    while True:
        after_signature = signature_start + len(signature)
        opening_brace = source.find("{", after_signature)
        semicolon = source.find(";", after_signature)
        if opening_brace >= 0 and (semicolon < 0 or opening_brace < semicolon):
            break
        signature_start = source.index(signature, after_signature)
    depth = 0
    quote = None
    escaped = False
    line_comment = False
    block_comment = False
    index = opening_brace
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if line_comment:
            line_comment = char != "\n"
        elif block_comment:
            if char == "*" and next_char == "/":
                block_comment = False
                index += 1
        elif quote is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char == "/" and next_char == "/":
            line_comment = True
            index += 1
        elif char == "/" and next_char == "*":
            block_comment = True
            index += 1
        elif char in ('"', "'"):
            quote = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[opening_brace + 1:index]
        index += 1
    raise AssertionError(f"Unclosed function body: {signature}")


def assert_fragments_in_order(test_case, body, *fragments):
    position = -1
    for fragment in fragments:
        next_position = body.find(fragment, position + 1)
        test_case.assertGreater(next_position, position, fragment)
        position = next_position


class ProjectStructureTests(unittest.TestCase):
    def test_platformio_targets_zectrix_esp32s3(self):
        ini = read_text("platformio.ini")
        self.assertIn("[env:zectrix_note4]", ini)
        self.assertIn("pioarduino/platform-espressif32/releases/download/55.03.39", ini)
        self.assertRegex(ini, r"framework\s*=\s*arduino")
        self.assertRegex(ini, r"board_upload\.flash_size\s*=\s*16MB")
        self.assertRegex(ini, r"board_build\.arduino\.memory_type\s*=\s*qio_opi")
        self.assertIn("-DBOARD_HAS_PSRAM", ini)
        self.assertIn("bblanchon/ArduinoJson", ini)

    def test_board_config_uses_zectrix_reference_pins_and_button_roles(self):
        selector = read_text("include/hardware/BoardProfile.h")
        profile = read_text("include/hardware/boards/ZectrixNote4.h")
        host_test = read_text("test_host/test_board_profile.cpp")
        self.assertIn("TRANSITINK_BOARD_ZECTRIX_NOTE4", selector)
        self.assertIn("inline constexpr BoardProfile kBoardProfile", profile)
        for assertion in (
            "display.dcPin == 10",
            "display.chipSelectPin == 11",
            "display.clockPin == 12",
            "display.mosiPin == 13",
            "display.resetPin == 9",
            "display.busyPin == 8",
            "display.powerPin == 6",
            "buttons.homePin == 0",
            "buttons.upPin == 39",
            "buttons.downPin == 18",
            "buttons.factoryResetHoldMs == 5000",
            "buttons.configPin == 39",
            "buttons.configDebounceMs == 30",
            "buttons.configMaxClickMs == 1200",
            "battery.adcPin == 4",
            "battery.sensePowerPin == 17",
            "battery.chargeDetectPin == 2",
            "battery.chargeFullPin == 1",
        ):
            self.assertIn(assertion, host_test)

    def test_ttc_only_application_composition_and_timezone(self):
        main = read_text("src/main.cpp")
        assert_fragments_in_order(
            self,
            main,
            "TtcClient ttcClient;",
            "TtcProvider ttcProvider(ttcClient);",
            "WidgetProviderRouter widgetProviderRouter(ttcProvider);",
            "transitink::WidgetScheduler widgetScheduler(widgetProviderRouter);",
            "ConfigPortal configPortal(deviceConfig, configStore);",
        )
        self.assertIn('configTzTime("EST5EDT,M3.2.0,M11.1.0"', main)
        self.assertIn('setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);', main)
        self.assertNotIn("HKT-8", main)
        for removed in (
            "KmbClient",
            "CitybusClient",
            "GmbClient",
            "MtrClient",
            "LightRailClient",
            "JourneyTimeClient",
            "WeatherClient",
            "WidgetCatalogService",
            "BusProvider",
            "GmbProvider",
            "MtrProvider",
            "LightRailProvider",
            "JourneyTimeProvider",
            "weatherClient",
            "weatherLocationTc",
            "deviceConfig.routes",
            "EtaController",
        ):
            self.assertNotIn(removed, main)

    def test_ttc_client_provider_router_and_gtfs_filter_are_the_only_live_transit_path(self):
        ttc_header = read_text("include/TtcClient.h")
        ttc_client = read_text("src/TtcClient.cpp")
        provider_header = read_text("include/providers/TtcProvider.h")
        provider = read_text("src/providers/TtcProvider.cpp")
        router_header = read_text("include/providers/WidgetProviderRouter.h")
        router = read_text("src/providers/WidgetProviderRouter.cpp")
        filter_source = read_text("src/core/GtfsRealtimeTripFilter.cpp")

        self.assertIn('kTripUpdatesUrl = "https://bustime.ttc.ca/gtfsrt/trips"', ttc_header)
        self.assertIn("kMaxFeedBytes = 200 * 1024", ttc_header)
        self.assertIn("filterGtfsRtTripUpdates", ttc_client)
        self.assertIn("configureVerifiedTls(tls)", ttc_client)
        self.assertIn("normalizeTtcSnapshot", provider)
        self.assertIn("explicit TtcProvider(TtcClient& client)", provider_header)
        self.assertIn("explicit WidgetProviderRouter(TtcProvider& ttc);", router_header)
        self.assertIn("case transitink::WidgetType::TtcEta:", router)
        self.assertIn("return ttc_.fetch(slot, config, nowEpoch);", router)
        self.assertIn("parseStopTimeUpdate", filter_source)
        for removed in (
            "BusProvider",
            "GmbProvider",
            "MtrProvider",
            "LightRailProvider",
            "JourneyTimeProvider",
            "BusEta",
            "GmbEta",
            "MtrEta",
            "JourneyTime",
        ):
            self.assertNotIn(removed, router_header + router)

    def test_widget_config_core_is_closed_to_disabled_and_ttc_eta(self):
        header = read_text("include/core/WidgetConfigCore.h")
        source = read_text("src/core/WidgetConfigCore.cpp")
        self.assertIn("constexpr uint16_t kConfigSchemaVersion = 3", header)
        self.assertIn("enum class WidgetType : uint8_t { Disabled, TtcEta };", header)
        self.assertIn("struct TtcWidgetConfig", header)
        self.assertIn('return "ttc_eta"', source)
        self.assertIn('value == "ttc_eta"', source)
        self.assertIn("isRequiredIdValid(widget.ttc.routeId)", source)
        for removed in (
            "BusOperator",
            "RailMode",
            "BusWidgetConfig",
            "GmbWidgetConfig",
            "RailWidgetConfig",
            "JourneyTimeWidgetConfig",
            "bus_eta",
            "gmb_eta",
            "mtr_eta",
            "journey_time",
        ):
            self.assertNotIn(removed, header + source)

    def test_app_config_schema_v3_has_ttc_widgets_and_no_weather_location(self):
        header = read_text("include/AppConfig.h")
        source = read_text("src/AppConfig.cpp")
        codec = read_text("src/PortalConfigCodec.cpp")
        self.assertIn("uint16_t schemaVersion = transitink::kConfigSchemaVersion", header)
        self.assertIn("transitink::WidgetSlots widgets", header)
        self.assertIn('doc["schema_version"] = transitink::kConfigSchemaVersion', source)
        self.assertIn('item.createNestedObject("ttc")', source)
        self.assertIn('item["ttc"].as<JsonObjectConst>()', source)
        self.assertIn('doc["schema_version"] = transitink::kConfigSchemaVersion', codec)
        self.assertIn("wifi_password_set", codec)
        for removed in (
            "weatherLocationTc",
            "weather_location_tc",
            "migrateLegacyRoutes",
            'doc["routes"]',
            'doc["stop_name_tc"]',
            "busOperatorId",
            "railModeId",
            "journey_time",
            "gmb",
            "mtr",
        ):
            self.assertNotIn(removed, header + source + codec)

    def test_config_portal_serves_english_ttc_catalog_without_catalog_service(self):
        header = read_text("include/ConfigPortal.h")
        source = read_text("src/ConfigPortal.cpp")
        page = read_text("src/TransitInkPortalPage.cpp")
        self.assertIn("ConfigPortal(DeviceConfig& config, ConfigStore& store);", header)
        self.assertIn("ConfigPortal::ConfigPortal(DeviceConfig& config, ConfigStore& store)", source)
        self.assertIn('server_.on("/assets/catalog/current/ttc/index.json"', source)
        self.assertIn('server_.on("/assets/catalog/current/ttc/stops-ttc.json"', source)
        self.assertIn("kEmbeddedTtcCatalogAssets", source)
        self.assertIn('<html lang="en">', page)
        self.assertIn("TTC surface arrivals for your e-ink dashboard", page)
        self.assertIn("TTC arrivals", page)
        self.assertIn("collectConfig(){return{schema_version:3", page)
        for removed in (
            "WidgetCatalogService",
            "/api/catalog/bus/",
            "/api/catalog/gmb/",
            "/api/catalog/rail/",
            "/api/catalog/journey/",
            "/api/catalog/update",
            "/assets/catalog/current/index.json",
            "/assets/catalog/current/stops-kmb.json",
            "/assets/catalog/current/rail.json",
            "weather_location",
            "gmb_",
            "mtr_eta",
            "journey_time",
        ):
            self.assertNotIn(removed, header + source + page)

    def test_ttc_catalog_generator_and_ci_are_scoped_to_ttc_assets(self):
        generator = read_text("scripts/generate_ttc_route_catalog.py")
        ci = read_text(".github/workflows/ci.yml")
        release = read_text(".github/workflows/release.yml")
        self.assertIn('OUTPUT = ROOT / "data" / "catalog" / "ttc"', generator)
        self.assertIn("SurfaceGTFS.zip", generator)
        self.assertIn('ASSET_NAMES = ("index.json.gz", "stops-ttc.json.gz")', generator)
        self.assertIn("TransitTtcCatalogAssets.cpp", generator)
        self.assertIn("Verify embedded TTC catalog", ci)
        self.assertIn("scripts/generate_ttc_route_catalog.py --check", ci)
        self.assertIn("scripts/generate_ttc_route_catalog.py --check", release)
        self.assertNotIn("generate_transit_route_catalog.py --check", ci + release)

    def test_weather_footer_type_exists_but_no_remote_weather_fetch(self):
        main = read_text("src/main.cpp")
        weather = read_text("include/WeatherSnapshot.h")
        display_header = read_text("include/EInkDisplay.h")
        display = read_text("src/EInkDisplay.cpp")
        self.assertIn("WeatherSnapshot weatherSnapshot", main)
        self.assertIn("void refreshWeatherNow()", main)
        refresh_weather = cpp_function_body(main, "void refreshWeatherNow()")
        self.assertIn("weatherSnapshot = WeatherSnapshot{}", refresh_weather)
        self.assertNotIn("fetchCurrentWeather", refresh_weather)
        self.assertIn("Weather is disabled in the TTC-only build", weather)
        self.assertIn("weatherDisplayText", weather)
        self.assertIn("const WeatherSnapshot& weather", display_header)
        self.assertIn("drawWeatherFooter", display)
        self.assertNotIn("WeatherClient", main + weather + display_header + display)
        self.assertNotIn("data.weather.gov.hk", main + weather + display_header + display)

    def test_display_uses_four_equal_widget_lane_regions_and_english_copy(self):
        header = read_text("include/EInkDisplay.h")
        display = read_text("src/EInkDisplay.cpp")
        for api in (
            "void showDashboard(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);",
            "void refreshWidgetLane(uint8_t slot, const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);",
            "void refreshClock(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);",
            "void refreshWeatherFooter(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);",
            "void showSleep(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);",
        ):
            self.assertIn(api, header)
        self.assertIn("constexpr DisplayRegion kStatusRegion{0, 0, EINK_WIDTH, 42};", display)
        self.assertIn("constexpr DisplayRegion kLaneRegions[transitink::kWidgetSlotCount]", display)
        for rectangle in (
            "{0, 42, EINK_WIDTH, 57}",
            "{0, 99, EINK_WIDTH, 57}",
            "{0, 156, EINK_WIDTH, 57}",
            "{0, 213, EINK_WIDTH, 57}",
        ):
            self.assertIn(rectangle, display)
        self.assertIn("constexpr DisplayRegion kFooterRegion{0, 270, EINK_WIDTH, 30};", display)
        self.assertEqual(display.count("static_assert(regionFitsPanel("), 6)
        self.assertIn('"No widgets configured"', display)
        self.assertIn('"Press Volume Up for settings"', display)
        self.assertIn('"Settings · "', display)
        self.assertIn('"Network: "', display)
        self.assertIn('"Save and restart when finished"', display)
        self.assertIn('"Sun", "Mon", "Tue", "Wed"', display)
        self.assertIn('"%02d/%02d %s %02d:%02d"', display)
        self.assertIn("const std::size_t valueLimit = 2U", display)
        self.assertNotIn("JourneyTime", display)
        for hk_text in ("尚未設定小工具", "按 Volume Up 開啟設定頁", "設定 ", "網絡：", "儲存"):
            self.assertNotIn(hk_text, display)

    def test_display_refresh_paths_use_snapshot_state_and_partial_regions(self):
        display = read_text("src/EInkDisplay.cpp")
        dashboard = cpp_function_body(display, "void EInkDisplay::showDashboard(")
        assert_fragments_in_order(
            self,
            dashboard,
            "canvas.clear();",
            "drawClockAndStatusBar();",
            "shouldDrawLaneDivider(snapshots, slot)",
            "drawWidgetLane(slot, snapshots[slot], false, drawDivider);",
            "drawWeatherFooter(weather);",
            "fullRefresh();",
            "dashboardFrameActive = true;",
        )
        lane = cpp_function_body(display, "void EInkDisplay::refreshWidgetLane(")
        assert_fragments_in_order(
            self,
            lane,
            "if (slot >= transitink::kWidgetSlotCount)",
            "return;",
            "if (!dashboardFrameActive || !previousFrameValid)",
            "showDashboard(snapshots, weather);",
            "std::memcpy(frameBuffer, previousFrameBuffer, sizeof(frameBuffer));",
            "clearRegion(region);",
            "drawWidgetLane(slot, snapshots[slot], false, drawDivider);",
            "partialRefresh(region.x, region.y, region.w, region.h);",
        )
        widget_lane = cpp_function_body(display, "void drawWidgetLane(uint8_t slot,")
        self.assertIn("snapshot.type == transitink::WidgetType::Disabled", widget_lane)
        self.assertIn("snapshot.state == transitink::WidgetState::Empty", widget_lane)
        self.assertIn("snapshot.state == transitink::WidgetState::Error", widget_lane)
        self.assertIn("snapshot.providerMessage", widget_lane)
        self.assertIn("snapshot.freshness == transitink::Freshness::Stale", widget_lane)
        self.assertIn('"No data"', widget_lane)
        self.assertIn('"Stale"', widget_lane)
        self.assertIn('drawText(valueX, valueY, "-")', widget_lane)
        partial = cpp_function_body(display, "void refreshCanvasPartially(")
        self.assertIn("stats.changedBits == 0", partial)
        self.assertIn("stats.ratio() >= kForceFullPartialDiffRatio", partial)
        self.assertIn("panel.showPartialRegion", partial)

    def test_home_button_sleep_wakeup_and_volume_buttons_reset_or_open_config(self):
        main = read_text("src/main.cpp")
        support = read_text("src/hardware/BoardSupport.cpp")
        self.assertIn("transitink::hardware::configureHomeWakeup()", main)
        self.assertIn("transitink::hardware::homeButtonPressed()", main)
        self.assertIn("transitink::hardware::takeHomePress()", main)
        self.assertIn("transitink::hardware::clearPendingHomePress()", main)
        self.assertIn("startButtonMonitoring", support)
        self.assertIn("DebouncedButtonPressDetector", support)
        self.assertIn("gpio_wakeup_enable", support)
        self.assertIn("rtc_gpio_deinit", support)
        self.assertIn("ESP_SLEEP_WAKEUP_GPIO", main)
        self.assertNotIn("pinMode(", main)

        self.assertIn("DualButtonHoldDetector", support)
        self.assertIn("SingleButtonClickDetector", support)
        self.assertIn("factoryResetUpButtonPressed()", support)
        self.assertIn("factoryResetDownButtonPressed()", support)
        self.assertIn("buttonPressed(kBoardProfile.buttons.configPin)", support)
        self.assertIn("takeFactoryResetHold()", main)
        self.assertIn("configStore.clear()", main)
        self.assertIn("WiFi.disconnect(true, true)", main)
        self.assertIn("LittleFS.format()", main)
        self.assertIn('"Device reset\\nRelease volume keys to reboot"', main)
        self.assertIn("void showConfigAccessScreen()", main)
        self.assertIn("void returnToDashboard()", main)
        self.assertIn("void serviceConfigButton()", main)
        self.assertIn("Config button clicked", main)
        self.assertIn("configPortal.begin(useAccessPoint)", main)
        self.assertIn("configPortal.pageUrl()", main)
        self.assertIn("configPortal.isApMode()", main)
        self.assertIn('"Password: " + configPortal.apPassword()', main)
        self.assertIn('"Local settings\\n"', main)

    def test_sleep_power_settings_are_persisted_exposed_and_use_light_sleep(self):
        config = read_text("include/ProductConfig.h")
        for define in (
            "#define SLEEP_ENABLED_DEFAULT 1",
            "#define SLEEP_WAKE_DEFAULT_MINUTES 5",
            "#define SLEEP_MAINTENANCE_DEFAULT_HOURS 12",
            "#define SCHEDULED_WAKE_ENABLED_DEFAULT 0",
            "#define SCHEDULED_WAKE_START_DEFAULT_MINUTES 480",
            "#define SCHEDULED_WAKE_END_DEFAULT_MINUTES 540",
        ):
            self.assertIn(define, config)
        app_config = read_text("include/AppConfig.h")
        for field in (
            "bool sleepEnabled = SLEEP_ENABLED_DEFAULT",
            "uint16_t wakeDurationMinutes = SLEEP_WAKE_DEFAULT_MINUTES",
            "uint16_t sleepMaintenanceHours = SLEEP_MAINTENANCE_DEFAULT_HOURS",
            "bool scheduledWakeEnabled = SCHEDULED_WAKE_ENABLED_DEFAULT",
            "uint16_t scheduledWakeStartMinutes = SCHEDULED_WAKE_START_DEFAULT_MINUTES",
            "uint16_t scheduledWakeEndMinutes = SCHEDULED_WAKE_END_DEFAULT_MINUTES",
        ):
            self.assertIn(field, app_config)

        page = read_text("src/TransitInkPortalPage.cpp")
        for fragment in (
            'id="sleep_enabled"',
            'id="wake_duration_minutes"',
            'id="sleep_maintenance_hours"',
            'id="scheduled_wake_enabled"',
            'id="scheduled_wake_start"',
            'id="scheduled_wake_end"',
            "Enable sleep",
            "Wake duration (minutes)",
            "Maintenance wake (hours)",
            "Daily wake window",
        ):
            self.assertIn(fragment, page)

        main = read_text("src/main.cpp")
        self.assertIn("#include <esp_sleep.h>", main)
        self.assertIn("#include <esp_wifi.h>", main)
        self.assertIn("void enterSleepMode(const char* reason)", main)
        self.assertIn("void configureLightSleepWakeup()", main)
        self.assertIn("void returnFromLightSleep()", main)
        self.assertIn("void performLightSleepMaintenance()", main)
        self.assertIn("RTC_NOINIT_ATTR uint32_t sleepResumeMarker", main)
        self.assertIn("bus_eta::SleepSettings sleepSettingsFromConfig", main)
        self.assertIn("const bool sleepBlocked = configAccessMode || chargeSnapshot.powerPresent", main)
        self.assertIn("scheduledWakeSession", main)
        self.assertIn("scheduledWakeWindowActiveNow()", main)
        self.assertIn("configPortal.stop();", main)
        self.assertIn("WiFi.disconnect(true, true)", main)
        self.assertIn("esp_wifi_stop()", main)
        self.assertIn("WiFi.mode(WIFI_OFF)", main)
        self.assertIn("esp_light_sleep_start()", main)
        self.assertNotIn("esp_deep_sleep_start()", main)
        self.assertNotIn("esp_sleep_enable_ext0_wakeup", main)
        self.assertIn("sleepMaintenanceIntervalUs", main)
        self.assertIn("secondsUntilScheduledWakeStart", main)
        self.assertIn("wakeCause == ESP_SLEEP_WAKEUP_TIMER", main)
        self.assertIn("wakeCause == ESP_SLEEP_WAKEUP_GPIO", main)

        configure_wakeup = cpp_function_body(main, "void configureLightSleepWakeup()")
        assert_fragments_in_order(
            self,
            configure_wakeup,
            "esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);",
            "transitink::hardware::configureHomeWakeup();",
            "esp_sleep_enable_timer_wakeup(timerUs);",
        )
        enter_sleep = cpp_function_body(main, "void enterSleepMode(const char* reason)")
        assert_fragments_in_order(
            self,
            enter_sleep,
            "armSleepResumeMarker();",
            "esp_light_sleep_start();",
            "clearSleepResumeMarker();",
        )
        maintenance = cpp_function_body(main, "void performLightSleepMaintenance()")
        self.assertIn("syncTimeAndWeatherBeforeDashboard(false);", maintenance)
        self.assertIn("stopNetworkForSleep();", maintenance)
        self.assertIn("einkDisplay.refreshSleepStatusAndWeather(currentDisplaySnapshots(), weatherSnapshot);", maintenance)
        self.assertNotIn("widgetScheduler", maintenance)

    def test_home_wake_refresh_keeps_dashboard_until_background_refresh_completes(self):
        main = read_text("src/main.cpp")
        start = cpp_function_body(main, "void startHomeWakeRefresh()")
        assert_fragments_in_order(
            self,
            start,
            "widgetScheduler.forceAllDue(wakeStartedAtMs);",
            "einkDisplay.showDashboard(homeWakeLoadingSnapshots(), weatherSnapshot);",
            "WiFi.begin(deviceConfig.wifiSsid.c_str(), deviceConfig.wifiPassword.c_str());",
        )
        self.assertNotIn("connectWifi", start)
        loading = cpp_function_body(main, "transitink::WidgetSnapshotSet homeWakeLoadingSnapshots()")
        assert_fragments_in_order(
            self,
            loading,
            "snapshot.values = {};",
            "snapshot.valueCount = 0;",
            "snapshot.state = transitink::WidgetState::Empty;",
            'snapshot.providerMessage = "Updating...";',
            "snapshot.fetchedAtEpoch = 0;",
            "snapshot.freshness = transitink::Freshness::Fresh;",
        )
        background = cpp_function_body(main, "void serviceHomeWakeRefresh()")
        assert_fragments_in_order(
            self,
            background,
            "HomeWakeRefreshPhase::ConnectingWifi",
            "HomeWakeRefreshPhase::WaitingForTime",
            "refreshClockNow();",
            "HomeWakeRefreshPhase::Widgets",
            "homeWakeWidgetAttempts < static_cast<uint8_t>(transitink::kWidgetSlotCount)",
            "serviceOneWidgetIfDue();",
            "HomeWakeRefreshPhase::Weather",
            "finishHomeWakeRefresh();",
        )

    def test_glyph_font_has_pinned_ofl_provenance(self):
        script = ROOT / "scripts" / "generate_hk_glyph_font.py"
        spec = importlib.util.spec_from_file_location("generate_hk_glyph_font", script)
        generator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(generator)

        font_path = ROOT / "third_party/fonts/noto-sans-cjk-hk/NotoSansCJKhk-Regular.otf"
        licence_path = ROOT / "third_party/fonts/noto-sans-cjk-hk/OFL.txt"
        upstream_notice_path = ROOT / "third_party/fonts/noto-sans-cjk-hk/UPSTREAM-NOTICE.md"
        source_path = ROOT / "third_party/fonts/noto-sans-cjk-hk/SOURCE.md"
        notices_path = ROOT / "THIRD_PARTY_NOTICES.md"

        self.assertTrue(font_path.is_file())
        self.assertTrue(licence_path.is_file())
        self.assertTrue(upstream_notice_path.is_file())
        self.assertTrue(source_path.is_file())
        self.assertTrue(notices_path.is_file())
        self.assertEqual(hashlib.sha256(font_path.read_bytes()).hexdigest(), generator.DEFAULT_FONT_SHA256)
        self.assertIn("SIL OPEN FONT LICENSE Version 1.1", licence_path.read_text(encoding="utf-8"))
        self.assertRegex(upstream_notice_path.read_text(encoding="utf-8"), r"copyright\s+is held by Adobe")
        self.assertIn(generator.DEFAULT_FONT_SHA256, source_path.read_text(encoding="utf-8"))
        self.assertIn("src/generated/HkGlyphFontData.cpp", notices_path.read_text(encoding="utf-8"))

    def test_glyph_generator_rejects_non_bmp_codepoints(self):
        script = ROOT / "scripts" / "generate_hk_glyph_font.py"
        spec = importlib.util.spec_from_file_location("generate_hk_glyph_font", script)
        generator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(generator)
        with tempfile.TemporaryDirectory() as directory:
            seed = Path(directory) / "seed.txt"
            seed.write_text("source=test\n---\nA🚊\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "U\\+1F68A"):
                generator.collect_chars([], seed)


if __name__ == "__main__":
    unittest.main()
