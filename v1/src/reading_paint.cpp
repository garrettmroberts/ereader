#include "reading.h"

#include "paginate.h"
#include "panel.h"

void ReadingPage::paint(size_t offset, bool fullRefresh) {
    const PageLayout page =
        layoutFitted(text.c_str(), text.length(), offset, textLineWidth(), linesPerPage(), glyphAdvance);

    beginRefresh(fullRefresh);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        applyTextStyle();

        int y = kMarginY;
        for (const TextLine& line : page.lines) {
            display.setCursor(kMarginX, textBaseline(y));
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

    display.powerOff();
}
