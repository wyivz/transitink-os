#ifdef BUTTON_MAPPER

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <driver/gpio.h>

#include "EInkDisplay.h"
#include "hardware/BoardProfile.h"

namespace {

struct CandidatePin {
    const char* name;
    int gpio;
};

struct ButtonStep {
    const char* label;
    const char* key;
    bool pinhole;
};

constexpr CandidatePin kCandidatePins[] = {
    {"Home / BOOT", transitink::hardware::kBoardProfile.buttons.homePin},
    {"Volume Up", transitink::hardware::kBoardProfile.buttons.upPin},
    {"Volume Down", transitink::hardware::kBoardProfile.buttons.downPin},
    {"GPIO_NUM_1", GPIO_NUM_1},
    {"GPIO_NUM_2", GPIO_NUM_2},
    {"GPIO_NUM_5", GPIO_NUM_5},
    {"GPIO_NUM_7", GPIO_NUM_7},
};

constexpr ButtonStep kButtonSteps[] = {
    {"Pinhole", "pinhole", true},
    {"Volume Up", "vol_up", false},
    {"Volume Down", "vol_down", false},
    {"Home Button", "home", false},
};

constexpr int kStepCount = sizeof(kButtonSteps) / sizeof(kButtonSteps[0]);
constexpr int kCandidateCount = sizeof(kCandidatePins) / sizeof(kCandidatePins[0]);
constexpr unsigned long kDetectWindowMs = 30000;
constexpr unsigned long kStableMs = 70;

Preferences preferences;
EInkDisplay display;
int stepIndex = 0;
int baseline[kCandidateCount] = {};

void showMessage(const String& message) {
    Serial.println();
    Serial.println(message);
    display.showWifiStatus(message);
}

void saveMapping(const ButtonStep& step, const String& mapping) {
    String key = "map_";
    key += step.key;
    preferences.putString(key.c_str(), mapping);
    Serial.print("MAPPED ");
    Serial.print(step.label);
    Serial.print(": ");
    Serial.println(mapping);
}

void printSummary() {
    Serial.println();
    Serial.println("=== Button map summary ===");
    String summary = "Button map\n";
    for (int i = 0; i < kStepCount; ++i) {
        String key = "map_";
        key += kButtonSteps[i].key;
        String value = preferences.getString(key.c_str(), "Not detected");
        Serial.print(kButtonSteps[i].label);
        Serial.print(" => ");
        Serial.println(value);
        summary += kButtonSteps[i].label;
        summary += ": ";
        summary += value;
        summary += "\n";
    }
    display.showWifiStatus(summary);
}

void configurePins() {
    for (int i = 0; i < kCandidateCount; ++i) {
        pinMode(kCandidatePins[i].gpio, INPUT_PULLUP);
    }
}

void captureBaseline() {
    for (int i = 0; i < kCandidateCount; ++i) {
        baseline[i] = digitalRead(kCandidatePins[i].gpio);
    }
}

String detectChangedPin() {
    unsigned long started = millis();
    while (millis() - started < kDetectWindowMs) {
        for (int i = 0; i < kCandidateCount; ++i) {
            int value = digitalRead(kCandidatePins[i].gpio);
            if (value == baseline[i]) {
                continue;
            }
            unsigned long changedAt = millis();
            while (millis() - changedAt < kStableMs) {
                if (digitalRead(kCandidatePins[i].gpio) == baseline[i]) {
                    break;
                }
                delay(5);
            }
            if (millis() - changedAt >= kStableMs) {
                String result = kCandidatePins[i].name;
                result += " GPIO";
                result += kCandidatePins[i].gpio;
                result += value == LOW ? " LOW" : " HIGH";
                return result;
            }
        }
        delay(10);
    }
    return "";
}

void runStep() {
    if (stepIndex >= kStepCount) {
        printSummary();
        delay(5000);
        return;
    }

    const ButtonStep& step = kButtonSteps[stepIndex];
    String prompt = "Press the requested button\n";
    prompt += step.label;
    prompt += "\nPress once within 30 seconds";
    if (step.pinhole) {
        prompt += "\nIf the device reboots, that control is hardware Reset/EN";
    }
    showMessage(prompt);
    captureBaseline();

    String detected = detectChangedPin();
    if (detected.length() == 0) {
        detected = step.pinhole
                       ? "No GPIO change; reboot on press means hardware Reset/EN"
                       : "Not detected";
    }
    saveMapping(step, detected);
    preferences.putInt("step", stepIndex + 1);
    stepIndex += 1;
    delay(900);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    display.begin();
    preferences.begin("button_mapper", false);
    preferences.clear();
    configurePins();
    stepIndex = preferences.getInt("step", 0);
    Serial.println("Button mapper ready");
    Serial.println("Press the requested button when it appears.");
    Serial.print("Boot reset_reason=");
    Serial.println(static_cast<int>(esp_reset_reason()));
}

void loop() {
    runStep();
}

#endif  // BUTTON_MAPPER
