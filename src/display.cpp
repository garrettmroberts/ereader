#include "display.h"
#include "epub.h"
#include "paginate.h"
#include "refresh.h"

#include "esp32/rom/tjpgd.h"

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <LittleFS.h>
#include <SPI.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Display control pins
#define EPD_CS   27
#define EPD_DC   26
#define EPD_RST  25
#define EPD_BUSY 13

// SPI pins
#define EPD_SCLK 14
#define EPD_MOSI 12

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

int paintsSinceFull = kRefreshStart;

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

}  // namespace

void initDisplay() {
    SPI.begin(EPD_SCLK, -1, EPD_MOSI, EPD_CS);
    display.init(115200);
    display.setRotation(0);
}

std::vector<size_t> paginate(const String& text) {
    return pageOffsets(text.c_str(), text.length(), charsPerLine(), linesPerPage());
}

void drawPage(const String& text, size_t offset, bool fullRefresh) {
    const PageLayout page = layoutPage(
        text.c_str(), text.length(), offset, charsPerLine(), linesPerPage());

    beginRefresh(fullRefresh);
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

    display.powerOff();
}

namespace {

struct JpegSource {
    File* file = nullptr;
    uint32_t start = 0;
    uint32_t pos = 0;
    uint32_t length = 0;
    uint8_t* bits = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
};

}  // namespace

extern "C" unsigned int jpegRead(JDEC* jd, unsigned char* buf, unsigned int ndata) {
    auto* src = static_cast<JpegSource*>(jd->device);
    if (src->pos >= src->length || ndata == 0) {
        return 0;
    }
    if (src->pos + ndata > src->length) {
        ndata = src->length - src->pos;
    }
    if (!src->file->seek(src->start + src->pos)) {
        return 0;
    }
    if (buf == nullptr) {
        src->pos += ndata;
        return ndata;
    }
    const size_t got = src->file->read(buf, ndata);
    src->pos += got;
    return got;
}

extern "C" unsigned int jpegWrite(JDEC* jd, void* bitmap, JRECT* rect) {
    auto* src = static_cast<JpegSource*>(jd->device);
    const auto* px = static_cast<const uint8_t*>(bitmap);
    const int span = rect->right - rect->left + 1;
    for (int y = rect->top; y <= rect->bottom; y++) {
        for (int x = 0; x < span; x++) {
            const int dx = rect->left + x;
            const uint8_t r = px[0];
            const uint8_t g = px[1];
            const uint8_t b = px[2];
            px += 3;
            if (dx < 0 || dx >= src->width || y < 0 || y >= src->height) {
                continue;
            }
            const int lum = (static_cast<int>(r) * 30 + static_cast<int>(g) * 59 + static_cast<int>(b) * 11) / 100;
            if (lum < 140) {
                src->bits[y * src->stride + (dx >> 3)] |= static_cast<uint8_t>(0x80 >> (dx & 7));
            }
        }
    }
    return 1;
}

