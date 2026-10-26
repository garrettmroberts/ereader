#include "input.h"
#include <esp32-hal-gpio.h>
#include <Arduino.h>

namespace Input {
    constexpr int BUTTON1_PIN = kPrevPin;
    constexpr int BUTTON2_PIN = kNextPin;
    constexpr unsigned long DEBOUNCE_MS = kDebounceMs;
    constexpr unsigned long LONG_PRESS_MS = kLongPressMs;

    struct Button {
        int pin;
        int lastReading = HIGH;
        int buttonState = HIGH;
        unsigned long lastDebounceTime = 0;
        unsigned long pressedAt = 0;
        bool longFired = false;
        Press pending = Press::None;
    };

    Button prevButton{BUTTON1_PIN};
    Button nextButton{BUTTON2_PIN};

    void init() {
        pinMode(BUTTON1_PIN, INPUT_PULLUP);
        pinMode(BUTTON2_PIN, INPUT_PULLUP);
    }

    void update(Button& button) {
        const int reading = digitalRead(button.pin);
        if (reading != button.lastReading) {
            button.lastDebounceTime = millis();
            button.lastReading = reading;
        }

        if (millis() - button.lastDebounceTime <= DEBOUNCE_MS) {
            return;
        }

        if (reading == button.buttonState) {
            return;
        }

        button.buttonState = reading;
        if (reading == LOW) {
            button.pressedAt = millis();
            button.longFired = false;
            return;
        }
        if (!button.longFired) {
            button.pending = Press::Short;
        }
    }

    bool held(const Button& button) {
        return button.buttonState == LOW;
    }

    bool longReady(const Button& button) {
        return held(button) && !button.longFired && millis() - button.pressedAt >= LONG_PRESS_MS;
    }

    Gesture read() {
        update(prevButton);
        update(nextButton);

        Gesture gesture;
        gesture.prev = prevButton.pending;
        gesture.next = nextButton.pending;
        prevButton.pending = Press::None;
        nextButton.pending = Press::None;

        const bool prevDown = held(prevButton);
        const bool nextDown = held(nextButton);
        const bool prevLong = longReady(prevButton);
        const bool nextLong = longReady(nextButton);

        if (prevDown && nextDown && prevLong && nextLong) {
            prevButton.longFired = true;
            nextButton.longFired = true;
            gesture.prev = Press::None;
            gesture.next = Press::None;
            gesture.bothLong = true;
            return gesture;
        }

        // While the other button is down, wait. It may become a two-button hold.
        if (prevLong && !nextDown) {
            prevButton.longFired = true;
            gesture.prev = Press::Long;
        }
        if (nextLong && !prevDown) {
            nextButton.longFired = true;
            gesture.next = Press::Long;
        }
        return gesture;
    }

#ifdef UNIT_TEST
    void resetForTest() {
        prevButton = Button{BUTTON1_PIN};
        nextButton = Button{BUTTON2_PIN};
    }
#endif
}
