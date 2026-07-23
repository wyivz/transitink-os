#pragma once

#include <Arduino.h>
#include <ctime>

struct WeatherSnapshot {
    bool valid = false;
    String locationTc;
    int temperatureC = 0;
    String conditionTc;
    time_t updatedAt = 0;
    String error;
};

// Weather is disabled in the TTC-only build; keep the snapshot type for display layout.
inline String weatherDisplayText(const WeatherSnapshot& snapshot) {
    (void)snapshot;
    return "";
}
