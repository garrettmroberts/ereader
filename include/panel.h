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

constexpr int kTextSize = 1;
constexpr int kCharW = 6 * kTextSize;
constexpr int kLineH = 8 * kTextSize;
constexpr int kMarginX = 5;
constexpr int kMarginY = 10;

int charsPerLine();
int linesPerPage();
bool beginRefresh(bool forceFull);
