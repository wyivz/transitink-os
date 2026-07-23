#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/WidgetCore.h"

class TtcClient {
public:
    static constexpr const char* kTripUpdatesUrl = "https://bustime.ttc.ca/gtfsrt/trips";
    static constexpr std::size_t kMaxFeedBytes = 200 * 1024;
    static constexpr uint32_t kFeedTtlMs = 55000;

    bool fetchEtaRecords(const transitink::TtcWidgetConfig& config,
                         std::vector<transitink::TtcEtaRecord>& records,
                         String& error);

private:
    bool ensureFeed(String& error);
    bool httpGetFeed(String& error);

    std::unique_ptr<uint8_t[]> feed_;
    std::size_t feedSize_ = 0;
    uint32_t feedFetchedAtMs_ = 0;
    bool feedValid_ = false;
};
