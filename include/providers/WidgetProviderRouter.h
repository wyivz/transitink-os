#pragma once

#include <cstdint>

#include "core/WidgetScheduler.h"

class TtcProvider;

class WidgetProviderRouter final : public transitink::IWidgetProviderRouter {
public:
    explicit WidgetProviderRouter(TtcProvider& ttc);

    transitink::ProviderResult fetch(uint8_t slot,
                                     const transitink::WidgetConfig& config,
                                     int64_t nowEpoch) override;

private:
    TtcProvider& ttc_;
};
