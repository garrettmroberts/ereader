#include "card.h"
#include "display.h"
#include "epub.h"
#include "library.h"
#include "panel.h"
#include "reading.h"

#include <unity.h>

#include <string>

int releases = 0;

FS& bookFs() {
    return CardFs;
}

void releaseDisplayBus() {
    releases++;
}

namespace {

enum class Extract { Fail, Empty, Write };

bool reportCover = false;
EpubCover scripted{};
Extract extract = Extract::Fail;
std::string extractBytes;

}  // namespace

bool epubFindCover(const char*, EpubCover& cover) {
    if (!reportCover) {
        return false;
    }
    cover = scripted;
    return true;
}

bool epubExtract(const char*, const EpubCover&, const char* dest) {
    if (extract == Extract::Fail) {
        return false;
    }
    if (extract == Extract::Write) {
        hostFs().files[dest] = extractBytes;
    }
    return true;
}

#include "../../src/paginate.cpp"
#include "../../src/display.cpp"
#include "../../src/reading_paint.cpp"
#include "../../src/library_paint.cpp"

namespace {

ReadingPage reading;
LibraryPage library;

std::string tjpg(unsigned width, unsigned height, uint8_t red, uint8_t green, uint8_t blue) {
    std::string out = "TJPG";
    out.push_back(static_cast<char>(width & 0xff));
    out.push_back(static_cast<char>((width >> 8) & 0xff));
    out.push_back(static_cast<char>(height & 0xff));
    out.push_back(static_cast<char>((height >> 8) & 0xff));
    const size_t pixels = static_cast<size_t>(width) * height;
    if (pixels > 8192) {
        return out;
    }
    for (size_t i = 0; i < pixels; i++) {
        out.push_back(static_cast<char>(red));
        out.push_back(static_cast<char>(green));
        out.push_back(static_cast<char>(blue));
    }
    return out;
}

std::string fitLine(const std::string& text) {
    const int maxWidth = display.width() - kMarginX * 2;
    std::string out;
    int used = 0;
    for (unsigned char c : text) {
        const int advance = glyphAdvance(c);
        if (!out.empty() && used + advance > maxWidth) {
            break;
        }
        used += advance;
        out.push_back(static_cast<char>(c));
    }
    return out;
}

std::string textAt(int y) {
    std::string out;
    for (const HostGlyph& glyph : hostPanel.glyphs) {
        if (glyph.y == y) {
            out.push_back(glyph.c);
        }
    }
    return out;
}

int coverX(int imageW) {
    return (display.width() - imageW) / 2;
}

int coverY(int imageH) {
    const int titleBottom = 4 + 2 * kLineH;
    const int footerTop = display.height() - kMarginY - kLineH;
    const int maxH = footerTop - titleBottom - 4;
    return titleBottom + (maxH - imageH) / 2;
}

void showBook(const char* path) {
    library.books.clear();
    library.books.push_back(path);
    library.pick = 0;
    hostPanel.reset();
    Serial.out.clear();
}

}  // namespace

extern "C" void setUp(void) {
    hostFs().reset();
    hostCard().reset();
    Serial.out.clear();
    hostPanel.reset();
    hostSpi.reset();
    hostJpeg = {};
    libraryResetForTest();
    displayResetForTest();
    releases = 0;
    reportCover = false;
    scripted = {};
    extract = Extract::Fail;
    extractBytes.clear();
    reading = ReadingPage();
    library = LibraryPage();
}

extern "C" void tearDown(void) {
    libraryResetForTest();
}

