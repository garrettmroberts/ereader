#pragma once

typedef int gpio_num_t;

inline int hostPullupPin = -1;

inline void gpio_pullup_en(gpio_num_t pin) {
    hostPullupPin = pin;
}
