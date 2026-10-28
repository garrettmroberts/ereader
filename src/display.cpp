#include "display.h"
#include "epub.h"
#include "paginate.h"
#include "refresh.h"

#include "esp32/rom/tjpgd.h"

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <LittleFS.h>
#include <SPI.h>

#include "card.h"
#include "pins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <vector>

// GDEM0213B74: 128 x 250, SSD1680
#define MAX_DISPLAY_BUFFER_SIZE 8000
#define MAX_HEIGHT(EPD) \
  (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) \
   ? EPD::HEIGHT \
   : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

GxEPD2_BW<GxEPD2_213_B74,
          MAX_HEIGHT(GxEPD2_213_B74)> display(
  GxEPD2_213_B74(kEpdCs, kEpdDc, kEpdRst, kEpdBusy)
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

bool decodeJpeg(const char* path, uint32_t offset, uint32_t length, int maxW, int maxH, int& outW, int& outH, uint8_t*& bits, bool internal) {
    bits = nullptr;
    outW = 0;
    outH = 0;
    if (length < 4 || maxW < 1 || maxH < 1) {
        return false;
    }

    if (!internal) {
        releaseDisplayBus();
    }
    File file = (internal ? LittleFS : bookFs()).open(path, "r");
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

struct CachedCover {
    String path;
    int width = 0;
    int height = 0;
    uint8_t* bits = nullptr;
};

std::vector<CachedCover> covers;

bool findCached(const char* path, int& outW, int& outH, uint8_t*& bits) {
    if (path == nullptr) {
        return false;
    }
    for (const CachedCover& cover : covers) {
        if (cover.path == path && cover.bits != nullptr) {
            outW = cover.width;
            outH = cover.height;
            bits = cover.bits;
            return true;
        }
    }
    return false;
}

void storeCover(const char* path, int width, int height, uint8_t* bits) {
    if (path == nullptr || bits == nullptr) {
        free(bits);
        return;
    }
    if (covers.size() >= 8) {
        free(covers.front().bits);
        covers.erase(covers.begin());
    }
    covers.push_back(CachedCover{String(path), width, height, bits});
}

bool replaceExtension(const char* path, const char* extension, char* out, size_t outSize) {
    const char* slash = strrchr(path, '/');
    const char* name = slash == nullptr ? path : slash + 1;
    const char* dot = strrchr(name, '.');
    if (dot == nullptr || dot == name) {
        return false;
    }
    const size_t stem = static_cast<size_t>(dot - path);
    const size_t extLen = strlen(extension);
    if (stem + extLen + 1 > outSize) {
        return false;
    }
    memcpy(out, path, stem);
    memcpy(out + stem, extension, extLen + 1);
    return true;
}

bool fileSize(const char* path, uint32_t& size) {
    releaseDisplayBus();
    File file = bookFs().open(path, "r");
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return false;
    }
    size = file.size();
    file.close();
    return size >= 4;
}

bool loadCover(const char* epubPath, int maxW, int maxH, int& outW, int& outH, uint8_t*& bits) {
    if (epubPath == nullptr) {
        return false;
    }
    if (findCached(epubPath, outW, outH, bits)) {
        Serial.println("Cover cached");
        return true;
    }

    const uint32_t started = millis();
    char sibling[768];
    const char* extensions[] = {".jpg", ".jpeg"};
    for (const char* extension : extensions) {
        uint32_t size = 0;
        if (replaceExtension(epubPath, extension, sibling, sizeof(sibling)) && fileSize(sibling, size) &&
            decodeJpeg(sibling, 0, size, maxW, maxH, outW, outH, bits, false)) {
            storeCover(epubPath, outW, outH, bits);
            Serial.print("Cover jpg ");
            Serial.print(millis() - started);
            Serial.println(" ms");
            return true;
        }
    }

    EpubCover cover;
    if (!epubFindCover(epubPath, cover)) {
        Serial.print("Cover missing ");
        Serial.print(millis() - started);
        Serial.println(" ms");
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

    const bool ok = decodeJpeg(imagePath, offset, length, maxW, maxH, outW, outH, bits, temporary);
    if (temporary) {
        LittleFS.remove(imagePath);
    }
    if (ok) {
        storeCover(epubPath, outW, outH, bits);
    }
    Serial.print("Cover epub ");
    Serial.print(millis() - started);
    Serial.println(" ms");
    return ok;
}

void bookCaption(const char* path, char* author, size_t authorSize, char* title, size_t titleSize) {
    author[0] = '\0';
    title[0] = '\0';
    if (path == nullptr || path[0] == '\0' || authorSize == 0 || titleSize == 0) {
        return;
    }

    const char* start = path;
    while (*start == '/') {
        start++;
    }
    const char* slash = strchr(start, '/');
    if (slash != nullptr) {
        size_t length = static_cast<size_t>(slash - start);
        if (length >= authorSize) {
            length = authorSize - 1;
        }
        memcpy(author, start, length);
        author[length] = '\0';
    }

    const char* last = strrchr(path, '/');
    const char* file = last == nullptr ? path : last + 1;
    strncpy(title, file, titleSize - 1);
    title[titleSize - 1] = '\0';
    const size_t length = strlen(title);
    if (length >= 5 && strcasecmp(title + length - 5, ".epub") == 0) {
        title[length - 5] = '\0';
    } else if (length >= 4 && strcasecmp(title + length - 4, ".epu") == 0) {
        title[length - 4] = '\0';
    }
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
    char author[64];
    char book[64];
    bookCaption(count == 0 ? nullptr : epubPath, author, sizeof(author), book, sizeof(book));
    const char* title = count == 0 || book[0] == '\0' ? "No books" : book;
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

    // Library moves stay partial. A full refresh here is a multi-second flash.
    if (fullRefresh) {
        beginRefresh(true);
    } else {
        display.setPartialWindow(0, 0, display.width(), display.height());
        Serial.println("Paint: partial");
    }
    const uint32_t painted = millis();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        if (author[0] == '\0') {
            paintText(title, kMarginX, 4, cols > 0 ? cols : 1, 2);
        } else {
            paintText(author, kMarginX, 4, cols > 0 ? cols : 1, 1);
            paintText(title, kMarginX, 4 + kLineH, cols > 0 ? cols : 1, 1);
        }
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

    if (fullRefresh) {
        display.powerOff();
    }

    Serial.print("Paint ");
    Serial.print(millis() - painted);
    Serial.println(" ms");
    Serial.print("Library ");
    Serial.print(index + 1);
    Serial.print("/");
    Serial.print(count);
    Serial.print(" ");
    if (author[0] != '\0') {
        Serial.print(author);
        Serial.print(" / ");
    }
    Serial.println(title);
}