extern "C" void test_panel_geometry_and_bus_init(void) {
    hostResetPins();
    hostSetPin(kSdCs, LOW);
    initDisplay();

    TEST_ASSERT_EQUAL_INT(HIGH, hostPins[kSdCs]);
    TEST_ASSERT_EQUAL_INT(HIGH, hostPins[kEpdCs]);
    TEST_ASSERT_EQUAL_INT(1, hostSpi.begins);
    TEST_ASSERT_EQUAL_INT(kSpiSclk, hostSpi.sclk);
    TEST_ASSERT_EQUAL_INT(kSpiMiso, hostSpi.miso);
    TEST_ASSERT_EQUAL_INT(kSpiMosi, hostSpi.mosi);
    TEST_ASSERT_EQUAL_INT(kEpdCs, hostSpi.ss);
    TEST_ASSERT_EQUAL_INT(115200, hostPanel.initBaud);
    TEST_ASSERT_EQUAL_INT(0, hostPanel.rotation);

    bool armedSd = false;
    for (const HostPinMode& mode : hostPinModes) {
        if (mode.pin == kSdCs && mode.mode == OUTPUT) {
            armedSd = true;
        }
    }
    TEST_ASSERT_TRUE(armedSd);

    TEST_ASSERT_EQUAL_INT((display.width() - kMarginX * 2) / kCharW, charsPerLine());
    TEST_ASSERT_EQUAL_INT((display.height() - kMarginY * 2) / kLineH, linesPerPage());
    const std::vector<size_t> pages = paginate(String("hello"));
    TEST_ASSERT_EQUAL_INT(1, pages.size());
    TEST_ASSERT_EQUAL_INT(0, pages[0]);
    TEST_ASSERT_EQUAL_INT(0, paginate(String()).size());
}

extern "C" void test_refresh_schedule_and_reading_paint(void) {
    reading.text = "abc";
    reading.paint(0, false);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Paint: full (every 3)"));
    TEST_ASSERT_TRUE(hostPanel.fullWindow);
    TEST_ASSERT_TRUE(hostPanel.poweredOff);
    TEST_ASSERT_EQUAL_INT(GxEPD_WHITE, hostPanel.fillColor);

    hostPanel.reset();
    Serial.out.clear();
    reading.paint(0, false);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Paint: partial"));
    TEST_ASSERT_TRUE(hostPanel.partialWindow);
    TEST_ASSERT_TRUE(hostPanel.poweredOff);

    hostPanel.reset();
    Serial.out.clear();
    reading.paint(0, false);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Paint: partial"));

    hostPanel.reset();
    Serial.out.clear();
    reading.paint(0, false);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Paint: full (every 3)"));

    hostPanel.reset();
    Serial.out.clear();
    reading.paint(0, true);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Paint: full\n"));
    TEST_ASSERT_NULL(strstr(Serial.out.c_str(), "every"));
    TEST_ASSERT_TRUE(hostPanel.poweredOff);
}

extern "C" void test_reading_paint_skips_carriage_returns(void) {
    reading.text = "hi\rthere\nnext";
    reading.paint(0, true);
    TEST_ASSERT_EQUAL_STRING("hithere", textAt(textBaseline(kMarginY)).c_str());
    TEST_ASSERT_EQUAL_STRING("next", textAt(textBaseline(kMarginY + kLineH)).c_str());
    TEST_ASSERT_EQUAL_INT(1, hostPanel.fillCount);

    hostPanel.reset();
    reading.paint(9, true);
    TEST_ASSERT_EQUAL_STRING("next", textAt(textBaseline(kMarginY)).c_str());
    TEST_ASSERT_TRUE(textAt(textBaseline(kMarginY + kLineH)).empty());

    hostPanel.reset();
    reading.text = "";
    reading.paint(0, true);
    TEST_ASSERT_TRUE(hostPanel.glyphs.empty());
    TEST_ASSERT_EQUAL_INT(1, hostPanel.fillCount);
    TEST_ASSERT_TRUE(hostPanel.poweredOff);
}

extern "C" void test_library_partial_does_not_spend_a_refresh(void) {
    library.books.push_back("/Austen/pride.epub");
    library.paint(false);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Paint: partial"));
    TEST_ASSERT_TRUE(hostPanel.partialWindow);
    TEST_ASSERT_FALSE(hostPanel.poweredOff);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Library 1/1 Austen / pride"));

    Serial.out.clear();
    reading.text = "abc";
    reading.paint(0, false);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Paint: full (every 3)"));

    displayResetForTest();
    hostPanel.reset();
    Serial.out.clear();
    library.paint(true);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Paint: full\n"));
    TEST_ASSERT_NULL(strstr(Serial.out.c_str(), "every"));
    TEST_ASSERT_TRUE(hostPanel.poweredOff);

    Serial.out.clear();
    reading.paint(0, false);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Paint: partial"));
}

