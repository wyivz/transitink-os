import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ConfigStructureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "src/AppConfig.cpp").read_text(encoding="utf-8")
        cls.header = (ROOT / "include/AppConfig.h").read_text(encoding="utf-8")
        cls.store = (ROOT / "src/ConfigStore.cpp").read_text(encoding="utf-8")

    def test_version_3_writer_has_ttc_widget_slots_only(self):
        source = self.source
        self.assertIn('doc["schema_version"] = transitink::kConfigSchemaVersion', source)
        self.assertIn("constexpr uint16_t kConfigSchemaVersion = 3", (ROOT / "include/core/WidgetConfigCore.h").read_text(encoding="utf-8"))
        self.assertIn('doc.createNestedArray("widgets")', source)
        self.assertIn('item.createNestedObject("ttc")', source)
        self.assertIn('item["ttc"].as<JsonObjectConst>()', source)
        for removed in (
            'doc["routes"]',
            'doc["refresh_seconds"]',
            'doc["stop_name_tc"]',
            'weather_location_tc',
            'item.createNestedObject("bus")',
            'item.createNestedObject("gmb")',
            'item.createNestedObject("mtr")',
            'item.createNestedObject("journey_time")',
        ):
            self.assertNotIn(removed, source)

    def test_device_config_uses_versioned_widget_slots_without_weather_location(self):
        self.assertIn("uint16_t schemaVersion = transitink::kConfigSchemaVersion", self.header)
        self.assertIn("transitink::WidgetSlots widgets", self.header)
        self.assertNotIn("String weatherLocationTc", self.header)
        self.assertNotIn("String stopNameTc;", self.header)
        self.assertNotIn("uint16_t refreshSeconds", self.header)
        self.assertNotIn("std::vector<bus_eta::RouteSelection> routes", self.header)

    def test_config_store_keeps_persistent_config_and_sleep_marker(self):
        store = self.store
        self.assertIn('preferences_.begin("bus_eta", false)', store)
        self.assertIn('preferences_.putString("config"', store)
        self.assertIn('preferences_.getBool("sleep_resume", false)', store)
        self.assertIn('preferences_.putBool("sleep_resume", pending)', store)
        self.assertIn("if (sleepResumePending() == pending)", store)

    def test_json_document_capacity_is_bounded(self):
        portal_codec = (ROOT / "src/PortalConfigCodec.cpp").read_text(encoding="utf-8")
        self.assertEqual(
            self.source.count("DynamicJsonDocument doc(transitink::kConfigJsonCapacity)"), 2
        )
        self.assertEqual(
            portal_codec.count("DynamicJsonDocument doc(transitink::kConfigJsonCapacity)"), 2
        )
        self.assertNotIn("StaticJsonDocument<transitink::kConfigJsonCapacity>", self.source)
        self.assertNotIn("StaticJsonDocument<transitink::kConfigJsonCapacity>", portal_codec)
        self.assertIn("doc.overflowed()", self.source)

    def test_nested_save_path_keeps_device_config_candidates_off_loop_stack(self):
        portal_codec = (ROOT / "src/PortalConfigCodec.cpp").read_text(encoding="utf-8")
        self.assertIn("std::unique_ptr<DeviceConfig> parsed", portal_codec)
        self.assertNotIn("DeviceConfig parsed;", portal_codec)


