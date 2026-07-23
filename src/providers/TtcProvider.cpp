#include "providers/TtcProvider.h"

#include <vector>

TtcProvider::TtcProvider(TtcClient& client) : client_(client) {}

transitink::ProviderResult TtcProvider::fetch(uint8_t slot,
                                              const transitink::WidgetConfig& config,
                                              int64_t nowEpoch) {
    auto baseline = transitink::normalizeTtcSnapshot(slot, config, {}, nowEpoch);
    if (baseline.outcome == transitink::ProviderOutcome::InvalidConfig ||
        baseline.outcome == transitink::ProviderOutcome::ClockUnsynced) {
        return baseline;
    }

    std::vector<transitink::TtcEtaRecord> records;
    String error;
    if (!client_.fetchEtaRecords(config.ttc, records, error)) {
        baseline.outcome = transitink::ProviderOutcome::Failure;
        baseline.snapshot.state = transitink::WidgetState::Error;
        baseline.snapshot.providerMessage =
            error.length() == 0 ? "未能更新 TTC 到站時間" : error.c_str();
        return baseline;
    }
    return transitink::normalizeTtcSnapshot(slot, config, records, nowEpoch);
}