extern "C" void test_library_caption_author_title_and_index(void) {
    library.paint(true);
    TEST_ASSERT_EQUAL_STRING("No books", textAt(textBaseline(4)).c_str());
    TEST_ASSERT_TRUE(hostPanel.printed.empty());
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Library 1/0 No books"));
    TEST_ASSERT_EQUAL_INT(0, releases);

    showBook("/Austen/pride.epub");
    library.paint(true);
    TEST_ASSERT_EQUAL_STRING("Austen", textAt(textBaseline(4)).c_str());
    TEST_ASSERT_EQUAL_STRING("pride", textAt(textBaseline(4 + kLineH)).c_str());
    TEST_ASSERT_EQUAL_STRING("1/1", hostPanel.printed.c_str());
    const int footer = display.height() - kMarginY - kLineH;
    TEST_ASSERT_EQUAL_INT(textBaseline(footer), hostPanel.printY);
    TEST_ASSERT_EQUAL_INT((display.width() - textWidth("1/1")) / 2, hostPanel.printX);

    showBook("/Krakauer/BOOK1.EPU");
    library.paint(true);
    TEST_ASSERT_EQUAL_STRING("Krakauer", textAt(textBaseline(4)).c_str());
    TEST_ASSERT_EQUAL_STRING("BOOK1", textAt(textBaseline(4 + kLineH)).c_str());

    showBook("/A/notes.epub.bak");
    library.paint(true);
    TEST_ASSERT_EQUAL_STRING(fitLine("notes.epub.bak").c_str(), textAt(textBaseline(4 + kLineH)).c_str());

    std::string word;
    for (char c = 'a'; c <= 'z'; c++) {
        word.push_back(c);
    }
    showBook((word + ".epub").c_str());
    library.paint(true);
    const std::string first = fitLine(word);
    const std::string second = fitLine(word.substr(first.size()));
    TEST_ASSERT_EQUAL_STRING(first.c_str(), textAt(textBaseline(4)).c_str());
    TEST_ASSERT_EQUAL_STRING(second.c_str(), textAt(textBaseline(4 + kLineH)).c_str());

    showBook("");
    library.paint(true);
    TEST_ASSERT_EQUAL_STRING("No books", textAt(textBaseline(4)).c_str());
    TEST_ASSERT_EQUAL_STRING("1/1", hostPanel.printed.c_str());

    const std::string author(80, 'Z');
    showBook(("/" + author + "/book.epub").c_str());
    library.paint(true);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), std::string(63, 'Z').c_str()));
    TEST_ASSERT_NULL(strstr(Serial.out.c_str(), std::string(64, 'Z').c_str()));
    TEST_ASSERT_EQUAL_STRING("book", textAt(textBaseline(4 + kLineH)).c_str());
}

