#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace transitink {

struct GtfsRtArrival {
    std::string routeId;
    std::string tripId;
    std::string stopId;
    int64_t eventEpoch = 0;
};

// Stream-parse a GTFS-Realtime FeedMessage and keep arrivals for one stop.
// When routeIdFilter is non-empty, only that route_id is kept.
// Results are sorted by eventEpoch ascending and truncated to maxResults.
bool filterGtfsRtTripUpdates(const uint8_t* data,
                             std::size_t size,
                             const std::string& stopId,
                             const std::string& routeIdFilter,
                             int64_t nowEpoch,
                             std::size_t maxResults,
                             std::vector<GtfsRtArrival>& out);

}  // namespace transitink
