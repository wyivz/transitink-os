#include "core/BusEtaCore.h"

namespace bus_eta {

DualButtonHoldDetector::DualButtonHoldDetector(unsigned long thresholdMs) : thresholdMs_(thresholdMs) {}

bool DualButtonHoldDetector::update(bool firstPressed, bool secondPressed, unsigned long nowMs) {
    const bool bothPressed = firstPressed && secondPressed;
    if (!bothPressed) {
        armed_ = true;
        wasBothPressed_ = false;
        fired_ = false;
        bothPressedAtMs_ = 0;
        return false;
    }
    if (!armed_) {
        return false;
    }
    if (!wasBothPressed_) {
        wasBothPressed_ = true;
        fired_ = false;
        bothPressedAtMs_ = nowMs;
        return false;
    }
    if (!fired_ && nowMs - bothPressedAtMs_ >= thresholdMs_) {
        fired_ = true;
        return true;
    }
    return false;
}

SingleButtonClickDetector::SingleButtonClickDetector(unsigned long debounceMs, unsigned long maxClickMs)
    : debounceMs_(debounceMs), maxClickMs_(maxClickMs) {}

bool SingleButtonClickDetector::update(bool pressed, bool inhibited, unsigned long nowMs) {
    if (inhibited) {
        cancelled_ = true;
        wasPressed_ = pressed;
        if (!pressed) {
            pressedAtMs_ = 0;
        }
        return false;
    }
    if (pressed) {
        if (!wasPressed_) {
            pressedAtMs_ = nowMs;
            cancelled_ = false;
        }
        wasPressed_ = true;
        return false;
    }
    if (!wasPressed_) {
        cancelled_ = false;
        return false;
    }

    wasPressed_ = false;
    const unsigned long durationMs = nowMs - pressedAtMs_;
    pressedAtMs_ = 0;
    const bool clicked = !cancelled_ && durationMs >= debounceMs_ && durationMs <= maxClickMs_;
    cancelled_ = false;
    return clicked;
}

DebouncedButtonPressDetector::DebouncedButtonPressDetector(unsigned long debounceMs)
    : debounceMs_(debounceMs) {}

bool DebouncedButtonPressDetector::update(bool pressed, unsigned long nowMs) {
    if (pressed != rawPressed_) {
        rawPressed_ = pressed;
        rawChangedAtMs_ = nowMs;
        return false;
    }
    if (stablePressed_ == rawPressed_ || nowMs - rawChangedAtMs_ < debounceMs_) {
        return false;
    }
    stablePressed_ = rawPressed_;
    return stablePressed_;
}

bool shouldAutoSleep(const SleepSettings& settings,
                     unsigned long wakeStartedAtMs,
                     unsigned long nowMs,
                     bool configAccessMode,
                     bool scheduledWakeSession,
                     bool scheduledWakeWindowActive) {
    if (!settings.enabled || configAccessMode) {
        return false;
    }
    if (settings.scheduledWakeEnabled) {
        if (scheduledWakeWindowActive) {
            return false;
        }
        if (scheduledWakeSession) {
            return true;
        }
    }
    if (settings.wakeDurationMinutes == 0 || wakeStartedAtMs == 0) {
        return false;
    }
    const unsigned long wakeDurationMs = settings.wakeDurationMinutes * 60UL * 1000UL;
    return nowMs - wakeStartedAtMs >= wakeDurationMs;
}

bool isScheduledWakeWindow(const SleepSettings& settings, unsigned int minuteOfDay) {
    constexpr unsigned int kMinutesPerDay = 24 * 60;
    const unsigned int start = settings.scheduledWakeStartMinutes;
    const unsigned int end = settings.scheduledWakeEndMinutes;
    if (!settings.enabled || !settings.scheduledWakeEnabled ||
        minuteOfDay >= kMinutesPerDay || start >= kMinutesPerDay ||
        end >= kMinutesPerDay || start == end) {
        return false;
    }
    if (start < end) {
        return minuteOfDay >= start && minuteOfDay < end;
    }
    return minuteOfDay >= start || minuteOfDay < end;
}

unsigned int secondsUntilScheduledWakeStart(const SleepSettings& settings, unsigned int secondOfDay) {
    constexpr unsigned int kSecondsPerDay = 24 * 60 * 60;
    constexpr unsigned int kMinutesPerDay = 24 * 60;
    if (!settings.enabled || !settings.scheduledWakeEnabled ||
        secondOfDay >= kSecondsPerDay ||
        settings.scheduledWakeStartMinutes >= kMinutesPerDay ||
        settings.scheduledWakeEndMinutes >= kMinutesPerDay ||
        settings.scheduledWakeStartMinutes == settings.scheduledWakeEndMinutes) {
        return 0;
    }
    const unsigned int startSecond = settings.scheduledWakeStartMinutes * 60;
    if (startSecond > secondOfDay) {
        return startSecond - secondOfDay;
    }
    return kSecondsPerDay - secondOfDay + startSecond;
}

unsigned long long sleepMaintenanceIntervalUs(const SleepSettings& settings) {
    if (!settings.enabled || settings.scheduledWakeEnabled || settings.maintenanceHours == 0) {
        return 0;
    }
    return static_cast<unsigned long long>(settings.maintenanceHours) * 60ULL * 60ULL * 1000000ULL;
}

SleepResumeAction decideSleepResumeAction(bool sleepMarkerPending,
                                          bool timerWake,
                                          bool homeGpioWake,
                                          bool homePressedAtBoot,
                                          bool powerOnReset) {
    if (timerWake) {
        return SleepResumeAction::RunMaintenance;
    }
    if (homeGpioWake ||
        (sleepMarkerPending && (homePressedAtBoot || powerOnReset))) {
        return SleepResumeAction::ShowDashboard;
    }
    if (sleepMarkerPending) {
        return SleepResumeAction::ResumeSleep;
    }
    return SleepResumeAction::NormalBoot;
}

}  // namespace bus_eta