extern "C" void test_sidecar_cover_is_scaled_cached_and_evicted(void) {
    hostJpeg.exerciseSkip = 2;
    hostCard().files["/Austen/pride.jpg"] = "abc";
    hostCard().files["/Austen/pride.jpeg"] = "XX" + tjpg(1, 1, 0, 0, 0);
    showBook("/Austen/pride.epub");
    library.paint(true);

    TEST_ASSERT_EQUAL_INT(1, hostJpeg.prepares);
    TEST_ASSERT_EQUAL_INT(1, hostPanel.pixels.size());
    TEST_ASSERT_EQUAL_INT(coverX(1), hostPanel.pixels[0].x);
    TEST_ASSERT_EQUAL_INT(coverY(1), hostPanel.pixels[0].y);
    TEST_ASSERT_EQUAL_INT(GxEPD_BLACK, hostPanel.pixels[0].color);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover jpg"));
    TEST_ASSERT_NULL(strstr(Serial.out.c_str(), "Cover cached"));
    TEST_ASSERT_EQUAL_INT(3, releases);

    hostJpeg.exerciseSkip = 0;
    hostPanel.reset();
    Serial.out.clear();
    releases = 0;
    library.paint(false);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover cached"));
    TEST_ASSERT_EQUAL_INT(0, releases);
    TEST_ASSERT_EQUAL_INT(1, hostPanel.pixels.size());
    TEST_ASSERT_FALSE(hostPanel.poweredOff);

    libraryResetForTest();
    hostCard().reset();
    hostJpeg = {};
    hostCard().files["/A/wide.jpg"] = tjpg(300, 16, 0, 0, 0);
    showBook("/A/wide.epub");
    library.paint(true);
    TEST_ASSERT_EQUAL_INT(75 * 4, hostPanel.pixels.size());
    TEST_ASSERT_EQUAL_INT(coverX(75), hostPanel.pixels[0].x);
    TEST_ASSERT_EQUAL_INT(coverY(4), hostPanel.pixels[0].y);

    libraryResetForTest();
    hostCard().reset();
    hostJpeg = {};
    hostCard().files["/A/white.jpg"] = tjpg(2, 2, 255, 255, 255);
    showBook("/A/white.epub");
    library.paint(true);
    TEST_ASSERT_EQUAL_INT(0, hostPanel.pixels.size());
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover jpg"));

    libraryResetForTest();
    hostCard().reset();
    hostJpeg = {};
    hostCard().root.push_back(HostRootEntry{"/A/book.jpg", true, {}});
    hostCard().rootSet = true;
    hostCard().files["/A/book.jpeg"] = tjpg(1, 1, 0, 0, 0);
    showBook("/A/book.epub");
    library.paint(true);
    TEST_ASSERT_EQUAL_INT(1, hostJpeg.prepares);
    TEST_ASSERT_EQUAL_INT(1, hostPanel.pixels.size());

    libraryResetForTest();
    hostCard().reset();
    hostJpeg = {};
    for (int i = 0; i < 9; i++) {
        const std::string path = "/B/b" + std::to_string(i) + ".epub";
        hostCard().files["/B/b" + std::to_string(i) + ".jpg"] = tjpg(1, 1, 0, 0, 0);
        showBook(path.c_str());
        library.paint(false);
    }
    hostJpeg.prepares = 0;
    showBook("/B/b0.epub");
    library.paint(false);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover jpg"));
    TEST_ASSERT_NULL(strstr(Serial.out.c_str(), "Cover cached"));
    TEST_ASSERT_EQUAL_INT(1, hostJpeg.prepares);

    hostJpeg.prepares = 0;
    showBook("/B/b8.epub");
    library.paint(false);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover cached"));
    TEST_ASSERT_EQUAL_INT(0, hostJpeg.prepares);
}

extern "C" void test_rejected_covers_are_not_cached(void) {
    showBook("/A/missing.epub");
    library.paint(true);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover missing"));
    TEST_ASSERT_EQUAL_INT(0, hostJpeg.prepares);
    TEST_ASSERT_TRUE(hostPanel.pixels.empty());

    hostCard().files["/A/big.jpg"] = tjpg(8000, 8000, 0, 0, 0);
    showBook("/A/big.epub");
    library.paint(true);
    TEST_ASSERT_EQUAL_INT(1, hostJpeg.prepares);
    TEST_ASSERT_EQUAL_INT(0, hostJpeg.decomps);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover missing"));
    hostJpeg.prepares = 0;
    Serial.out.clear();
    library.paint(true);
    TEST_ASSERT_EQUAL_INT(1, hostJpeg.prepares);

    libraryResetForTest();
    hostCard().reset();
    hostJpeg = {};
    hostJpeg.failDecomp = true;
    hostCard().files["/A/bad.jpg"] = tjpg(1, 1, 0, 0, 0);
    showBook("/A/bad.epub");
    library.paint(true);
    TEST_ASSERT_EQUAL_INT(1, hostJpeg.decomps);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover missing"));
    const int decomps = hostJpeg.decomps;
    library.paint(true);
    TEST_ASSERT_TRUE(hostJpeg.decomps > decomps);

    libraryResetForTest();
    hostCard().reset();
    hostJpeg = {};
    showBook((std::string(764, 'a') + ".epub").c_str());
    library.paint(true);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover missing"));

    showBook("/.epub");
    library.paint(true);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover missing"));
    TEST_ASSERT_EQUAL_STRING("No books", textAt(textBaseline(4)).c_str());
}

