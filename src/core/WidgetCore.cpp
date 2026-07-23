#include "core/WidgetCore.h"

#include <algorithm>
#include <initializer_list>
#include <utility>
#include <vector>

namespace transitink {
namespace {

constexpr const char* kInvalidConfigMessage = "Incomplete settings";
constexpr const char* kEmptyMessage = "No upcoming arrivals";
constexpr const char* kClockUnsyncedMessage = "Clock not synced";

std::string joinNonEmpty(std::initializer_list<std::string> parts) {
    std::string result;
    for (const auto& part : parts) {
        if (part.empty()) continue;
        if (!result.empty()) result += " · ";
        result += part;
    }
    return result;
}

std::string countdownText(int64_t eventEpoch, int64_t nowEpoch) {
    const int64_t seconds = eventEpoch - nowEpoch;
    const int64_t minutes = seconds / 60 + (seconds % 60 == 0 ? 0 : 1);
    if (minutes <= 1) return "1 min";
    return std::to_string(minutes) + " min";
}

WidgetSnapshot baseSnapshot(uint8_t slot,
                            const WidgetConfig& config,
                            int64_t fetchedAtEpoch,
                            int64_t dataAtEpoch) {
    WidgetSnapshot snapshot = configuredWidgetSnapshot(slot, config);
    snapshot.fetchedAtEpoch = fetchedAtEpoch;
    snapshot.dataAtEpoch = dataAtEpoch;
    return snapshot;
}

ProviderResult errorResult(uint8_t slot,
                           const WidgetConfig& config,
                           int64_t nowEpoch,
                           ProviderOutcome outcome,
                           const char* message) {
    auto snapshot = baseSnapshot(slot, config, nowEpoch, 0);
    snapshot.state = WidgetState::Error;
    snapshot.providerMessage = message;
    return {outcome, std::move(snapshot)};
}

ProviderResult emptyResult(WidgetSnapshot snapshot,
                           const std::string& message = kEmptyMessage) {
    snapshot.state = WidgetState::Empty;
    snapshot.providerMessage = message;
    return {ProviderOutcome::Empty, std::move(snapshot)};
}

bool sameValue(const WidgetValue& left, const WidgetValue& right) {
    return left.eventEpoch == right.eventEpoch && left.text == right.text &&
           left.context == right.context;
}

void appendUnique(std::vector<WidgetValue>& values, WidgetValue value) {
    const bool exists = std::any_of(values.begin(), values.end(), [&](const WidgetValue& other) {
        return sameValue(value, other);
    });
    if (!exists) values.push_back(std::move(value));
}

void storeFirstTwo(WidgetSnapshot& snapshot, std::vector<WidgetValue>& values) {
    std::stable_sort(values.begin(), values.end(), [](const WidgetValue& left,
                                                       const WidgetValue& right) {
        return left.eventEpoch < right.eventEpoch;
    });
    snapshot.valueCount = std::min(values.size(), snapshot.values.size());
    for (std::size_t index = 0; index < snapshot.valueCount; ++index) {
        snapshot.values[index] = std::move(values[index]);
    }
}

}  // namespace

WidgetSnapshot configuredWidgetSnapshot(uint8_t slot, const WidgetConfig& config) {
    WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = config.type;
    switch (config.type) {
        case WidgetType::TtcEta:
            snapshot.title = joinNonEmpty({config.ttc.routeLabel, config.ttc.destinationLabel});
            snapshot.subtitle = config.ttc.stopLabel;
            break;
        case WidgetType::Disabled:
            break;
    }
    return snapshot;
}

uint32_t refreshIntervalMs(WidgetType type) {
    switch (type) {
        case WidgetType::TtcEta:
            return 60000;
        case WidgetType::Disabled:
            return 0;
    }
    return 0;
}

uint32_t staleWindowSeconds(WidgetType type) {
    switch (type) {
        case WidgetType::TtcEta:
            return 180;
        case WidgetType::Disabled:
            return 0;
    }
    return 0;
}

bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

void removeExpiredValues(WidgetSnapshot& snapshot, int64_t nowEpoch) {
    std::size_t writeIndex = 0;
    for (std::size_t readIndex = 0; readIndex < snapshot.valueCount; ++readIndex) {
        const auto& value = snapshot.values[readIndex];
        if (value.eventEpoch > 0 && value.eventEpoch <= nowEpoch) continue;
        if (writeIndex != readIndex) snapshot.values[writeIndex] = value;
        ++writeIndex;
    }
    for (std::size_t index = writeIndex; index < snapshot.values.size(); ++index) {
        snapshot.values[index] = {};
    }
    snapshot.valueCount = writeIndex;
}

ProviderResult normalizeTtcSnapshot(uint8_t slot,
                                    const WidgetConfig& config,
                                    const std::vector<TtcEtaRecord>& records,
                                    int64_t nowEpoch) {
    if (config.type != WidgetType::TtcEta || !isWidgetConfigValid(config)) {
        return errorResult(slot, config, nowEpoch, ProviderOutcome::InvalidConfig,
                           kInvalidConfigMessage);
    }
    if (nowEpoch <= 0) {
        return errorResult(slot, config, nowEpoch, ProviderOutcome::ClockUnsynced,
                           kClockUnsyncedMessage);
    }

    auto snapshot = baseSnapshot(slot, config, nowEpoch, nowEpoch);
    std::vector<WidgetValue> values;
    values.reserve(records.size());
    for (const auto& record : records) {
        if (record.eventEpoch <= nowEpoch) continue;
        if (!record.routeId.empty() && record.routeId != config.ttc.routeId) continue;
        appendUnique(values,
                     {countdownText(record.eventEpoch, nowEpoch), config.ttc.destinationLabel,
                      record.eventEpoch});
    }
    storeFirstTwo(snapshot, values);
    if (snapshot.valueCount == 0) return emptyResult(std::move(snapshot));
    snapshot.state = WidgetState::Ready;
    return {ProviderOutcome::Success, std::move(snapshot)};
}

}  // namespace transitink
