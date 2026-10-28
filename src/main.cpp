#include <Arduino.h>
#include <LittleFS.h>

#include "card.h"
#include "display.h"
#include "input.h"
#include "library.h"
#include "places.h"
#include "reading.h"

ReadingPage reading;
LibraryPage library;
bool inLibrary = false;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Input::init();
    initDisplay();
    mountCard();

    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS");
        return;
    }

    loadPlaces();
    library.list();
    library.select(reading.path);
    if (library.has(reading.path)) {
        size_t chapter = 0;
        size_t page = 0;
        savedPlace(reading.path, chapter, page);
        if (reading.open(chapter, page, false)) {
            return;
        }
    } else {
        library.pick = 0;
    }
    inLibrary = true;
    library.show(true);
}

void loop() {
    const Input::Gesture gesture = Input::read();
    if (gesture.bothLong) {
        if (!inLibrary) {
            library.enter(reading);
            inLibrary = true;
        }
        return;
    }

    if (inLibrary) {
        if (library.onGesture(gesture, reading) == LibraryAction::Opened) {
            inLibrary = false;
        }
        return;
    }

    reading.onGesture(gesture);
}
