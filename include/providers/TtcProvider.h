#pragma once

#include <cstdint>

#include "TtcClient.h"
#include "core/WidgetCore.h"

class TtcProvider {
public:
    explicit TtcProvider(TtcClient& client);

    transitink::ProviderResult fetch(uint8_t slot,
                                     const transitink::WidgetConfig& config,
                                     int64_t nowEpoch);

private:
    TtcClient& client_;
};
