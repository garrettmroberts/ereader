#pragma once

namespace Input {
    constexpr int kPrevPin = 32;
    constexpr int kNextPin = 33;
    constexpr unsigned long kDebounceMs = 50;
    constexpr unsigned long kLongPressMs = 500;

    enum class Press { None, Short, Long };

    struct Gesture {
        Press prev = Press::None;
        Press next = Press::None;
        bool bothLong = false;
    };

    void init();
    Gesture read();
}
