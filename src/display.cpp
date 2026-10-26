#include "display.h"

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SPI.h>

// Display control pins
#define EPD_CS   27
#define EPD_DC   14
#define EPD_RST  12
#define EPD_BUSY 13

// SPI pins
#define EPD_SCLK 18
#define EPD_MOSI 23

// GDEM0213B74: 128 x 250, SSD1680
#define MAX_DISPLAY_BUFFER_SIZE 8000
#define MAX_HEIGHT(EPD) \
  (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) \
   ? EPD::HEIGHT \
   : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

GxEPD2_BW<GxEPD2_213_B74,
          MAX_HEIGHT(GxEPD2_213_B74)> display(
  GxEPD2_213_B74(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

void initDisplay() {
    SPI.begin(EPD_SCLK, -1, EPD_MOSI, EPD_CS);
    display.init(115200);
    display.setRotation(0);
}

void displayText(const char* text) {
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(10, 30);
        display.setTextColor(GxEPD_BLACK);
        display.setTextWrap(true);
        display.print(text);
  } while (display.nextPage());

  display.hibernate();
}