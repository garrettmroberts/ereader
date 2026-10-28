#pragma once

#include "input.h"
#include "reading.h"

#include <Arduino.h>
#include <vector>

enum class LibraryAction { None, Opened };

// The card catalog. One book is shown at a time, with its cover when one is available.
class LibraryPage {
public:
    std::vector<String> books;
    size_t pick = 0;

    void list();
    void select(const String& path);
    bool has(const String& path) const;
    void show(bool fullRefresh = false);
    void enter(ReadingPage& reading);
    LibraryAction onGesture(const Input::Gesture& gesture, ReadingPage& reading);

    // Draws the selected book. The device paints the panel; tests record the call.
    void paint(bool fullRefresh);

private:
    bool openSelected(ReadingPage& reading);
};

bool endsWithEpub(const char* path);

#ifdef UNIT_TEST
void libraryResetForTest();
#endif
