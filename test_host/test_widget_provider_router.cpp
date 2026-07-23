#include "providers/WidgetProviderRouter.h"

#include "providers/TtcProvider.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using transitink::ProviderOutcome;
using transitink::ProviderResult;
using transitink::WidgetConfig;
using transitink::WidgetSnapshot;
using transitink::WidgetState;
using transitink::WidgetType;

struct ProviderCall {
    uint8_t slot;
    const WidgetConfig* config;
    int64_t nowEpoch;
};

std::vector<ProviderCall> calls;

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

ProviderResult markerResult(uint8_t slot, WidgetType type, int64_t nowEpoch) {
    WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = type;
    snapshot.state = WidgetState::Ready;
    snapshot.fetchedAtEpoch = nowEpoch;
    snapshot.title = "ttc";
    return {ProviderOutcome::Success, snapshot};
}

void assertForwarded(const ProviderCall& call,
                     uint8_t slot,
                     const WidgetConfig& config,
                     int64_t nowEpoch) {
    assert(call.slot == slot);
    assert(call.config == &config);
    assert(call.nowEpoch == nowEpoch);
}

void assertInvalid(const ProviderResult& result, uint8_t slot, WidgetType type) {
    assert(result.outcome == ProviderOutcome::InvalidConfig);
    assert(result.snapshot.slot == slot);
    assert(result.snapshot.type == type);
    assert(result.snapshot.state == WidgetState::Error);
    assert(result.snapshot.providerMessage == "Incomplete settings");
    assert(result.snapshot.valueCount == 0);
}

class FakeRouter : public transitink::IWidgetProviderRouter {
public:
    std::vector<uint8_t> schedulerCalls;

    ProviderResult fetch(uint8_t slot, const WidgetConfig& config, int64_t nowEpoch) override {
        schedulerCalls.push_back(slot);
        return markerResult(slot, config.type, nowEpoch);
    }
};

}  // namespace

TtcProvider::TtcProvider(TtcClient& client) : client_(client) {}

ProviderResult TtcProvider::fetch(uint8_t slot,
                                  const WidgetConfig& config,
                                  int64_t nowEpoch) {
    (void)client_;
    calls.push_back({slot, &config, nowEpoch});
    return markerResult(slot, config.type, nowEpoch);
}

int main() {
    TtcClient ttcClient;
    TtcProvider ttc(ttcClient);
    WidgetProviderRouter router(ttc);

    {
        const WidgetConfig config = ttcConfig();
        const auto result = router.fetch(1, config, 1700000006);
        assert(result.snapshot.title == "ttc");
        assert(calls.size() == 1);
        assertForwarded(calls.back(), 1, config, 1700000006);
    }

    {
        WidgetConfig config;
        const std::size_t before = calls.size();
        const auto result = router.fetch(2, config, 1700000010);
        assert(result.outcome == ProviderOutcome::Empty);
        assert(result.snapshot.slot == 2);
        assert(result.snapshot.type == WidgetType::Disabled);
        assert(result.snapshot.state == WidgetState::Empty);
        assert(result.snapshot.valueCount == 0);
        assert(calls.size() == before);
    }
    {
        WidgetConfig config = ttcConfig();
        config.ttc.stopId.clear();
        const std::size_t before = calls.size();
        assertInvalid(router.fetch(0, config, 1700000011), 0, WidgetType::TtcEta);
        assert(calls.size() == before);
    }
    {
        WidgetConfig config = ttcConfig();
        const std::size_t before = calls.size();
        assertInvalid(router.fetch(transitink::kWidgetSlotCount, config, 1700000012),
                      transitink::kWidgetSlotCount, WidgetType::TtcEta);
        assert(calls.size() == before);
    }
    {
        WidgetConfig config;
        config.type = static_cast<WidgetType>(255);
        const std::size_t before = calls.size();
        assertInvalid(router.fetch(0, config, 1700000013), 0, config.type);
        assert(calls.size() == before);
    }

    {
        calls.clear();
        transitink::WidgetSlots configs{};
        configs[0] = ttcConfig("501");
        configs[1] = ttcConfig("504");
        configs[2] = ttcConfig("506");
        configs[3] = ttcConfig("511");
        transitink::WidgetScheduler scheduler(router);
        scheduler.configure(configs, 500);

        for (uint8_t slot = 0; slot < 4; ++slot) {
            const std::size_t before = calls.size();
            const auto tick = scheduler.serviceNextDue(500, 1700000100 + slot);
            assert(tick.ran && tick.success && tick.slot == slot);
            assert(calls.size() == before + 1);
            assert(calls.back().slot == slot);
            assert(calls.back().config->type == WidgetType::TtcEta);
            assert(calls.back().nowEpoch == 1700000100 + slot);
        }
        assert(!scheduler.serviceNextDue(500, 1700000200).ran);
        assert(calls.size() == 4);

        scheduler.forceAllDue(600);
        const auto rotated = scheduler.serviceNextDue(600, 1700000201);
        assert(rotated.ran && rotated.slot == 0);
        assert(calls.size() == 5);
    }

    {
        FakeRouter fake;
        transitink::WidgetSlots configs{};
        configs[0] = ttcConfig("506");
        configs[2] = ttcConfig("510");
        transitink::WidgetScheduler scheduler(fake);
        scheduler.configure(configs, 100);
        assert(scheduler.hasEnabledWidgets());
        assert(scheduler.serviceNextDue(100, 1700000300).slot == 0);
        assert(scheduler.serviceNextDue(100, 1700000301).slot == 2);
        assert((fake.schedulerCalls == std::vector<uint8_t>{0, 2}));
    }

    return 0;
}
