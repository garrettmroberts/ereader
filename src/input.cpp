#include "input.h"
#include <esp32-hal-gpio.h>
#include <Arduino.h>

namespace Input {
    constexpr int BUTTON1_PIN = 25;
    constexpr int BUTTON2_PIN = 26;
    constexpr unsigned long DEBOUNCE_MS = 50;
    constexpr unsigned long LONG_PRESS_MS = 500;

    struct Button {
        int pin;
        int lastReading = HIGH;
        int buttonState = HIGH;
        unsigned long lastDebounceTime = 0;
        unsigned long pressedAt = 0;
        bool longFired = false;
    };

    Button prevButton{BUTTON1_PIN};
    Button nextButton{BUTTON2_PIN};

    void init() {
        pinMode(BUTTON1_PIN, INPUT_PULLUP);
        pinMode(BUTTON2_PIN, INPUT_PULLUP);
    }

    Press poll(Button& button) {
        const int reading = digitalRead(button.pin);
        if (reading != button.lastReading) {
            button.lastDebounceTime = millis();
            button.lastReading = reading;
        }

        if (millis() - button.lastDebounceTime <= DEBOUNCE_MS) {
            return Press::None;
        }

        if (reading != button.buttonState) {
            button.buttonState = reading;
            if (reading == LOW) {
                button.pressedAt = millis();
                button.longFired = false;
                return Press::None;
            }
            if (!button.longFired) {
                return Press::Short;
            }
            return Press::None;
        }

        if (button.buttonState == LOW && !button.longFired &&
            millis() - button.pressedAt >= LONG_PRESS_MS) {
            button.longFired = true;
            return Press::Long;
        }
        return Press::None;
    }

    Press button1() {
        return poll(prevButton);
    }

    Press button2() {
        return poll(nextButton);
    }
}
