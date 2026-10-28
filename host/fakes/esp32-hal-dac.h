#pragma once

#include <stdint.h>

inline int hostDacDisabledPin = -1;

inline void dacDisable(uint8_t pin) {
    hostDacDisabledPin = pin;
}
