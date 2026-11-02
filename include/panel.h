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

// Built-in font scale for every screen. 1 is 6×8 pixels; 2 is 12×16; and so on.
// Pagination uses the same constant, so changing it reflows each page.
constexpr int kFontSize = 1;
static_assert(kFontSize >= 1, "kFontSize must be at least 1");
constexpr int kCharW = 6 * kFontSize;
constexpr int kLineH = 8 * kFontSize;
constexpr int kMarginX = 5;
constexpr int kMarginY = 10;

int charsPerLine();
int linesPerPage();
bool beginRefresh(bool forceFull);