namespace {

bool decodeJpeg(const char* path, uint32_t offset, uint32_t length, int maxW, int maxH, int& outW, int& outH, uint8_t*& bits) {
    bits = nullptr;
    outW = 0;
    outH = 0;
    if (length < 4 || maxW < 1 || maxH < 1) {
        return false;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }

    JpegSource src;
    src.file = &file;
    src.start = offset;
    src.length = length;

    static uint8_t pool[16 * 1024];
    JDEC decoder;
    const JRESULT prepared = jd_prepare(&decoder, jpegRead, pool, sizeof(pool), &src);
    if (prepared != JDR_OK || decoder.width == 0 || decoder.height == 0) {
        file.close();
        return false;
    }

    int scale = 0;
    while (scale < 3) {
        const int w = decoder.width >> scale;
        const int h = decoder.height >> scale;
        if (w <= maxW && h <= maxH) {
            break;
        }
        scale++;
    }

    outW = decoder.width >> scale;
    outH = decoder.height >> scale;
    if (outW < 1) {
        outW = 1;
    }
    if (outH < 1) {
        outH = 1;
    }

    src.width = outW;
    src.height = outH;
    src.stride = (outW + 7) / 8;
    const size_t bytes = static_cast<size_t>(src.stride) * static_cast<size_t>(outH);
    if (bytes == 0 || bytes > 32 * 1024) {
        file.close();
        return false;
    }
    src.bits = static_cast<uint8_t*>(calloc(bytes, 1));
    if (src.bits == nullptr) {
        file.close();
        return false;
    }

    const JRESULT decoded = jd_decomp(&decoder, jpegWrite, static_cast<uint8_t>(scale));
    file.close();
    if (decoded != JDR_OK) {
        free(src.bits);
        return false;
    }
    bits = src.bits;
    return true;
}

bool loadCover(const char* epubPath, int maxW, int maxH, int& outW, int& outH, uint8_t*& bits) {
    EpubCover cover;
    if (!epubFindCover(epubPath, cover)) {
        return false;
    }

    const char* imagePath = epubPath;
    uint32_t offset = cover.offset;
    uint32_t length = cover.size;
    bool temporary = false;
    if (cover.method != 0) {
        imagePath = "/cover.tmp";
        if (!epubExtract(epubPath, cover, imagePath)) {
            return false;
        }
        File temp = LittleFS.open(imagePath, "r");
        if (!temp) {
            LittleFS.remove(imagePath);
            return false;
        }
        offset = 0;
        length = temp.size();
        temp.close();
        temporary = true;
    }

    const bool ok = decodeJpeg(imagePath, offset, length, maxW, maxH, outW, outH, bits);
    if (temporary) {
        LittleFS.remove(imagePath);
    }
    return ok;
}

const char* fileName(const char* path) {
    if (path == nullptr) {
        return "";
    }
    const char* slash = strrchr(path, '/');
    return slash == nullptr ? path : slash + 1;
}

int paintText(const char* text, int x, int y, int maxCols, int maxLines) {
    int line = 0;
    int col = 0;
    display.setCursor(x, y);
    for (const char* p = text; *p != '\0' && line < maxLines; p++) {
        if (col >= maxCols) {
            line++;
            col = 0;
            if (line >= maxLines) {
                break;
            }
            display.setCursor(x, y + line * kLineH);
        }
        display.write(static_cast<uint8_t>(*p));
        col++;
    }
    return y + (line + 1) * kLineH;
}

void paintCover(int x0, int y0, const uint8_t* bits, int width, int height) {
    const int stride = (width + 7) / 8;
    for (int y = 0; y < height; y++) {
        const int dy = y0 + y;
        if (dy < 0 || dy >= display.height()) {
            continue;
        }
        for (int x = 0; x < width; x++) {
            const int dx = x0 + x;
            if (dx < 0 || dx >= display.width()) {
                continue;
            }
            if ((bits[y * stride + (x >> 3)] & static_cast<uint8_t>(0x80 >> (x & 7))) != 0) {
                display.drawPixel(dx, dy, GxEPD_BLACK);
            }
        }
    }
}

}  // namespace

void drawLibrary(const char* epubPath, size_t index, size_t count, bool fullRefresh) {
    const char* title = count == 0 ? "No books" : fileName(epubPath);
    const int cols = charsPerLine();
    const int titleBottom = 4 + 2 * kLineH;
    const int footerTop = display.height() - kMarginY - kLineH;
    const int maxW = display.width() - 4;
    const int maxH = footerTop - titleBottom - 4;

    int imageW = 0;
    int imageH = 0;
    uint8_t* bits = nullptr;
    const bool haveCover = count > 0 && epubPath != nullptr && loadCover(epubPath, maxW, maxH, imageW, imageH, bits);

    char label[16];
    if (count == 0) {
        label[0] = '\0';
    } else {
        snprintf(label, sizeof(label), "%u/%u", static_cast<unsigned>(index + 1), static_cast<unsigned>(count));
    }

    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(kTextSize);
    display.setTextWrap(false);

    beginRefresh(fullRefresh);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        paintText(title, kMarginX, 4, cols > 0 ? cols : 1, 2);
        if (haveCover && bits != nullptr) {
            const int x = (display.width() - imageW) / 2;
            const int y = titleBottom + (maxH - imageH) / 2;
            paintCover(x, y, bits, imageW, imageH);
        }
        if (label[0] != '\0') {
            const int labelW = static_cast<int>(strlen(label)) * kCharW;
            display.setCursor((display.width() - labelW) / 2, footerTop);
            display.print(label);
        }
    } while (display.nextPage());

    display.powerOff();
    free(bits);

    Serial.print("Library ");
    Serial.print(index + 1);
    Serial.print("/");
    Serial.print(count);
    Serial.print(" ");
    Serial.println(title);
}
