#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>

#include "display.h"
#include "pins.h"

#define MAX_DISPLAY_BUFFER_SIZE 8000
#define MAX_HEIGHT(EPD) \
  (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) \
   ? EPD::HEIGHT \
   : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

GxEPD2_BW<GxEPD2_213_B74, MAX_HEIGHT(GxEPD2_213_B74)> display(
    GxEPD2_213_B74(kEpdCs, kEpdDc, kEpdRst, kEpdBusy));

namespace Display {
    void init() {
        pinMode(kSdCs, OUTPUT);
        digitalWrite(kSdCs, HIGH);
        SPI.begin(kSpiSclk, kSpiMiso, kSpiMosi, kEpdCs);
        display.init(115200);
    }
}