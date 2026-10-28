#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#define GxEPD_WHITE 0xFFFF
#define GxEPD_BLACK 0x0000

class GxEPD2_213_B74 {
public:
    static const int WIDTH = 128;
    static const int HEIGHT = 250;

    GxEPD2_213_B74(int, int, int, int) {}
};

struct HostGlyph {
    int x = 0;
    int y = 0;
    char c = 0;
};

struct HostPixel {
    int x = 0;
    int y = 0;
    uint16_t color = 0;
};

struct HostPanel {
    std::vector<HostGlyph> glyphs;
    std::vector<HostPixel> pixels;
    std::string printed;
    int cursorX = 0;
    int cursorY = 0;
    int printX = 0;
    int printY = 0;
    bool fullWindow = false;
    bool partialWindow = false;
    bool poweredOff = false;
    int fillCount = 0;
    uint16_t fillColor = 0;
    unsigned long initBaud = 0;
    int rotation = -1;

    void reset() { *this = {}; }
};

inline HostPanel hostPanel;

template <typename EPD, int pageHeight>
class GxEPD2_BW {
public:
    explicit GxEPD2_BW(EPD) {}

    int width() const { return EPD::WIDTH; }
    int height() const { return EPD::HEIGHT; }

    void init(unsigned long baud) { hostPanel.initBaud = baud; }
    void setRotation(int value) { hostPanel.rotation = value; }

    void setFullWindow() {
        hostPanel.fullWindow = true;
        hostPanel.partialWindow = false;
    }

    void setPartialWindow(int, int, int, int) {
        hostPanel.partialWindow = true;
        hostPanel.fullWindow = false;
    }

    void firstPage() {}
    bool nextPage() { return false; }

    void fillScreen(uint16_t color) {
        hostPanel.fillCount++;
        hostPanel.fillColor = color;
    }

    void setTextColor(uint16_t) {}
    void setTextSize(int) {}
    void setTextWrap(bool) {}

    void setCursor(int x, int y) {
        hostPanel.cursorX = x;
        hostPanel.cursorY = y;
    }

    size_t write(uint8_t c) {
        hostPanel.glyphs.push_back(HostGlyph{hostPanel.cursorX, hostPanel.cursorY, static_cast<char>(c)});
        hostPanel.cursorX += 6;
        return 1;
    }

    void print(const char* text) {
        hostPanel.printX = hostPanel.cursorX;
        hostPanel.printY = hostPanel.cursorY;
        hostPanel.printed += text == nullptr ? "" : text;
    }

    void powerOff() { hostPanel.poweredOff = true; }

    void drawPixel(int x, int y, uint16_t color) {
        hostPanel.pixels.push_back(HostPixel{x, y, color});
    }
};
