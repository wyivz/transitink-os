#include "ConfigPortal.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <cstring>

#include "ProductConfig.h"
#include "PortalConfigCodec.h"
#include "TransitInkPortalPage.h"
#include "core/PortalRequestAuth.h"
#include "generated/TransitTtcCatalogAssets.h"

namespace {

String chipSuffix() {
    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%06X",
             static_cast<unsigned int>(ESP.getEfuseMac() & 0xFFFFFF));
    return String(suffix);
}

constexpr byte kDnsPort = 53;
const char* kRequestHeaders[] = {
    "Content-Type", "X-TransitInk-CSRF", "X-TransitInk-Access", "Origin"};

String generateCsrfToken() {
    char token[33];
    snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    return String(token);
}

}  // namespace

ConfigPortal::ConfigPortal(DeviceConfig& config, ConfigStore& store)
    : config_(config), store_(store), server_(80) {}

void ConfigPortal::begin(bool forceAp) {
    batteryMonitor_.begin();
    const bool useAp = forceAp || WiFi.status() != WL_CONNECTED;
    if (useAp && !apMode_) {
        startAp();
    } else if (!useAp) {
        apMode_ = false;
    }
    if (!routesRegistered_) {
        registerRoutes();
        routesRegistered_ = true;
    }
    if (!serverStarted_) {
        csrfToken_ = generateCsrfToken();
        accessToken_ = transitink::generatePortalApPassword(
                           esp_random(), esp_random(), esp_random()).c_str();
        server_.begin();
        serverStarted_ = true;
    }
}

void ConfigPortal::stop() {
    if (serverStarted_) {
        server_.stop();
        serverStarted_ = false;
    }
    if (apMode_) {
        dns_.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        apMode_ = false;
    }
    apPassword_ = "";
    csrfToken_ = "";
    accessToken_ = "";
}

void ConfigPortal::registerRoutes() {
    server_.collectHeaders(kRequestHeaders, sizeof(kRequestHeaders) / sizeof(kRequestHeaders[0]));
    server_.on("/", HTTP_GET, [this]() {
        if (authorizePortalRequest(false)) sendIndex();
    });
    server_.on("/api/config", HTTP_GET, [this]() {
        if (authorizePortalRequest(false)) sendConfig();
    });
    server_.on("/api/config", HTTP_POST, [this]() {
        if (authorizePortalRequest(true)) saveConfig();
    });
    server_.on("/api/wifi/scan", HTTP_GET, [this]() {
        if (authorizePortalRequest(false)) scanWifiNetworks();
    });
    server_.on("/assets/catalog/current/ttc/index.json", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) serveEmbeddedCatalog("ttc/index.json"); });
    server_.on("/assets/catalog/current/ttc/stops-ttc.json", HTTP_GET,
               [this]() {
                   if (authorizePortalRequest(false)) serveEmbeddedCatalog("ttc/stops-ttc.json");
               });
    server_.onNotFound([this]() {
        if (!authorizePortalRequest(false)) return;
        if (apMode_) {
            sendIndex();
            return;
        }
        sendText(404, "text/plain; charset=utf-8", "Not found");
    });
}

bool ConfigPortal::authorizePortalRequest(bool validateOrigin) {
    const IPAddress expectedIp = portalIp();
    const String allowedHost = expectedIp.toString();
    const bool accessAllowed = apMode_ || transitink::isPortalAccessTokenAuthorized(
                                                server_.header("X-TransitInk-Access").c_str(),
                                                accessToken_.c_str());
    if (serverStarted_ && server_.client().localIP() == expectedIp && accessAllowed &&
        transitink::isPortalRequestSourceAllowed(
                       server_.hostHeader().c_str(),
                       server_.header("Origin").c_str(),
                       allowedHost.c_str(), validateOrigin)) {
        return true;
    }
    server_.sendHeader("Cache-Control", "no-store");
    sendText(403, "text/plain; charset=utf-8", "Invalid settings request origin");
    return false;
}

void ConfigPortal::sendIndex() {
    const IPAddress expectedIp = portalIp();
    const String allowedHost = expectedIp.toString();
    if (!serverStarted_ || server_.client().localIP() != expectedIp) {
        server_.sendHeader("Cache-Control", "no-store");
        sendText(403, "text/plain; charset=utf-8", "Settings page is only available on device Wi-Fi");
        return;
    }
    if (!transitink::isPortalRequestSourceAllowed(
            server_.hostHeader().c_str(), "", allowedHost.c_str(), false)) {
        if (apMode_) {
            server_.sendHeader("Location", pageUrl());
            server_.sendHeader("Cache-Control", "no-store");
            sendText(302, "text/plain; charset=utf-8", "Redirecting to settings");
        } else {
            server_.sendHeader("Cache-Control", "no-store");
            sendText(403, "text/plain; charset=utf-8", "Invalid settings URL");
        }
        return;
    }
    if (!apMode_) {
        String submittedToken = server_.uri();
        if (submittedToken.startsWith("/")) submittedToken.remove(0, 1);
        if (!transitink::isPortalAccessTokenAuthorized(
                submittedToken.c_str(), accessToken_.c_str())) {
            server_.sendHeader("Cache-Control", "no-store");
            sendText(403, "text/plain; charset=utf-8", "Open settings with the QR code on the device screen");
            return;
        }
    }
    server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server_.sendHeader("Pragma", "no-cache");
    server_.sendHeader(
        "Content-Security-Policy",
        "default-src 'self'; script-src 'unsafe-inline'; "
        "style-src 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; "
        "object-src 'none'; base-uri 'none'; form-action 'self'; "
        "frame-ancestors 'none'");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    server_.sendHeader("X-Frame-Options", "DENY");
    server_.sendHeader("Referrer-Policy", "no-referrer");
    server_.send_P(200, "text/html; charset=utf-8", kTransitInkPortalHtml);
}

