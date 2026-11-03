#include <LittleFS.h>

#include "card.h"

namespace Card {
    void init() {
        if (!LittleFS.begin()) {
            Serial.println("Failed to mount LittleFS");
            return;
        }
    }
}