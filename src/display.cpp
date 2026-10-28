#include "display.h"
#include "panel.h"
#include "paginate.h"
#include "refresh.h"

#include <Arduino.h>
#include <SPI.h>

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
    return pageOffsets(text.c_str(), text.length(), charsPerLine(), linesPerPage());
}

#ifdef UNIT_TEST
void displayResetForTest() {
    paintsSinceFull = kRefreshStart;
}
#endif
