#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include "AppConfig.h"
#include "BatteryMonitor.h"
#include "ConfigStore.h"

class ConfigPortal {
public:
    ConfigPortal(DeviceConfig& config, ConfigStore& store);

    void begin(bool forceAp);
    void stop();
    void loop();
    bool isApMode() const { return apMode_; }
    bool isStarted() const { return serverStarted_; }
    const String& apPassword() const { return apPassword_; }
    String pageUrl() const;

private:
    void startAp();
    void registerRoutes();
    bool authorizePortalRequest(bool validateOrigin);
    void sendIndex();
    void sendConfig();
    void saveConfig();
    void scanWifiNetworks();
    void serveEmbeddedCatalog(const char* assetPath);
    void sendText(int code, const String& contentType, const String& body);
    IPAddress portalIp() const;

    DeviceConfig& config_;
    ConfigStore& store_;
    BatteryMonitor batteryMonitor_;
    WebServer server_;
    DNSServer dns_;
    bool apMode_ = false;
    bool routesRegistered_ = false;
    bool serverStarted_ = false;
    String csrfToken_;
    String apPassword_;
    String accessToken_;
};
