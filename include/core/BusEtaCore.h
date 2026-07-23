#pragma once

#include <cstddef>
#include <string>

namespace bus_eta {

class DualButtonHoldDetector {
public:
    explicit DualButtonHoldDetector(unsigned long thresholdMs);
    bool update(bool firstPressed, bool secondPressed, unsigned long nowMs);

private:
    unsigned long thresholdMs_;
    unsigned long bothPressedAtMs_ = 0;
    bool armed_ = false;
    bool wasBothPressed_ = false;
    bool fired_ = false;
};

class SingleButtonClickDetector {
public:
    SingleButtonClickDetector(unsigned long debounceMs, unsigned long maxClickMs);
    bool update(bool pressed, bool inhibited, unsigned long nowMs);

private:
    unsigned long debounceMs_;
    unsigned long maxClickMs_;
    unsigned long pressedAtMs_ = 0;
    bool wasPressed_ = false;
    bool cancelled_ = false;
};

class DebouncedButtonPressDetector {
public:
    explicit DebouncedButtonPressDetector(unsigned long debounceMs);
    bool update(bool pressed, unsigned long nowMs);

private:
    unsigned long debounceMs_;
    unsigned long rawChangedAtMs_ = 0;
    bool rawPressed_ = false;
    bool stablePressed_ = false;
};

struct SleepSettings {
    bool enabled = true;
    unsigned int wakeDurationMinutes = 5;
    unsigned int maintenanceHours = 12;
    bool scheduledWakeEnabled = false;
    unsigned int scheduledWakeStartMinutes = 8 * 60;
    unsigned int scheduledWakeEndMinutes = 9 * 60;
};

enum class SleepResumeAction {
    NormalBoot,
    ShowDashboard,
    RunMaintenance,
    ResumeSleep,
};

bool shouldAutoSleep(const SleepSettings& settings,
                     unsigned long wakeStartedAtMs,
                     unsigned long nowMs,
                     bool configAccessMode,
                     bool scheduledWakeSession = false,
                     bool scheduledWakeWindowActive = false);
bool isScheduledWakeWindow(const SleepSettings& settings, unsigned int minuteOfDay);
unsigned int secondsUntilScheduledWakeStart(const SleepSettings& settings, unsigned int secondOfDay);
unsigned long long sleepMaintenanceIntervalUs(const SleepSettings& settings);
SleepResumeAction decideSleepResumeAction(bool sleepMarkerPending,
                                          bool timerWake,
                                          bool homeGpioWake,
                                          bool homePressedAtBoot,
                                          bool powerOnReset);

}  // namespace bus_eta
