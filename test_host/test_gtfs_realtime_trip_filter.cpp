#include "core/GtfsRealtimeTripFilter.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

void appendVarint(std::vector<uint8_t>& out, uint64_t value) {
    while (value >= 0x80) {
        out.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<uint8_t>(value));
}

void appendKey(std::vector<uint8_t>& out, uint32_t field, uint32_t wire) {
    appendVarint(out, (static_cast<uint64_t>(field) << 3) | wire);
}

void appendBytes(std::vector<uint8_t>& out, uint32_t field, const std::vector<uint8_t>& payload) {
    appendKey(out, field, 2);
    appendVarint(out, payload.size());
    out.insert(out.end(), payload.begin(), payload.end());
}

void appendString(std::vector<uint8_t>& out, uint32_t field, const std::string& value) {
    appendBytes(out, field, std::vector<uint8_t>(value.begin(), value.end()));
}

std::vector<uint8_t> makeFeed() {
    std::vector<uint8_t> arrival;
    appendKey(arrival, 2, 0);
    appendVarint(arrival, 1700000060);

    std::vector<uint8_t> stopUpdate;
    appendBytes(stopUpdate, 2, arrival);
    appendString(stopUpdate, 4, "8431");

    std::vector<uint8_t> otherUpdate;
    std::vector<uint8_t> otherArrival;
    appendKey(otherArrival, 2, 0);
    appendVarint(otherArrival, 1700000120);
    appendBytes(otherUpdate, 2, otherArrival);
    appendString(otherUpdate, 4, "9999");

    std::vector<uint8_t> trip;
    appendString(trip, 1, "trip-1");
    appendString(trip, 5, "506");

    std::vector<uint8_t> tripUpdate;
    appendBytes(tripUpdate, 1, trip);
    appendBytes(tripUpdate, 2, stopUpdate);
    appendBytes(tripUpdate, 2, otherUpdate);

    std::vector<uint8_t> entity;
    appendString(entity, 1, "1");
    appendBytes(entity, 3, tripUpdate);

    std::vector<uint8_t> feed;
    appendBytes(feed, 2, entity);
    return feed;
}

}  // namespace

int main() {
    const auto feed = makeFeed();
    std::vector<transitink::GtfsRtArrival> arrivals;
    assert(transitink::filterGtfsRtTripUpdates(feed.data(), feed.size(), "8431", "506",
                                               1700000000, 4, arrivals));
    assert(arrivals.size() == 1);
    assert(arrivals[0].routeId == "506");
    assert(arrivals[0].stopId == "8431");
    assert(arrivals[0].eventEpoch == 1700000060);

    assert(transitink::filterGtfsRtTripUpdates(feed.data(), feed.size(), "8431", "504",
                                               1700000000, 4, arrivals));
    assert(arrivals.empty());

    assert(transitink::filterGtfsRtTripUpdates(feed.data(), feed.size(), "8431", "",
                                               1700000000, 4, arrivals));
    assert(arrivals.size() == 1);
    return 0;
}
