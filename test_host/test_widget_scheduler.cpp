#include "core/WidgetScheduler.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using transitink::Freshness;
using transitink::ProviderOutcome;
using transitink::ProviderResult;
using transitink::WidgetConfig;
using transitink::WidgetSnapshot;
using transitink::WidgetState;
using transitink::WidgetType;

WidgetConfig ttcConfig(const std::string& route = "506") {
    WidgetConfig config;
    config.type = WidgetType::TtcEta;
    config.ttc.routeId = route;
    config.ttc.directionId = "0";
    config.ttc.stopId = "8431";
    config.ttc.routeLabel = route;
    config.ttc.stopLabel = "College St at University Ave";
    config.ttc.destinationLabel = "Eastbound";
    return config;
}

ProviderResult ready(uint8_t slot,
                     WidgetType type,
                     int64_t fetchedAt,
                     int64_t eventEpoch,
                     const std::string& text) {
    WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = type;
    snapshot.valueCount = 1;
    snapshot.values[0] = {text, "", eventEpoch};
    snapshot.state = WidgetState::Ready;
    snapshot.fetchedAtEpoch = fetchedAt;
    snapshot.dataAtEpoch = fetchedAt;
    return {ProviderOutcome::Success, snapshot};
}

ProviderResult outcome(ProviderOutcome providerOutcome,
                       uint8_t slot,
                       WidgetType type,
                       const std::string& message = {}) {
    WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = type;
    snapshot.state =
        providerOutcome == ProviderOutcome::Empty ? WidgetState::Empty : WidgetState::Error;
    snapshot.providerMessage = message;
    return {providerOutcome, snapshot};
}

class FakeRouter : public transitink::IWidgetProviderRouter {
public:
    std::array<std::vector<ProviderResult>, transitink::kWidgetSlotCount> scripted{};
    std::array<std::size_t, transitink::kWidgetSlotCount> cursors{};
    std::vector<uint8_t> calls;

    ProviderResult fetch(uint8_t slot, const WidgetConfig&, int64_t) override {
        calls.push_back(slot);
        assert(cursors[slot] < scripted[slot].size());
        return scripted[slot][cursors[slot]++];
    }
};

}  // namespace

