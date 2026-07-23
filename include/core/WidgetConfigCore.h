#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace transitink {

constexpr uint16_t kConfigSchemaVersion = 3;
constexpr std::size_t kWidgetSlotCount = 4;
constexpr std::size_t kMaxStableIdBytes = 64;
constexpr std::size_t kMaxConfigLabelBytes = 96;

enum class WidgetType : uint8_t { Disabled, TtcEta };

struct TtcWidgetConfig {
    std::string routeId, directionId, stopId;
    std::string routeLabel, stopLabel, destinationLabel;
};

struct WidgetConfig {
    WidgetType type = WidgetType::Disabled;
    TtcWidgetConfig ttc;
};

using WidgetSlots = std::array<WidgetConfig, kWidgetSlotCount>;

const char* widgetTypeId(WidgetType value);
bool parseWidgetTypeId(const std::string& value, WidgetType& out);
bool isWidgetConfigValid(const WidgetConfig& widget);

}  // namespace transitink
