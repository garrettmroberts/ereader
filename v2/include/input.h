#pragma once

namespace Input {
    constexpr unsigned long kDebounceMs = 50;
    constexpr unsigned long kLongPressMs = 500;

    enum class Press { None, Short, Long };

    struct Gesture {
        Press prev = Press::None;
        Press next = Press::None;
        bool bothLong = false;

        bool operator==(const Gesture& other) const {
            return prev == other.prev && next == other.next && bothLong == other.bothLong;
        }

        bool operator!=(const Gesture& other) const {
            return !(*this == other);
        }
    };

    struct Button {
        int pin;
        int lastReading = HIGH;
        int buttonState = HIGH;
        unsigned long lastDebounceTime = 0;
        unsigned long pressedAt = 0;
        bool longFired = false;
        Press pending = Press::None;
    };

    void init();
    void print(const Gesture& gesture);
    Gesture read();
}