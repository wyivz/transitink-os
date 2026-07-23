#include "core/WidgetConfigCore.h"

#include <cassert>
#include <string>

namespace {

transitink::WidgetConfig validTtcWidget() {
    transitink::WidgetConfig widget;
    widget.type = transitink::WidgetType::TtcEta;
    widget.ttc.routeId = "506";
    widget.ttc.directionId = "0";
    widget.ttc.stopId = "8431";
    widget.ttc.routeLabel = "506";
    widget.ttc.stopLabel = "College St at University Ave";
    widget.ttc.destinationLabel = "Eastbound";
    return widget;
}

}  // namespace

int main() {
    static_assert(transitink::kConfigSchemaVersion == 3);

    transitink::WidgetSlots slots{};
    static_assert(slots.size() == 4);
    for (const auto& slot : slots) {
        assert(slot.type == transitink::WidgetType::Disabled);
        assert(transitink::isWidgetConfigValid(slot));
    }

    assert(std::string(transitink::widgetTypeId(transitink::WidgetType::Disabled)) ==
           "disabled");
    assert(std::string(transitink::widgetTypeId(transitink::WidgetType::TtcEta)) ==
           "ttc_eta");
    assert(std::string(transitink::widgetTypeId(static_cast<transitink::WidgetType>(255))) ==
           "");

    transitink::WidgetType widgetType = transitink::WidgetType::Disabled;
    assert(transitink::parseWidgetTypeId("disabled", widgetType));
    assert(widgetType == transitink::WidgetType::Disabled);
    assert(transitink::parseWidgetTypeId("ttc_eta", widgetType));
    assert(widgetType == transitink::WidgetType::TtcEta);
    assert(!transitink::parseWidgetTypeId("bus_eta", widgetType));
    assert(!transitink::parseWidgetTypeId("gmb_eta", widgetType));
    assert(!transitink::parseWidgetTypeId("mtr_eta", widgetType));
    assert(!transitink::parseWidgetTypeId("journey_time", widgetType));

    transitink::WidgetConfig ttcWidget = validTtcWidget();
    assert(transitink::isWidgetConfigValid(ttcWidget));

    ttcWidget.ttc.routeId.clear();
    assert(!transitink::isWidgetConfigValid(ttcWidget));
    ttcWidget = validTtcWidget();
    ttcWidget.ttc.directionId.clear();
    assert(!transitink::isWidgetConfigValid(ttcWidget));
    ttcWidget = validTtcWidget();
    ttcWidget.ttc.stopId.clear();
    assert(!transitink::isWidgetConfigValid(ttcWidget));

    ttcWidget = validTtcWidget();
    ttcWidget.ttc.routeLabel.assign(transitink::kMaxConfigLabelBytes + 1, 'R');
    assert(!transitink::isWidgetConfigValid(ttcWidget));

    return 0;
}
