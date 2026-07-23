#include "TtcClient.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ctime>
#include <cstring>

#include "TransitTlsTrust.h"
#include "core/GtfsRealtimeTripFilter.h"

bool TtcClient::fetchEtaRecords(const transitink::TtcWidgetConfig& config,
                                std::vector<transitink::TtcEtaRecord>& records,
                                String& error) {
    records.clear();
    if (config.stopId.empty() || config.routeId.empty()) {
        error = "TTC stop settings incomplete";
        return false;
    }
    if (!ensureFeed(error)) {
        return false;
    }

    std::vector<transitink::GtfsRtArrival> arrivals;
    if (!transitink::filterGtfsRtTripUpdates(feed_.get(), feedSize_, config.stopId,
                                             config.routeId, static_cast<int64_t>(time(nullptr)),
                                             4, arrivals)) {
        error = "Failed to parse TTC trip updates";
        return false;
    }
    records.reserve(arrivals.size());
    for (const auto& arrival : arrivals) {
        transitink::TtcEtaRecord record;
        record.routeId = arrival.routeId;
        record.eventEpoch = arrival.eventEpoch;
        records.push_back(std::move(record));
    }
    return true;
}

bool TtcClient::ensureFeed(String& error) {
    const uint32_t nowMs = millis();
    if (feedValid_ && feed_ && feedSize_ > 0 &&
        static_cast<int32_t>(nowMs - feedFetchedAtMs_) < static_cast<int32_t>(kFeedTtlMs)) {
        return true;
    }
    return httpGetFeed(error);
}

bool TtcClient::httpGetFeed(String& error) {
    WiFiClientSecure tls;
    transitink::configureVerifiedTls(tls);
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(12000);
    if (!http.begin(tls, kTripUpdatesUrl)) {
        error = "Unable to start TTC HTTPS request";
        return false;
    }

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        error = String("TTC HTTP ") + String(status);
        http.end();
        return false;
    }

    const int contentLength = http.getSize();
    if (contentLength > static_cast<int>(kMaxFeedBytes)) {
        error = "TTC feed too large";
        http.end();
        return false;
    }

    auto* stream = http.getStreamPtr();
    if (stream == nullptr) {
        error = "TTC response stream missing";
        http.end();
        return false;
    }

    auto buffer = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[kMaxFeedBytes]);
    if (!buffer) {
        error = "Out of memory for TTC feed";
        http.end();
        return false;
    }

    std::size_t copied = 0;
    uint8_t chunk[1024];
    const unsigned long deadline = millis() + 15000;
    while (http.connected() &&
           (contentLength < 0 || static_cast<int>(copied) < contentLength) &&
           static_cast<int32_t>(millis() - deadline) < 0) {
        const size_t available = stream->available();
        if (available == 0) {
            delay(10);
            continue;
        }
        size_t toRead = available;
        if (toRead > sizeof(chunk)) toRead = sizeof(chunk);
        if (copied + toRead > kMaxFeedBytes) {
            error = "TTC feed exceeded buffer";
            http.end();
            return false;
        }
        const int readLen = stream->readBytes(chunk, toRead);
        if (readLen <= 0) break;
        std::memcpy(buffer.get() + copied, chunk, static_cast<size_t>(readLen));
        copied += static_cast<size_t>(readLen);
        if (contentLength >= 0 && static_cast<int>(copied) >= contentLength) break;
    }
    http.end();

    if (copied == 0) {
        error = "Empty TTC feed";
        return false;
    }

    feed_ = std::move(buffer);
    feedSize_ = copied;
    feedFetchedAtMs_ = millis();
    feedValid_ = true;
    return true;
}