extern "C" void test_epub_cover_extract_and_stored_image(void) {
    const auto image = tjpg(1, 1, 0, 0, 0);
    hostCard().files["/Austen/pride.epub"] = "XXXX" + image;
    reportCover = true;
    scripted.method = 0;
    scripted.offset = 4;
    scripted.size = static_cast<uint32_t>(image.size());
    showBook("/Austen/pride.epub");
    library.paint(true);
    TEST_ASSERT_EQUAL_INT(1, hostPanel.pixels.size());
    TEST_ASSERT_EQUAL_INT(coverX(1), hostPanel.pixels[0].x);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover epub"));
    TEST_ASSERT_EQUAL_INT(3, releases);

    libraryResetForTest();
    hostJpeg = {};
    scripted.offset = 5000;
    scripted.size = 20;
    showBook("/Austen/pride.epub");
    library.paint(true);
    TEST_ASSERT_TRUE(hostPanel.pixels.empty());
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover epub"));
    Serial.out.clear();
    library.paint(true);
    TEST_ASSERT_NULL(strstr(Serial.out.c_str(), "Cover cached"));

    libraryResetForTest();
    hostJpeg = {};
    scripted.method = 8;
    scripted.offset = 0;
    scripted.size = 10;
    extract = Extract::Fail;
    showBook("/A/deflated.epub");
    library.paint(true);
    TEST_ASSERT_NULL(strstr(Serial.out.c_str(), "Cover epub"));
    TEST_ASSERT_NULL(strstr(Serial.out.c_str(), "Cover missing"));

    extract = Extract::Empty;
    showBook("/A/empty.epub");
    library.paint(true);
    TEST_ASSERT_NULL(strstr(Serial.out.c_str(), "Cover epub"));
    TEST_ASSERT_TRUE(hostFs().files.find("/cover.tmp") == hostFs().files.end());

    extract = Extract::Write;
    extractBytes = "not-a-jpeg";
    showBook("/A/garbage.epub");
    library.paint(true);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover epub"));
    TEST_ASSERT_TRUE(hostFs().files.find("/cover.tmp") == hostFs().files.end());
    TEST_ASSERT_TRUE(hostPanel.pixels.empty());
    Serial.out.clear();
    library.paint(true);
    TEST_ASSERT_NULL(strstr(Serial.out.c_str(), "Cover cached"));

    libraryResetForTest();
    hostJpeg = {};
    extractBytes = image;
    releases = 0;
    showBook("/A/inflated.epub");
    library.paint(true);
    TEST_ASSERT_EQUAL_INT(1, hostPanel.pixels.size());
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover epub"));
    TEST_ASSERT_EQUAL_INT(2, releases);
    TEST_ASSERT_TRUE(hostFs().files.find("/cover.tmp") == hostFs().files.end());
    Serial.out.clear();
    const int after = releases;
    library.paint(true);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "Cover cached"));
    TEST_ASSERT_EQUAL_INT(after, releases);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_panel_geometry_and_bus_init);
    RUN_TEST(test_refresh_schedule_and_reading_paint);
    RUN_TEST(test_reading_paint_skips_carriage_returns);
    RUN_TEST(test_library_partial_does_not_spend_a_refresh);
    RUN_TEST(test_library_caption_author_title_and_index);
    RUN_TEST(test_sidecar_cover_is_scaled_cached_and_evicted);
    RUN_TEST(test_rejected_covers_are_not_cached);
    RUN_TEST(test_epub_cover_extract_and_stored_image);
    return UNITY_END();
}