void ConfigPortal::sendConfig() {
    server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server_.sendHeader("Pragma", "no-cache");
    const bus_eta::BatterySnapshot battery = batteryMonitor_.read();
    String json;
    String error;
    if (!encodePortalConfig(config_, battery, FIRMWARE_VERSION, csrfToken_, json, error)) {
        sendText(500, "text/plain; charset=utf-8", error);
        return;
    }
    sendText(200, "application/json; charset=utf-8", json);
}

void ConfigPortal::saveConfig() {
    if (!transitink::isPortalSaveAuthorized(server_.header("Content-Type").c_str(),
                                            server_.header("X-TransitInk-CSRF").c_str(),
                                            csrfToken_.c_str())) {
        sendText(403, "text/plain; charset=utf-8", "Save request authorization failed");
        return;
    }
    String error;
    if (!savePortalConfig(server_.arg("plain"), config_, store_, error)) {
        sendText(400, "text/plain; charset=utf-8", error);
        return;
    }
    sendText(200, "text/plain; charset=utf-8", "Settings saved. Device is restarting.");
    delay(400);
    ESP.restart();
}

void ConfigPortal::scanWifiNetworks() {
    const int count = WiFi.scanNetworks(false, true);
    if (count < 0) {
        WiFi.scanDelete();
        if (apMode_) WiFi.enableSTA(false);
        sendText(500, "text/plain; charset=utf-8", "Wi-Fi scan failed");
        return;
    }
    DynamicJsonDocument doc(4096);
    JsonArray data = doc.createNestedArray("data");
    for (int index = 0; index < count && index < 24; ++index) {
        const String ssid = WiFi.SSID(index);
        if (ssid.isEmpty()) {
            continue;
        }
        bool seen = false;
        for (JsonObjectConst item : data) {
            if (ssid == (item["id"] | "")) {
                seen = true;
                break;
            }
        }
        if (seen) {
            continue;
        }
        JsonObject item = data.createNestedObject();
        item["id"] = ssid;
        item["label"] = ssid;
        item["rssi"] = WiFi.RSSI(index);
        item["secure"] = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    if (apMode_) WiFi.enableSTA(false);
    String json;
    serializeJson(doc, json);
    sendText(200, "application/json; charset=utf-8", json);
}

void ConfigPortal::serveEmbeddedCatalog(const char* assetPath) {
    for (std::size_t index = 0; index < transitink::kEmbeddedTtcCatalogAssetCount; ++index) {
        const auto& asset = transitink::kEmbeddedTtcCatalogAssets[index];
        if (strcmp(asset.path, assetPath) != 0) {
            continue;
        }
        server_.sendHeader("Cache-Control",
                           strcmp(assetPath, "ttc/index.json") == 0
                               ? "no-cache"
                               : "public, max-age=31536000, immutable");
        server_.sendHeader("Content-Encoding", "gzip");
        server_.send_P(200, "application/json; charset=utf-8",
                       reinterpret_cast<PGM_P>(asset.data), asset.size);
        return;
    }
    sendText(404, "text/plain; charset=utf-8", "Catalog asset not found");
}

void ConfigPortal::sendText(int code,
                            const String& contentType,
                            const String& body) {
    server_.send(code, contentType, body);
}

IPAddress ConfigPortal::portalIp() const {
    return apMode_ ? WiFi.softAPIP() : WiFi.localIP();
}

String ConfigPortal::pageUrl() const {
    const String base = "http://" + portalIp().toString() + "/";
    return apMode_ ? base : base + accessToken_;
}

void ConfigPortal::startAp() {
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP);
    const String ssid = String(CONFIG_AP_PREFIX) + "-" + chipSuffix();
    apPassword_ = transitink::generatePortalApPassword(
                      esp_random(), esp_random(), esp_random()).c_str();
    WiFi.softAP(ssid.c_str(), apPassword_.c_str());
    dns_.start(kDnsPort, "*", WiFi.softAPIP());
    apMode_ = true;
}

void ConfigPortal::loop() {
    if (!serverStarted_) {
        return;
    }
    if (apMode_) {
        dns_.processNextRequest();
    }
    server_.handleClient();
}

