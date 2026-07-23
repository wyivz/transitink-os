#include "core/WidgetCore.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

transitink::WidgetConfig ttcConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::TtcEta;
    config.ttc.routeId = "506";
    config.ttc.directionId = "0";
    config.ttc.stopId = "8431";
    config.ttc.routeLabel = "506";
    config.ttc.stopLabel = "College St at University Ave";
    config.ttc.destinationLabel = "Eastbound to Main Street Station";
    return config;
}

}  // namespace

int main() {
    using namespace transitink;

    const int64_t now = 2'000'000'000;
    const TtcEtaRecord ttcContract{"506", now + 60};
    assert(ttcContract.routeId == "506");
    assert(ttcContract.eventEpoch == now + 60);

    assert(refreshIntervalMs(WidgetType::Disabled) == 0);
    assert(refreshIntervalMs(WidgetType::TtcEta) == 60000);
    assert(refreshIntervalMs(static_cast<WidgetType>(255)) == 0);
    assert(staleWindowSeconds(WidgetType::Disabled) == 0);
    assert(staleWindowSeconds(WidgetType::TtcEta) == 180);
    assert(staleWindowSeconds(static_cast<WidgetType>(255)) == 0);

    assert(deadlineReached(100, 100));
    assert(deadlineReached(101, 100));
    assert(!deadlineReached(99, 100));
    assert(deadlineReached(0x00000010U, 0xfffffff0U));
    assert(!deadlineReached(0xfffffff0U, 0x00000010U));

    WidgetSnapshot expiring;
    expiring.valueCount = 2;
    expiring.values[0] = {"expired", "", now};
    expiring.values[1] = {"fresh", "", now + 1};
    removeExpiredValues(expiring, now);
    assert(expiring.valueCount == 1);
    assert(expiring.values[0].text == "fresh");

    WidgetSnapshot duration;
    duration.valueCount = 1;
    duration.values[0] = {"24 min", "", 0};
    removeExpiredValues(duration, now);
    assert(duration.valueCount == 1);

    const WidgetConfig config = ttcConfig();
    const auto placeholder = configuredWidgetSnapshot(2, config);
    assert(placeholder.slot == 2);
    assert(placeholder.type == WidgetType::TtcEta);
    assert(placeholder.title == "506 · Eastbound to Main Street Station");
    assert(placeholder.subtitle == "College St at University Ave");
    assert(placeholder.valueCount == 0);

    std::vector<TtcEtaRecord> records = {
        {"506", now + 300},
        {"506", now + 300},
        {"506", now - 30},
        {"504", now + 120},
        {"506", now + 600},
        {"", now + 900},
    };
    const auto normalized = normalizeTtcSnapshot(0, config, records, now);
    assert(normalized.outcome == ProviderOutcome::Success);
    assert(normalized.snapshot.state == WidgetState::Ready);
    assert(normalized.snapshot.slot == 0);
    assert(normalized.snapshot.type == WidgetType::TtcEta);
    assert(normalized.snapshot.title == "506 · Eastbound to Main Street Station");
    assert(normalized.snapshot.subtitle == "College St at University Ave");
    assert(normalized.snapshot.valueCount == 2);
    assert(normalized.snapshot.values[0].text == "5 min");
    assert(normalized.snapshot.values[0].context == "Eastbound to Main Street Station");
    assert(normalized.snapshot.values[0].eventEpoch == now + 300);
    assert(normalized.snapshot.values[1].text == "10 min");
    assert(normalized.snapshot.values[1].eventEpoch == now + 600);
    assert(normalized.snapshot.fetchedAtEpoch == now);
    assert(normalized.snapshot.dataAtEpoch == now);

    const auto arriving = normalizeTtcSnapshot(0, config, {{"506", now + 1}}, now);
    assert(arriving.outcome == ProviderOutcome::Success);
    assert(arriving.snapshot.values[0].text == "1 min");

    const auto empty = normalizeTtcSnapshot(1, config, {}, now);
    assert(empty.outcome == ProviderOutcome::Empty);
    assert(empty.snapshot.state == WidgetState::Empty);
    assert(empty.snapshot.providerMessage == "No upcoming arrivals");

    WidgetConfig invalid = config;
    invalid.ttc.stopId.clear();
    const auto invalidResult = normalizeTtcSnapshot(1, invalid, records, now);
    assert(invalidResult.outcome == ProviderOutcome::InvalidConfig);
    assert(invalidResult.snapshot.state == WidgetState::Error);
    assert(invalidResult.snapshot.valueCount == 0);
    assert(invalidResult.snapshot.providerMessage == "Incomplete settings");

    const auto unsynced = normalizeTtcSnapshot(1, config, records, 0);
    assert(unsynced.outcome == ProviderOutcome::ClockUnsynced);
    assert(unsynced.snapshot.state == WidgetState::Error);
    assert(unsynced.snapshot.providerMessage == "Clock not synced");

    return 0;
}
