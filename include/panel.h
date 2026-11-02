#pragma once

#include <GxEPD2_BW.h>

#include "pins.h"

// GDEM0213B74: 128 x 250, SSD1680
#define MAX_DISPLAY_BUFFER_SIZE 8000
#define MAX_HEIGHT(EPD) \
  (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) \
   ? EPD::HEIGHT \
   : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

extern GxEPD2_BW<GxEPD2_213_B74, MAX_HEIGHT(GxEPD2_213_B74)> display;

// Adafruit GFX FreeFont point size. Use 5, 6, 7, 8, 9, 12, 18, or 24.
// Sizes below 9 are the same FreeMono and FreeSans faces, converted at that point size.
#define kFontSize 6

// 1 draws FreeMono. 0 draws FreeSans (proportional).
#define kMonospace 1

static_assert(kFontSize == 5 || kFontSize == 6 || kFontSize == 7 || kFontSize == 8 ||
                  kFontSize == 9 || kFontSize == 12 || kFontSize == 18 || kFontSize == 24,
              "kFontSize must be 5, 6, 7, 8, 9, 12, 18, or 24");

// Advances for FreeSans, ASCII 0x20 through 0x7E, in the same order as the GFX font.
constexpr uint8_t kSans5[] = {
    3, 3, 3, 6, 6, 9, 7, 2, 3, 3, 4, 6, 3, 3, 2, 3,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 2, 3, 6, 6, 6, 6,
    10, 7, 7, 7, 7, 6, 6, 8, 7, 3, 5, 7, 6, 8, 7, 8,
    7, 8, 7, 7, 6, 7, 6, 9, 7, 7, 6, 3, 3, 3, 5, 6,
    3, 5, 6, 5, 6, 5, 3, 6, 5, 2, 2, 5, 2, 8, 5, 5,
    6, 6, 3, 5, 3, 5, 5, 7, 5, 5, 5, 3, 2, 3, 5};
constexpr uint8_t kSans6[] = {
    3, 4, 4, 7, 7, 11, 8, 2, 4, 4, 5, 7, 3, 4, 3, 3,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 3, 3, 7, 7, 7, 7,
    12, 8, 8, 9, 8, 8, 7, 9, 9, 3, 6, 8, 7, 10, 9, 9,
    8, 9, 9, 8, 8, 9, 8, 11, 8, 8, 7, 3, 3, 3, 6, 7,
    3, 7, 7, 6, 7, 6, 3, 7, 6, 3, 3, 6, 3, 10, 6, 6,
    7, 7, 4, 6, 3, 6, 6, 9, 6, 6, 6, 4, 3, 4, 6};
constexpr uint8_t kSans7[] = {
    4, 5, 5, 8, 8, 12, 9, 3, 5, 5, 5, 8, 4, 5, 4, 4,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 4, 4, 8, 8, 8, 8,
    14, 9, 9, 10, 10, 9, 8, 11, 10, 4, 7, 9, 8, 12, 10, 11,
    9, 11, 10, 9, 9, 10, 9, 13, 9, 9, 9, 4, 4, 4, 7, 8,
    4, 8, 8, 7, 8, 7, 4, 8, 8, 3, 3, 7, 3, 11, 8, 7,
    8, 8, 5, 7, 4, 8, 7, 10, 7, 7, 7, 5, 4, 5, 7};
constexpr uint8_t kSans8[] = {
    4, 5, 5, 9, 9, 14, 11, 3, 5, 5, 6, 9, 4, 5, 4, 4,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 4, 4, 9, 9, 9, 9,
    16, 11, 11, 11, 11, 10, 10, 12, 12, 4, 8, 11, 9, 14, 12, 13,
    11, 13, 11, 11, 10, 12, 10, 15, 11, 11, 10, 4, 4, 4, 7, 9,
    4, 9, 9, 8, 9, 9, 4, 9, 9, 4, 4, 8, 3, 13, 9, 9,
    9, 9, 5, 8, 4, 9, 8, 12, 8, 8, 8, 5, 4, 5, 8};
constexpr uint8_t kSans9[] = {
    5, 6, 6, 10, 10, 16, 12, 4, 6, 6, 7, 11, 5, 6, 5, 5,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 5, 5, 11, 11, 11, 10,
    18, 12, 12, 13, 13, 11, 11, 14, 13, 5, 10, 12, 10, 15, 13, 14,
    12, 14, 13, 12, 11, 13, 12, 17, 12, 12, 11, 5, 5, 5, 8, 10,
    5, 10, 10, 9, 10, 10, 5, 10, 10, 4, 4, 9, 4, 15, 10, 10,
    10, 10, 6, 9, 5, 10, 9, 13, 9, 9, 9, 6, 4, 6, 9};
constexpr uint8_t kSans12[] = {
    6, 8, 8, 13, 13, 21, 16, 5, 8, 8, 9, 14, 7, 8, 6, 7,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 6, 6, 14, 14, 14, 13,
    24, 16, 16, 17, 17, 15, 14, 18, 17, 7, 13, 16, 14, 20, 18, 19,
    16, 19, 17, 16, 15, 17, 15, 22, 16, 16, 15, 7, 7, 7, 11, 13,
    6, 13, 13, 12, 13, 13, 7, 13, 13, 5, 6, 12, 5, 19, 13, 13,
    13, 13, 8, 12, 7, 13, 12, 17, 11, 11, 12, 8, 6, 8, 12};
