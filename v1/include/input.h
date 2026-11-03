#pragma once

namespace Input {
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
