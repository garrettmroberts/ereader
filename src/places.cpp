#include "places.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <vector>

namespace {

struct Place {
    String path;
    size_t chapter = 0;
    size_t page = 0;
};

std::vector<Place> places;

void savePlaces() {
    File file = LittleFS.open("/positions.txt", "w");
    if (!file) {
        Serial.println("Failed to save position");
        return;
    }
    for (const Place& place : places) {
        file.print(place.path);
        file.print('\t');
        file.print(place.chapter);
        file.print('\t');
        file.println(place.page);
    }
    file.close();
}

}  // namespace

void loadPlaces() {
    places.clear();
    File file = LittleFS.open("/positions.txt", "r");
    if (!file) {
        return;
    }
    while (file.available()) {
        const String line = file.readStringUntil('\n');
        const int first = line.indexOf('\t');
        const int second = first < 0 ? -1 : line.indexOf('\t', first + 1);
        if (first <= 0 || second < 0) {
            continue;
        }
        Place place;
        place.path = line.substring(0, first);
        place.chapter = static_cast<size_t>(line.substring(first + 1, second).toInt());
        place.page = static_cast<size_t>(line.substring(second + 1).toInt());
        places.push_back(place);
    }
    file.close();
}

void remember(const String& path, size_t chapter, size_t page) {
    for (Place& place : places) {
        if (place.path == path) {
            place.chapter = chapter;
            place.page = page;
            savePlaces();
            return;
        }
    }
    places.push_back(Place{path, chapter, page});
    savePlaces();
}

void savedPlace(const String& path, size_t& chapter, size_t& page) {
    chapter = 0;
    page = 0;
    for (const Place& place : places) {
        if (place.path == path) {
            chapter = place.chapter;
            page = place.page;
            return;
        }
    }
}