constexpr uint8_t kSans18[] = {
    9, 12, 12, 19, 19, 31, 23, 7, 12, 12, 14, 20, 10, 12, 9, 10,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 9, 9, 20, 20, 20, 19,
    36, 23, 23, 25, 24, 22, 21, 27, 25, 10, 18, 24, 20, 30, 26, 27,
    23, 27, 25, 23, 22, 25, 23, 33, 23, 24, 22, 10, 10, 10, 16, 19,
    9, 19, 20, 18, 20, 19, 10, 19, 19, 8, 9, 18, 7, 28, 19, 19,
    20, 20, 12, 17, 10, 19, 17, 25, 17, 17, 17, 12, 9, 12, 18};
constexpr uint8_t kSans24[] = {
    12, 16, 16, 26, 26, 42, 31, 9, 16, 16, 18, 27, 13, 16, 12, 13,
    26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 12, 12, 27, 27, 27, 26,
    48, 31, 31, 33, 33, 30, 28, 36, 34, 13, 25, 32, 26, 40, 34, 37,
    31, 37, 33, 31, 30, 34, 30, 44, 31, 32, 29, 13, 13, 13, 22, 26,
    12, 26, 26, 24, 26, 25, 13, 26, 25, 10, 11, 24, 10, 38, 25, 25,
    26, 26, 16, 23, 13, 25, 23, 34, 22, 22, 23, 16, 12, 16, 24};
static_assert(sizeof(kSans5) == 95 && sizeof(kSans6) == 95 && sizeof(kSans7) == 95 &&
                  sizeof(kSans8) == 95 && sizeof(kSans9) == 95 && sizeof(kSans12) == 95 &&
                  sizeof(kSans18) == 95 && sizeof(kSans24) == 95,
              "FreeSans advance tables");

#if kFontSize == 5
constexpr int kMonoAdvance = 6;
constexpr int kMonoLineH = 10;
constexpr int kMonoAscent = 6;
constexpr int kSansLineH = 12;
constexpr int kSansAscent = 7;
constexpr const uint8_t* kSansAdvance = kSans5;
#elif kFontSize == 6
constexpr int kMonoAdvance = 7;
constexpr int kMonoLineH = 12;
constexpr int kMonoAscent = 7;
constexpr int kSansLineH = 14;
constexpr int kSansAscent = 9;
constexpr const uint8_t* kSansAdvance = kSans6;
#elif kFontSize == 7
constexpr int kMonoAdvance = 8;
constexpr int kMonoLineH = 14;
constexpr int kMonoAscent = 8;
constexpr int kSansLineH = 16;
constexpr int kSansAscent = 10;
constexpr const uint8_t* kSansAdvance = kSans7;
#elif kFontSize == 8
constexpr int kMonoAdvance = 10;
constexpr int kMonoLineH = 16;
constexpr int kMonoAscent = 10;
constexpr int kSansLineH = 19;
constexpr int kSansAscent = 11;
constexpr const uint8_t* kSansAdvance = kSans8;
#elif kFontSize == 9
constexpr int kMonoAdvance = 11;
constexpr int kMonoLineH = 18;
constexpr int kMonoAscent = 11;
constexpr int kSansLineH = 22;
constexpr int kSansAscent = 13;
constexpr const uint8_t* kSansAdvance = kSans9;
#elif kFontSize == 12
constexpr int kMonoAdvance = 14;
constexpr int kMonoLineH = 24;
constexpr int kMonoAscent = 15;
constexpr int kSansLineH = 29;
constexpr int kSansAscent = 17;
constexpr const uint8_t* kSansAdvance = kSans12;
#elif kFontSize == 18
constexpr int kMonoAdvance = 21;
constexpr int kMonoLineH = 35;
constexpr int kMonoAscent = 22;
constexpr int kSansLineH = 42;
constexpr int kSansAscent = 26;
constexpr const uint8_t* kSansAdvance = kSans18;
#elif kFontSize == 24
constexpr int kMonoAdvance = 28;
constexpr int kMonoLineH = 47;
constexpr int kMonoAscent = 30;
constexpr int kSansLineH = 56;
constexpr int kSansAscent = 34;
constexpr const uint8_t* kSansAdvance = kSans24;
#endif

#if kMonospace
constexpr int kCharW = kMonoAdvance;
constexpr int kLineH = kMonoLineH;
constexpr int kFontAscent = kMonoAscent;
#else
constexpr int kCharW = kSansAdvance[0];
constexpr int kLineH = kSansLineH;
constexpr int kFontAscent = kSansAscent;
#endif

constexpr int kMarginX = 5;
constexpr int kMarginY = 10;

inline int glyphAdvance(unsigned char c) {
    if (c < 0x20 || c > 0x7E) {
        return 0;
    }
#if kMonospace
    // FreeMono 7pt is fixed-width except for "k", which advances 9 pixels.
    return kFontSize == 7 && c == 'k' ? 9 : kCharW;
#else
    return kSansAdvance[c - 0x20];
#endif
}

inline int textWidth(const char* text) {
    int width = 0;
    if (text == nullptr) {
        return 0;
    }
    for (const auto* p = reinterpret_cast<const unsigned char*>(text); *p != 0; ++p) {
        width += glyphAdvance(*p);
    }
    return width;
}

constexpr int textBaseline(int top) {
    return top + kFontAscent;
}

int charsPerLine();
int linesPerPage();
int textLineWidth();
void applyTextStyle();
bool beginRefresh(bool forceFull);
