#include "display.h"
#include "panel.h"
#include "paginate.h"
#include "refresh.h"

#include <Arduino.h>
#include <SPI.h>

#ifndef UNIT_TEST
#if kMonospace
#if kFontSize == 5
#include "fonts/FreeMono5pt7b.h"
#define READER_FONT FreeMono5pt7b
#elif kFontSize == 6
#include "fonts/FreeMono6pt7b.h"
#define READER_FONT FreeMono6pt7b
#elif kFontSize == 7
#include "fonts/FreeMono7pt7b.h"
#define READER_FONT FreeMono7pt7b
#elif kFontSize == 8
#include "fonts/FreeMono8pt7b.h"
#define READER_FONT FreeMono8pt7b
#elif kFontSize == 9
#include <Fonts/FreeMono9pt7b.h>
#define READER_FONT FreeMono9pt7b
#elif kFontSize == 12
#include <Fonts/FreeMono12pt7b.h>
#define READER_FONT FreeMono12pt7b
#elif kFontSize == 18
#include <Fonts/FreeMono18pt7b.h>
#define READER_FONT FreeMono18pt7b
#elif kFontSize == 24
#include <Fonts/FreeMono24pt7b.h>
#define READER_FONT FreeMono24pt7b
#endif
#else
#if kFontSize == 5
#include "fonts/FreeSans5pt7b.h"
#define READER_FONT FreeSans5pt7b
#elif kFontSize == 6
#include "fonts/FreeSans6pt7b.h"
#define READER_FONT FreeSans6pt7b
#elif kFontSize == 7
#include "fonts/FreeSans7pt7b.h"
#define READER_FONT FreeSans7pt7b
#elif kFontSize == 8
#include "fonts/FreeSans8pt7b.h"
#define READER_FONT FreeSans8pt7b
#elif kFontSize == 9
#include <Fonts/FreeSans9pt7b.h>
#define READER_FONT FreeSans9pt7b
#elif kFontSize == 12
#include <Fonts/FreeSans12pt7b.h>
#define READER_FONT FreeSans12pt7b
#elif kFontSize == 18
#include <Fonts/FreeSans18pt7b.h>
#define READER_FONT FreeSans18pt7b
#elif kFontSize == 24
#include <Fonts/FreeSans24pt7b.h>
#define READER_FONT FreeSans24pt7b
#endif
#endif
#endif

GxEPD2_BW<GxEPD2_213_B74, MAX_HEIGHT(GxEPD2_213_B74)> display(
    GxEPD2_213_B74(kEpdCs, kEpdDc, kEpdRst, kEpdBusy));

namespace {

int paintsSinceFull = kRefreshStart;

}  // namespace

int charsPerLine() {
    const int usable = display.width() - kMarginX * 2;
    return usable > 0 ? usable / kCharW : 0;
}

int linesPerPage() {
    const int usable = display.height() - kMarginY * 2;
    return usable > 0 ? usable / kLineH : 0;
}

int textLineWidth() {
    const int usable = display.width() - kMarginX * 2;
    return usable > 0 ? usable : 0;
}

void applyTextStyle() {
#ifndef UNIT_TEST
    display.setFont(&READER_FONT);
#endif
    display.setTextSize(1);
    display.setTextWrap(false);
}

bool beginRefresh(bool forceFull) {
    const RefreshPlan plan = chooseRefresh(paintsSinceFull, forceFull);
    paintsSinceFull = plan.paintsSinceFull;
    if (plan.full) {
        display.setFullWindow();
        if (forceFull) {
            Serial.println("Paint: full");
        } else {
            Serial.print("Paint: full (every ");
            Serial.print(kFullEvery);
            Serial.println(")");
        }
    } else {
        display.setPartialWindow(0, 0, display.width(), display.height());
        Serial.println("Paint: partial");
    }
    return plan.full;
}

void initDisplay() {
    // MISO stays attached so the microSD reader can share this bus.
    pinMode(kSdCs, OUTPUT);
    digitalWrite(kSdCs, HIGH);
    SPI.begin(kSpiSclk, kSpiMiso, kSpiMosi, kEpdCs);
    display.init(115200);
    display.setRotation(0);
    digitalWrite(kEpdCs, HIGH);
}

std::vector<size_t> paginate(const String& text) {
    return pageOffsetsFitted(text.c_str(), text.length(), textLineWidth(), linesPerPage(), glyphAdvance);
}

#ifdef UNIT_TEST
void displayResetForTest() {
    paintsSinceFull = kRefreshStart;
}
#endif
