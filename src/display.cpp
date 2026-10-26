#include "display.h"
#include "paginate.h"

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

namespace {

// Built-in Adafruit GFX font: 6px advance, 8px line height, at text size 1.
constexpr int kTextSize = 1;
constexpr int kCharW = 6 * kTextSize;
constexpr int kLineH = 8 * kTextSize;
constexpr int kMarginX = 5;
constexpr int kMarginY = 10;

int charsPerLine() {
    const int usable = display.width() - kMarginX * 2;
    return usable > 0 ? usable / kCharW : 0;
}

int linesPerPage() {
    const int usable = display.height() - kMarginY * 2;
    return usable > 0 ? usable / kLineH : 0;
}

}  // namespace

void initDisplay() {
    SPI.begin(EPD_SCLK, -1, EPD_MOSI, EPD_CS);
    display.init(115200);
    display.setRotation(0);
}

std::vector<size_t> paginate(const String& text) {
    return pageOffsets(text.c_str(), text.length(), charsPerLine(), linesPerPage());
}

void drawPage(const String& text, size_t offset) {
    const PageLayout page = layoutPage(
        text.c_str(), text.length(), offset, charsPerLine(), linesPerPage());

    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(kTextSize);
        display.setTextWrap(false);

        int y = kMarginY;
        for (const TextLine& line : page.lines) {
            display.setCursor(kMarginX, y);
            for (size_t index = line.begin; index < line.end; index++) {
                const char c = text[index];
                if (c == '\r') {
                    continue;
                }
                display.write(static_cast<uint8_t>(c));
            }
            y += kLineH;
        }
    } while (display.nextPage());

    display.hibernate();
}