class ThinConfigPortalStructureTests(unittest.TestCase):
    def test_portal_has_two_argument_constructor_and_no_catalog_service(self):
        header = (ROOT / "include/ConfigPortal.h").read_text(encoding="utf-8")
        source = (ROOT / "src/ConfigPortal.cpp").read_text(encoding="utf-8")
        self.assertIn("ConfigPortal(DeviceConfig& config, ConfigStore& store);", header)
        self.assertIn("ConfigPortal::ConfigPortal(DeviceConfig& config, ConfigStore& store)", source)
        self.assertNotIn("WidgetCatalogService", header + source)
        self.assertNotIn("CatalogAssetStore", header + source)
        self.assertNotIn("TRANSITINK_LEGACY_COMPAT", header + source)
        self.assertNotIn("KmbClient", header + source)
        self.assertNotIn("GmbClient", header + source)

    def test_portal_registers_ttc_catalog_assets_only(self):
        source = (ROOT / "src/ConfigPortal.cpp").read_text(encoding="utf-8")
        self.assertEqual(source.count('server_.on("/assets/catalog/current/ttc/index.json"'), 1)
        self.assertEqual(source.count('server_.on("/assets/catalog/current/ttc/stops-ttc.json"'), 1)
        self.assertIn('serveEmbeddedCatalog("ttc/index.json")', source)
        self.assertIn('serveEmbeddedCatalog("ttc/stops-ttc.json")', source)
        self.assertIn("kEmbeddedTtcCatalogAssets", source)
        self.assertIn('strcmp(assetPath, "ttc/index.json") == 0', source)
        self.assertIn('"public, max-age=31536000, immutable"', source)
        self.assertIn('server_.sendHeader("Content-Encoding", "gzip")', source)
        for endpoint in (
            "/api/catalog/bus/routes",
            "/api/catalog/gmb/routes",
            "/api/catalog/rail/lines",
            "/api/catalog/journey/locations",
            "/api/catalog/update",
            "/api/catalog/route-refresh",
            "/api/routes",
            "/api/stops",
        ):
            self.assertNotIn(endpoint, source)

    def test_ttc_catalog_assets_are_generated_and_manifested(self):
        header = (ROOT / "include/generated/TransitTtcCatalogAssets.h").read_text(encoding="utf-8")
        source = (ROOT / "src/generated/TransitTtcCatalogAssets.cpp").read_text(encoding="utf-8")
        manifest = (ROOT / "data/catalog/ttc/catalog-manifest.json").read_text(encoding="utf-8")
        self.assertIn("kEmbeddedTtcCatalogRevision", header)
        self.assertIn("kEmbeddedTtcCatalogAssets", header)
        self.assertIn('{"ttc/index.json"', source)
        self.assertIn('{"ttc/stops-ttc.json"', source)
        self.assertIn('"index.json.gz"', manifest)
        self.assertIn('"stops-ttc.json.gz"', manifest)
        self.assertNotIn("stops-kmb", manifest + source)
        self.assertNotIn("stops-gmb", manifest + source)
        self.assertNotIn("rail.json", manifest + source)

    def test_portal_delegates_page_codec_and_atomic_save(self):
        source = (ROOT / "src/ConfigPortal.cpp").read_text(encoding="utf-8")
        self.assertIn("kTransitInkPortalHtml", source)
        self.assertIn("encodePortalConfig", source)
        self.assertIn("savePortalConfig", source)
        self.assertNotIn("serializeDeviceConfigJson(config_)", source)
        self.assertNotIn("parseDeviceConfigJson(body, parsed", source)

    def test_portal_rejects_cross_site_save_before_reading_body(self):
        source = (ROOT / "src/ConfigPortal.cpp").read_text(encoding="utf-8")
        page = (ROOT / "src/TransitInkPortalPage.cpp").read_text(encoding="utf-8")
        codec = (ROOT / "src/PortalConfigCodec.cpp").read_text(encoding="utf-8")
        save = source.split("void ConfigPortal::saveConfig()", 1)[1].split(
            "void ConfigPortal::scanWifiNetworks()", 1
        )[0]
        self.assertNotIn("Access-Control-Allow-Origin", source)
        self.assertIn("esp_random", source)
        self.assertIn("csrfToken_", source)
        self.assertIn("isPortalSaveAuthorized", save)
        self.assertLess(save.index("isPortalSaveAuthorized"), save.index('server_.arg("plain")'))
        self.assertLess(save.index("isPortalSaveAuthorized"), save.index("savePortalConfig"))
        self.assertIn('doc["csrf_token"]', codec)
        self.assertIn("cfg.csrf_token", page)
        self.assertIn("const csrfHeader='X-TransitInk-CSRF'", page)
        self.assertIn("[csrfHeader]:csrfToken", page)

    def test_config_get_disables_http_and_browser_caches_on_success_and_error(self):
        source = (ROOT / "src/ConfigPortal.cpp").read_text(encoding="utf-8")
        page = (ROOT / "src/TransitInkPortalPage.cpp").read_text(encoding="utf-8")
        send_config = source.split("void ConfigPortal::sendConfig()", 1)[1].split(
            "void ConfigPortal::saveConfig()", 1
        )[0]
        self.assertIn('server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate")', send_config)
        self.assertIn('server_.sendHeader("Pragma", "no-cache")', send_config)
        self.assertLess(send_config.index("sendHeader"), send_config.index("encodePortalConfig"))
        self.assertIn("api('/api/config',{cache:'no-store'})", page)


if __name__ == "__main__":
    unittest.main()
