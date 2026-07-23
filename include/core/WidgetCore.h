#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/WidgetConfigCore.h"

namespace transitink {

enum class WidgetState : uint8_t { Ready, Empty, Error };
enum class Freshness : uint8_t { Fresh, Stale };
enum class ProviderOutcome : uint8_t { Success, Empty, InvalidConfig, ClockUnsynced, Failure };

struct WidgetValue {
    std::string text, context;
    int64_t eventEpoch = 0;
};

struct WidgetSnapshot {
    uint8_t slot = 0;
    WidgetType type = WidgetType::Disabled;
    std::string title, subtitle;
    std::array<WidgetValue, 2> values{};
    std::size_t valueCount = 0;
    WidgetState state = WidgetState::Empty;
    std::string providerMessage;
    int64_t fetchedAtEpoch = 0, dataAtEpoch = 0;
    Freshness freshness = Freshness::Fresh;
    uint8_t consecutiveFailures = 0;
};

using WidgetSnapshotSet = std::array<WidgetSnapshot, kWidgetSlotCount>;

WidgetSnapshot configuredWidgetSnapshot(uint8_t slot, const WidgetConfig& config);

struct ProviderResult {
    ProviderOutcome outcome;
    WidgetSnapshot snapshot;
};

struct TtcEtaRecord {
    std::string routeId;
    int64_t eventEpoch = 0;
};

uint32_t refreshIntervalMs(WidgetType type);
uint32_t staleWindowSeconds(WidgetType type);
bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs);
void removeExpiredValues(WidgetSnapshot& snapshot, int64_t nowEpoch);

ProviderResult normalizeTtcSnapshot(uint8_t slot,
                                    const WidgetConfig& config,
                                    const std::vector<TtcEtaRecord>& records,
                                    int64_t nowEpoch);

}  // namespace transitink
