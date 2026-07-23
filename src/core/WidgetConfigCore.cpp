#include "core/WidgetConfigCore.h"

namespace transitink {
namespace {

bool isRequiredIdValid(const std::string& value) {
    return !value.empty() && value.size() <= kMaxStableIdBytes;
}

bool isLabelValid(const std::string& value) {
    return value.size() <= kMaxConfigLabelBytes;
}

}  // namespace

const char* widgetTypeId(WidgetType value) {
    switch (value) {
        case WidgetType::Disabled:
            return "disabled";
        case WidgetType::TtcEta:
            return "ttc_eta";
    }
    return "";
}

bool parseWidgetTypeId(const std::string& value, WidgetType& out) {
    if (value == "disabled") {
        out = WidgetType::Disabled;
    } else if (value == "ttc_eta") {
        out = WidgetType::TtcEta;
    } else {
        return false;
    }
    return true;
}

bool isWidgetConfigValid(const WidgetConfig& widget) {
    switch (widget.type) {
        case WidgetType::Disabled:
            return true;
        case WidgetType::TtcEta:
            return isRequiredIdValid(widget.ttc.routeId) &&
                   isRequiredIdValid(widget.ttc.directionId) &&
                   isRequiredIdValid(widget.ttc.stopId) &&
                   isLabelValid(widget.ttc.routeLabel) &&
                   isLabelValid(widget.ttc.stopLabel) &&
                   isLabelValid(widget.ttc.destinationLabel);
    }
    return false;
}

}  // namespace transitink
