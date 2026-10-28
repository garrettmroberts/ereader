#pragma once

#include "input.h"

#include <Arduino.h>
#include <vector>

// One open book. The epub stays on the card; this holds the current chapter text.
class ReadingPage {
public:
    String path;
    String text;
    std::vector<size_t> pageStarts;
    size_t chapter = 0;
    size_t page = 0;

    bool open(size_t index, size_t page, bool lastPage);
    void show(bool fullRefresh = false);
    void onGesture(const Input::Gesture& gesture);

    // Draws the current chapter text. The device paints the panel; tests record the call.
    void paint(size_t offset, bool fullRefresh);
};
