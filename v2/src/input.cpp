#include <Arduino.h>

#include "pins.h"
#include "input.h"

namespace Input {
    constexpr unsigned long DEBOUNCE_MS = kDebounceMs;
    constexpr unsigned long LONG_PRESS_MS = kLongPressMs;

    Button prevButton{kPrevPin};
    Button nextButton{kNextPin};

    void init() {
        pinMode(kPrevPin, INPUT_PULLUP);
        pinMode(kNextPin, INPUT_PULLUP);
    }

    void update(Button& button) {
        const int reading = digitalRead(button.pin);
        if (reading != button.lastReading) {
            button.lastDebounceTime = millis();
            button.lastReading = reading;
        }

        if (millis() - button.lastDebounceTime <= kDebounceMs) {
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

    const char* toString(Press press) {
        switch (press) {
            case Press::None: return "None";
            case Press::Short: return "Short";
            case Press::Long: return "Long";
        }
        return "?";
    }

    void print(const Gesture& gesture) {
        Serial.print("prev=");
        Serial.print(toString(gesture.prev));
        Serial.print(" next=");
        Serial.print(toString(gesture.next));
        Serial.print(" bothLong=");
        Serial.println(gesture.bothLong ? "true" : "false");
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
            prevButton.longFired = false;
            nextButton.longFired = false;
            gesture.prev = Press::None;
            gesture.next = Press::None;
            gesture.bothLong = true;
            return gesture;
        }

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
}