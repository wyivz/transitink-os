#include "core/GtfsRealtimeTripFilter.h"

#include <algorithm>
#include <cstring>

namespace transitink {
namespace {

struct Reader {
    const uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t offset = 0;

    bool remaining(std::size_t need) const { return offset + need <= size; }

    bool readVarint(uint64_t& value) {
        value = 0;
        int shift = 0;
        while (remaining(1)) {
            const uint8_t byte = data[offset++];
            value |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) return true;
            shift += 7;
            if (shift > 63) return false;
        }
        return false;
    }

    bool skipVarint() {
        uint64_t unused = 0;
        return readVarint(unused);
    }

    bool readBytes(std::size_t length, const uint8_t*& out, std::size_t& outLength) {
        if (!remaining(length)) return false;
        out = data + offset;
        outLength = length;
        offset += length;
        return true;
    }

    bool skip(std::size_t length) {
        if (!remaining(length)) return false;
        offset += length;
        return true;
    }

    bool skipField(uint32_t wireType) {
        switch (wireType) {
            case 0:
                return skipVarint();
            case 1:
                return skip(8);
            case 2: {
                uint64_t length = 0;
                return readVarint(length) && skip(static_cast<std::size_t>(length));
            }
            case 5:
                return skip(4);
            default:
                return false;
        }
    }
};

bool readLengthDelimited(Reader& reader, Reader& nested) {
    uint64_t length = 0;
    if (!reader.readVarint(length)) return false;
    const uint8_t* bytes = nullptr;
    std::size_t byteLength = 0;
    if (!reader.readBytes(static_cast<std::size_t>(length), bytes, byteLength)) return false;
    nested = Reader{bytes, byteLength, 0};
    return true;
}

bool readString(Reader& reader, std::string& out) {
    Reader nested;
    if (!readLengthDelimited(reader, nested)) return false;
    out.assign(reinterpret_cast<const char*>(nested.data), nested.size);
    return true;
}

bool parseStopTimeEvent(Reader reader, int64_t& eventEpoch) {
    eventEpoch = 0;
    while (reader.offset < reader.size) {
        uint64_t key = 0;
        if (!reader.readVarint(key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 0x7);
        if (field == 2 && wire == 0) {
            uint64_t value = 0;
            if (!reader.readVarint(value)) return false;
            eventEpoch = static_cast<int64_t>(value);
        } else if (!reader.skipField(wire)) {
            return false;
        }
    }
    return true;
}

bool parseStopTimeUpdate(Reader reader,
                         const std::string& stopId,
                         int64_t& eventEpoch,
                         bool& matched) {
    matched = false;
    eventEpoch = 0;
    int64_t arrival = 0;
    int64_t departure = 0;
    std::string updateStopId;
    while (reader.offset < reader.size) {
        uint64_t key = 0;
        if (!reader.readVarint(key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 0x7);
        if (field == 2 && wire == 2) {
            Reader nested;
            if (!readLengthDelimited(reader, nested)) return false;
            if (!parseStopTimeEvent(nested, arrival)) return false;
        } else if (field == 3 && wire == 2) {
            Reader nested;
            if (!readLengthDelimited(reader, nested)) return false;
            if (!parseStopTimeEvent(nested, departure)) return false;
        } else if (field == 4 && wire == 2) {
            if (!readString(reader, updateStopId)) return false;
        } else if (!reader.skipField(wire)) {
            return false;
        }
    }
    if (updateStopId != stopId) return true;
    eventEpoch = arrival > 0 ? arrival : departure;
    matched = eventEpoch > 0;
    return true;
}

bool parseTripDescriptor(Reader reader, std::string& tripId, std::string& routeId) {
    tripId.clear();
    routeId.clear();
    while (reader.offset < reader.size) {
        uint64_t key = 0;
        if (!reader.readVarint(key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 0x7);
        if (field == 1 && wire == 2) {
            if (!readString(reader, tripId)) return false;
        } else if (field == 5 && wire == 2) {
            if (!readString(reader, routeId)) return false;
        } else if (!reader.skipField(wire)) {
            return false;
        }
    }
    return true;
}

bool parseTripUpdate(Reader reader,
                     const std::string& stopId,
                     const std::string& routeIdFilter,
                     int64_t nowEpoch,
                     std::vector<GtfsRtArrival>& out) {
    std::string tripId;
    std::string routeId;
    const Reader pass = reader;
    while (reader.offset < reader.size) {
        uint64_t key = 0;
        if (!reader.readVarint(key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 0x7);
        if (field == 1 && wire == 2) {
            Reader nested;
            if (!readLengthDelimited(reader, nested)) return false;
            if (!parseTripDescriptor(nested, tripId, routeId)) return false;
        } else if (!reader.skipField(wire)) {
            return false;
        }
    }
    if (!routeIdFilter.empty() && routeId != routeIdFilter) {
        return true;
    }

    reader = pass;
    while (reader.offset < reader.size) {
        uint64_t key = 0;
        if (!reader.readVarint(key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 0x7);
        if (field == 2 && wire == 2) {
            Reader nested;
            if (!readLengthDelimited(reader, nested)) return false;
            int64_t eventEpoch = 0;
            bool matched = false;
            if (!parseStopTimeUpdate(nested, stopId, eventEpoch, matched)) return false;
            if (matched && eventEpoch > nowEpoch) {
                GtfsRtArrival arrival;
                arrival.routeId = routeId;
                arrival.tripId = tripId;
                arrival.stopId = stopId;
                arrival.eventEpoch = eventEpoch;
                out.push_back(std::move(arrival));
            }
        } else if (!reader.skipField(wire)) {
            return false;
        }
    }
    return true;
}

bool parseFeedEntity(Reader reader,
                     const std::string& stopId,
                     const std::string& routeIdFilter,
                     int64_t nowEpoch,
                     std::vector<GtfsRtArrival>& out) {
    while (reader.offset < reader.size) {
        uint64_t key = 0;
        if (!reader.readVarint(key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 0x7);
        if (field == 3 && wire == 2) {
            Reader nested;
            if (!readLengthDelimited(reader, nested)) return false;
            if (!parseTripUpdate(nested, stopId, routeIdFilter, nowEpoch, out)) return false;
        } else if (!reader.skipField(wire)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool filterGtfsRtTripUpdates(const uint8_t* data,
                             std::size_t size,
                             const std::string& stopId,
                             const std::string& routeIdFilter,
                             int64_t nowEpoch,
                             std::size_t maxResults,
                             std::vector<GtfsRtArrival>& out) {
    out.clear();
    if (data == nullptr || size == 0 || stopId.empty() || maxResults == 0) {
        return false;
    }

    Reader reader{data, size, 0};
    while (reader.offset < reader.size) {
        uint64_t key = 0;
        if (!reader.readVarint(key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 0x7);
        if (field == 2 && wire == 2) {
            Reader nested;
            if (!readLengthDelimited(reader, nested)) return false;
            if (!parseFeedEntity(nested, stopId, routeIdFilter, nowEpoch, out)) return false;
        } else if (!reader.skipField(wire)) {
            return false;
        }
    }

    std::stable_sort(out.begin(), out.end(),
                     [](const GtfsRtArrival& left, const GtfsRtArrival& right) {
                         return left.eventEpoch < right.eventEpoch;
                     });
    if (out.size() > maxResults) out.resize(maxResults);
    return true;
}

}  // namespace transitink
