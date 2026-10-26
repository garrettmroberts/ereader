#include "input.h"
#include <esp32-hal-gpio.h>
#include <Arduino.h>


namespace Input {
    constexpr int BUTTON1_PIN = 26;
    constexpr int BUTTON2_PIN = 25;
    constexpr unsigned long DEBOUNCE_MS = 50;

    struct Button {
        int pin;
        int lastReading = HIGH;
        int buttonState = HIGH;
        unsigned long lastDebounceTime = 0;
    };

    Button button1 {BUTTON1_PIN};
    Button button2 {BUTTON2_PIN};

    void init() {
        pinMode(BUTTON1_PIN, INPUT_PULLUP);
        pinMode(BUTTON2_PIN, INPUT_PULLUP);
    }

    bool wasPressed(Button& button) {
        int reading = digitalRead(button.pin);
        
        if (reading != button.lastReading) {
            button.lastDebounceTime = millis();
        }

        if (millis() - button.lastDebounceTime > DEBOUNCE_MS) {
            if (reading != button.buttonState) {
                button.buttonState = reading;

                if (button.buttonState == LOW) {
                    button.lastReading = reading;
                    return true;
                }
            }
        }

        button.lastReading = reading;
        return false;
    }

    bool button1Pressed() {
        return wasPressed(button1);
    }

    bool button2Pressed() {
        return wasPressed(button2);
    }
}