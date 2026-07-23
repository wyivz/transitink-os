#include "providers/WidgetProviderRouter.h"

#include "providers/TtcProvider.h"

namespace {

transitink::ProviderResult invalidResult(uint8_t slot,
                                         const transitink::WidgetConfig& config,
                                         int64_t nowEpoch) {
    transitink::WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = config.type;
    snapshot.state = transitink::WidgetState::Error;
    snapshot.providerMessage = "Incomplete settings";
    snapshot.fetchedAtEpoch = nowEpoch;
    return {transitink::ProviderOutcome::InvalidConfig, snapshot};
}

transitink::ProviderResult disabledResult(uint8_t slot) {
    transitink::WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = transitink::WidgetType::Disabled;
    snapshot.state = transitink::WidgetState::Empty;
    return {transitink::ProviderOutcome::Empty, snapshot};
}

}  // namespace

WidgetProviderRouter::WidgetProviderRouter(TtcProvider& ttc) : ttc_(ttc) {}

transitink::ProviderResult WidgetProviderRouter::fetch(
    uint8_t slot, const transitink::WidgetConfig& config, int64_t nowEpoch) {
    if (slot >= transitink::kWidgetSlotCount || !transitink::isWidgetConfigValid(config)) {
        return invalidResult(slot, config, nowEpoch);
    }

    switch (config.type) {
        case transitink::WidgetType::Disabled:
            return disabledResult(slot);
        case transitink::WidgetType::TtcEta:
            return ttc_.fetch(slot, config, nowEpoch);
    }
    return invalidResult(slot, config, nowEpoch);
}