int main() {
    using namespace transitink;

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = ttcConfig();

        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 500);

        const auto& placeholder = scheduler.snapshot(0);
        assert(placeholder.type == WidgetType::TtcEta);
        assert(placeholder.title == "506 · Eastbound");
        assert(placeholder.subtitle == "College St at University Ave");
        assert(placeholder.fetchedAtEpoch == 0);
        assert(placeholder.valueCount == 0);
        assert(router.calls.empty());
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = ttcConfig("501");
        configs[1] = ttcConfig("504");
        configs[2] = ttcConfig("506");
        router.scripted[0] = {ready(0, WidgetType::TtcEta, 1000, 1300, "501")};
        router.scripted[1] = {ready(1, WidgetType::TtcEta, 1000, 1300, "504")};
        router.scripted[2] = {ready(2, WidgetType::TtcEta, 1000, 1300, "506")};

        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 500);
        assert(scheduler.hasEnabledWidgets());
        assert(scheduler.hasPendingDue(500));

        const auto first = scheduler.serviceNextDue(500, 1000);
        assert(first.ran && first.slot == 0 && first.success);
        const auto second = scheduler.serviceNextDue(500, 1000);
        assert(second.ran && second.slot == 1 && second.success);
        const auto third = scheduler.serviceNextDue(500, 1000);
        assert(third.ran && third.slot == 2 && third.success);
        assert(!scheduler.serviceNextDue(500, 1000).ran);
        assert((router.calls == std::vector<uint8_t>{0, 1, 2}));
        assert(!scheduler.hasPendingDue(500));
        assert(!scheduler.hasPendingDue(500 + 59999));

        router.scripted[0].push_back(ready(0, WidgetType::TtcEta, 1060, 1360, "501 2"));
        router.scripted[1].push_back(ready(1, WidgetType::TtcEta, 1060, 1360, "504 2"));
        assert(scheduler.hasPendingDue(500 + 60000));
        const auto due0 = scheduler.serviceNextDue(500 + 60000, 1060);
        const auto due1 = scheduler.serviceNextDue(500 + 60000, 1060);
        assert(due0.slot == 0);
        assert(due1.slot == 1);
        assert(router.calls.back() == 1);

        router.scripted[2].push_back(ready(2, WidgetType::TtcEta, 1060, 1360, "506 2"));
        scheduler.forceAllDue(500 + 60000);
        const std::size_t before = router.calls.size();
        const auto forced = scheduler.serviceNextDue(500 + 60000, 1060);
        assert(forced.ran && forced.slot == 2);
        assert(router.calls.size() == before + 1);
        assert(scheduler.snapshot(3).type == WidgetType::Disabled);
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = ttcConfig();
        router.scripted[0] = {
            ready(0, WidgetType::TtcEta, 1000, 2000, "last good"),
            outcome(ProviderOutcome::Failure, 0, WidgetType::TtcEta),
            outcome(ProviderOutcome::Failure, 0, WidgetType::TtcEta),
            outcome(ProviderOutcome::Failure, 0, WidgetType::TtcEta),
            ready(0, WidgetType::TtcEta, 1240, 2200, "recovered"),
        };

        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 100);
        assert(scheduler.serviceNextDue(100, 1000).success);

        const auto failedOnce = scheduler.serviceNextDue(60100, 1060);
        assert(failedOnce.ran && !failedOnce.success);
        assert(scheduler.snapshot(0).freshness == Freshness::Stale);
        assert(scheduler.snapshot(0).consecutiveFailures == 1);
        assert(scheduler.snapshot(0).valueCount == 1);
        assert(scheduler.snapshot(0).values[0].text == "last good");

        scheduler.serviceNextDue(120100, 1120);
        assert(scheduler.snapshot(0).valueCount == 1);
        scheduler.serviceNextDue(180100, 1180);
        assert(scheduler.snapshot(0).valueCount == 0);
        assert(scheduler.snapshot(0).state == WidgetState::Error);
        assert(scheduler.snapshot(0).providerMessage == "Data expired");
        assert(scheduler.snapshot(0).consecutiveFailures == 3);

        const auto recovered = scheduler.serviceNextDue(240100, 1240);
        assert(recovered.success);
        assert(scheduler.snapshot(0).freshness == Freshness::Fresh);
        assert(scheduler.snapshot(0).consecutiveFailures == 0);
        assert(scheduler.snapshot(0).valueCount == 1);
        assert(scheduler.snapshot(0).values[0].text == "recovered");
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = ttcConfig();
        configs[1] = ttcConfig("504");
        router.scripted[0] = {outcome(ProviderOutcome::Failure, 0, WidgetType::TtcEta)};
        router.scripted[1] = {ready(1, WidgetType::TtcEta, 1000, 1300, "second")};

        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 10);
        scheduler.serviceNextDue(10, 1000);
        assert(scheduler.snapshot(0).state == WidgetState::Error);
        assert(scheduler.snapshot(0).providerMessage == "Unable to refresh");
        assert(scheduler.snapshot(0).valueCount == 0);
        assert(scheduler.snapshot(1).valueCount == 0);
        scheduler.serviceNextDue(10, 1000);
        assert(scheduler.snapshot(1).values[0].text == "second");
        assert(scheduler.snapshot(0).providerMessage == "Unable to refresh");
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = ttcConfig();
        router.scripted[0] = {
            ready(0, WidgetType::TtcEta, 1000, 2000, "old"),
            outcome(ProviderOutcome::InvalidConfig, 0, WidgetType::TtcEta,
                    "Incomplete settings"),
            ready(0, WidgetType::TtcEta, 1120, 2000, "new"),
            outcome(ProviderOutcome::ClockUnsynced, 0, WidgetType::TtcEta,
                    "Clock not synced"),
        };

        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 0);
        scheduler.serviceNextDue(0, 1000);
        scheduler.serviceNextDue(60000, 1060);
        assert(scheduler.snapshot(0).valueCount == 0);
        assert(scheduler.snapshot(0).providerMessage == "Incomplete settings");
        scheduler.serviceNextDue(120000, 1120);
        assert(scheduler.snapshot(0).valueCount == 1);
        scheduler.serviceNextDue(180000, 0);
        assert(scheduler.snapshot(0).valueCount == 0);
        assert(scheduler.snapshot(0).providerMessage == "Clock not synced");
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = ttcConfig();
        router.scripted[0] = {ready(0, WidgetType::TtcEta, 1000, 1050, "expiring")};
        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 0);
        scheduler.serviceNextDue(0, 1000);
        const auto beforeExpiry = scheduler.displaySnapshots(1049);
        assert(beforeExpiry[0].valueCount == 1);
        const auto afterExpiry = scheduler.displaySnapshots(1050);
        assert(afterExpiry[0].valueCount == 0);
        assert(afterExpiry[0].state == WidgetState::Empty);
        assert(afterExpiry[0].providerMessage == "No upcoming arrivals");
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = ttcConfig();
        router.scripted[0] = {ready(0, WidgetType::TtcEta, 1000, 2000, "wrap")};
        WidgetScheduler scheduler(router);
        constexpr uint32_t configuredAt = 0xfffffff5U;
        scheduler.configure(configs, configuredAt);
        scheduler.serviceNextDue(configuredAt, 1000);
        assert(!scheduler.hasPendingDue(20));
        assert(!scheduler.hasPendingDue(59988));
        router.scripted[0].push_back(ready(0, WidgetType::TtcEta, 1060, 2030, "wrap 2"));
        assert(scheduler.hasPendingDue(59989));
        assert(scheduler.serviceNextDue(59989, 1060).ran);
    }

    {
        FakeRouter router;
        WidgetSlots initial{};
        initial[0] = ttcConfig("501");
        initial[1] = ttcConfig("504");
        router.scripted[0] = {ready(0, WidgetType::TtcEta, 1000, 1300, "old 501")};
        router.scripted[1] = {ready(1, WidgetType::TtcEta, 1000, 1300, "old 504")};

        WidgetScheduler scheduler(router);
        scheduler.configure(initial, 100);
        scheduler.serviceNextDue(100, 1000);
        scheduler.serviceNextDue(100, 1000);
        assert(scheduler.snapshot(0).valueCount == 1);
        assert(scheduler.snapshot(1).valueCount == 1);

        WidgetSlots reconfigured{};
        reconfigured[1] = ttcConfig("506");
        const std::size_t callsBeforeReconfigure = router.calls.size();
        scheduler.configure(reconfigured, 500);

        assert(scheduler.snapshot(0).type == WidgetType::Disabled);
        assert(scheduler.snapshot(0).valueCount == 0);
        assert(scheduler.snapshot(1).type == WidgetType::TtcEta);
        assert(scheduler.snapshot(1).valueCount == 0);
        assert(router.calls.size() == callsBeforeReconfigure);
        assert(!scheduler.hasPendingDue(499));
        assert(scheduler.hasPendingDue(500));

        router.scripted[1].push_back(ready(1, WidgetType::TtcEta, 1001, 1400, "new 506"));
        const auto refreshed = scheduler.serviceNextDue(500, 1001);
        assert(refreshed.ran && refreshed.slot == 1);
        assert(scheduler.snapshot(1).values[0].text == "new 506");
    }

    {
        FakeRouter router;
        WidgetScheduler scheduler(router);
        scheduler.configure(WidgetSlots{}, 0);
        assert(!scheduler.hasEnabledWidgets());
        assert(!scheduler.hasPendingDue(0));
        assert(!scheduler.serviceNextDue(0, 1000).ran);
        assert(router.calls.empty());
    }

    return 0;
}
