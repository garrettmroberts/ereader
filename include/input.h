#pragma once

namespace Input {
    enum class Press { None, Short, Long };

    void init();
    Press button1();
    Press button2();
}
