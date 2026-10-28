#include "display.h"
#include "epub.h"
#include "input.h"
#include "paginate.h"

#include <unity.h>

#include <LittleFS.h>

#include <algorithm>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <strings.h>
#include <vector>

#include "../../src/paginate.cpp"

void appSetup();
void appLoop();

namespace {

constexpr int kTestColumns = 5;
constexpr int kTestRows = 1;

struct Paint {
    bool library = false;
    bool full = false;
    size_t offset = 0;
    size_t index = 0;
    size_t count = 0;
    std::string text;
};

struct ChapterSpec {
    const char* text = "";
    bool ok = true;
};

std::vector<Paint> paints;
std::deque<Input::Gesture> queued;
std::map<std::string, std::vector<ChapterSpec>> catalog;

const char* kChapter0 = "hello world";
const char* kChapter1 = "alone";
const char* kChapter2 = "zzzzz yyyyy";

Input::Gesture gesture(Input::Press prev, Input::Press next, bool bothLong = false) {
    Input::Gesture value;
    value.prev = prev;
    value.next = next;
    value.bothLong = bothLong;
    return value;
}

void tap(Input::Gesture value) {
    queued.push_back(value);
    appLoop();
}

void setRoot(std::initializer_list<HostRootEntry> entries) {
    hostFs().root.assign(entries.begin(), entries.end());
    hostFs().rootSet = true;
}

std::string positionsFile() {
    const auto found = hostFs().files.find("/positions.txt");
    if (found == hostFs().files.end()) {
        return {};
    }
    return found->second;
}

void stock() {
    catalog["/walden.epub"] = {{kChapter0, true}, {kChapter1, true}, {kChapter2, true}};
    catalog["/aaa.epub"] = {{"other", true}};
}

void boot() {
    stock();
    appSetup();
    paints.clear();
}

}  // namespace

std::vector<size_t> paginate(const String& text) {
    return pageOffsets(text.c_str(), text.length(), kTestColumns, kTestRows);
}

void initDisplay() {}

void drawPage(const String& text, size_t offset, bool fullRefresh) {
    paints.push_back(Paint{false, fullRefresh, offset, 0, 0, std::string(text.c_str(), text.length())});
}

void drawLibrary(const char* epubPath, size_t index, size_t count, bool fullRefresh) {
    paints.push_back(Paint{true, fullRefresh, 0, index, count, epubPath == nullptr ? "" : epubPath});
}

size_t epubChapterCount(const char* path) {
    const auto found = catalog.find(path == nullptr ? "" : path);
    if (found == catalog.end()) {
        return 0;
    }
    return found->second.size();
}

bool loadEpubChapter(const char* path, size_t index, String& text) {
    const auto found = catalog.find(path == nullptr ? "" : path);
    if (found == catalog.end() || index >= found->second.size() || !found->second[index].ok) {
        return false;
    }
    text = found->second[index].text;
    return true;
}

bool epubFindCover(const char*, EpubCover&) { return false; }
bool epubExtract(const char*, const EpubCover&, const char*) { return false; }

namespace Input {
void init() {}

Gesture read() {
    if (queued.empty()) {
        return {};
    }
    const Gesture value = queued.front();
    queued.pop_front();
    return value;
}
}  // namespace Input

#define setup appSetup
#define loop appLoop

bool hostCardOn = false;

bool mountCard() {
    return hostCardOn;
}

bool cardMounted() {
    return hostCardOn;
}

FS& bookFs() {
    return hostCardOn ? CardFs : LittleFS;
}

void releaseDisplayBus() {}

#include "card.h"

bool readCardDirectory(const char* path, std::vector<CardEntry>& out) {
    File dir = CardFs.open(path == nullptr ? "/" : path);
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        return false;
    }
    File entry = dir.openNextFile();
    while (entry) {
        CardEntry item;
        item.directory = entry.isDirectory();
        const char* raw = entry.path();
        const char* slash = raw == nullptr ? nullptr : strrchr(raw, '/');
        const char* name = raw == nullptr ? "" : (slash == nullptr ? raw : slash + 1);
        item.name = String(name);
        entry.close();
        if (item.name.length() > 0 && item.name.c_str()[0] != '.') {
            out.push_back(item);
        }
        entry = dir.openNextFile();
    }
    dir.close();
    return true;
}

#include "../../src/main.cpp"
#undef setup
#undef loop

extern "C" void setUp(void) {
    hostFs().reset();
    hostCard().reset();
    hostCardOn = false;
    Serial.out.clear();
    paints.clear();
    queued.clear();
    catalog.clear();
    bookPath = "/walden.epub";
    bookText = "";
    pageStarts.clear();
    books.clear();
    chapterIndex = 0;
    pageIndex = 0;
    bookPick = 0;
    inLibrary = false;
    loadPlaces();
}

extern "C" void tearDown(void) {}

extern "C" void test_filename_suffix(void) {
    TEST_ASSERT_TRUE(endsWithEpub("walden.epub"));
    TEST_ASSERT_TRUE(endsWithEpub("walden.EPUB"));
    TEST_ASSERT_TRUE(endsWithEpub(".epub"));
    TEST_ASSERT_TRUE(endsWithEpub("BOOK1.EPU"));
    TEST_ASSERT_TRUE(endsWithEpub("BOOK1~1.EPU"));
    TEST_ASSERT_FALSE(endsWithEpub("epub"));
    TEST_ASSERT_FALSE(endsWithEpub("notes.txt"));
    TEST_ASSERT_FALSE(endsWithEpub("walden.epub.bak"));
}

extern "C" void test_positions_skip_bad_lines_and_keep_carriage_return(void) {
    hostFs().files["/positions.txt"] = "nope\n/walden.epub\t2\t1\r\n\t1\t1\n/aaa.epub\t3\t4\n";
    loadPlaces();

    size_t chapter = 9;
    size_t page = 9;
    savedPlace("/walden.epub", chapter, page);
    TEST_ASSERT_EQUAL_INT(2, chapter);
    TEST_ASSERT_EQUAL_INT(1, page);

    savedPlace("/aaa.epub", chapter, page);
    TEST_ASSERT_EQUAL_INT(3, chapter);
    TEST_ASSERT_EQUAL_INT(4, page);

    savedPlace("/missing.epub", chapter, page);
    TEST_ASSERT_EQUAL_INT(0, chapter);
    TEST_ASSERT_EQUAL_INT(0, page);
}

extern "C" void test_boot_opens_the_saved_page_with_a_full_refresh(void) {
    stock();
    hostFs().files["/positions.txt"] = "/walden.epub\t1\t0\n";
    appSetup();

    TEST_ASSERT_EQUAL_INT(1, chapterIndex);
    TEST_ASSERT_EQUAL_INT(0, pageIndex);
    TEST_ASSERT_EQUAL_INT(1, paints.size());
    TEST_ASSERT_FALSE(paints[0].library);
    TEST_ASSERT_TRUE(paints[0].full);
    TEST_ASSERT_EQUAL_STRING("alone", paints[0].text.c_str());
}

extern "C" void test_boot_clamps_a_saved_page_past_the_end(void) {
    stock();
    hostFs().files["/positions.txt"] = "/walden.epub\t0\t50\n";
    appSetup();

    TEST_ASSERT_EQUAL_INT(2, pageStarts.size());
    TEST_ASSERT_EQUAL_INT(1, pageIndex);
    TEST_ASSERT_TRUE(paints.back().full);
}

extern "C" void test_boot_reports_failure_without_a_book(void) {
    hostFs().mounted = false;
    appSetup();
    TEST_ASSERT_EQUAL_INT(0, paints.size());

    hostFs().mounted = true;
    appSetup();
    TEST_ASSERT_EQUAL_INT(0, paints.size());
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
}

extern "C" void test_saved_chapter_past_the_end_does_not_paint(void) {
    stock();
    hostFs().files["/positions.txt"] = "/walden.epub\t9\t0\n";
    appSetup();
    TEST_ASSERT_EQUAL_INT(0, paints.size());
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
}

extern "C" void test_short_presses_turn_pages_and_cross_chapters(void) {
    boot();
    TEST_ASSERT_EQUAL_INT(2, pageStarts.size());
    TEST_ASSERT_EQUAL_STRING("/walden.epub\t0\t0\n", positionsFile().c_str());

    tap(gesture(Input::Press::None, Input::Press::Short));
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
    TEST_ASSERT_EQUAL_INT(1, pageIndex);
    TEST_ASSERT_FALSE(paints.back().full);
    TEST_ASSERT_EQUAL_STRING("/walden.epub\t0\t1\n", positionsFile().c_str());

    tap(gesture(Input::Press::Short, Input::Press::None));
    TEST_ASSERT_EQUAL_INT(0, pageIndex);
    TEST_ASSERT_FALSE(paints.back().full);

    tap(gesture(Input::Press::Short, Input::Press::None));
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
    TEST_ASSERT_EQUAL_INT(0, pageIndex);
    TEST_ASSERT_EQUAL_INT(2, paints.size());

    tap(gesture(Input::Press::None, Input::Press::Short));
    tap(gesture(Input::Press::None, Input::Press::Short));
    TEST_ASSERT_EQUAL_INT(1, chapterIndex);
    TEST_ASSERT_EQUAL_INT(0, pageIndex);
    TEST_ASSERT_TRUE(paints.back().full);
    TEST_ASSERT_EQUAL_STRING("alone", paints.back().text.c_str());

    tap(gesture(Input::Press::Short, Input::Press::None));
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
    TEST_ASSERT_EQUAL_INT(pageStarts.size() - 1, pageIndex);
    TEST_ASSERT_TRUE(paints.back().full);
}

extern "C" void test_long_press_changes_chapter_from_the_first_page(void) {
    boot();
    tap(gesture(Input::Press::None, Input::Press::Short));
    TEST_ASSERT_EQUAL_INT(1, pageIndex);

    tap(gesture(Input::Press::None, Input::Press::Long));
    TEST_ASSERT_EQUAL_INT(1, chapterIndex);
    TEST_ASSERT_EQUAL_INT(0, pageIndex);
    TEST_ASSERT_TRUE(paints.back().full);

    tap(gesture(Input::Press::Long, Input::Press::None));
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
    TEST_ASSERT_EQUAL_INT(0, pageIndex);
    TEST_ASSERT_TRUE(paints.back().full);

    paints.clear();
    tap(gesture(Input::Press::Long, Input::Press::None));
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
    TEST_ASSERT_TRUE(paints.empty());

    openChapter(2, 0, true);
    TEST_ASSERT_EQUAL_INT(2, chapterIndex);
    TEST_ASSERT_EQUAL_INT(1, pageIndex);
    paints.clear();
    tap(gesture(Input::Press::None, Input::Press::Long));
    TEST_ASSERT_EQUAL_INT(2, chapterIndex);
    TEST_ASSERT_EQUAL_INT(1, pageIndex);
    TEST_ASSERT_TRUE(paints.empty());
}

extern "C" void test_failed_chapter_load_paints_an_empty_full_page(void) {
    boot();
    catalog["/walden.epub"][1].ok = false;
    const int before = paints.size();
    TEST_ASSERT_FALSE(openChapter(1, 0, false));
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
    TEST_ASSERT_EQUAL_INT(before + 1, paints.size());
    TEST_ASSERT_TRUE(paints.back().full);
    TEST_ASSERT_TRUE(paints.back().text.empty());
}

extern "C" void test_empty_chapter_is_a_full_refresh(void) {
    catalog["/walden.epub"] = {{"", true}};
    TEST_ASSERT_TRUE(openChapter(0, 3, false));
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
    TEST_ASSERT_EQUAL_INT(0, pageIndex);
    TEST_ASSERT_TRUE(pageStarts.empty());
    TEST_ASSERT_TRUE(paints.back().full);
}

extern "C" void test_library_lists_epubs_and_ignores_other_files(void) {
    boot();
    setRoot({
        {"notes.txt", false},
        {"Plain.EPUB", false},
        {"/walden.epub", false},
        {"subdir", true},
    });
    tap(gesture(Input::Press::None, Input::Press::None, true));

    TEST_ASSERT_TRUE(inLibrary);
    TEST_ASSERT_EQUAL_INT(2, books.size());
    TEST_ASSERT_EQUAL_STRING("/Plain.EPUB", books[0].c_str());
    TEST_ASSERT_EQUAL_STRING("/walden.epub", books[1].c_str());
    TEST_ASSERT_EQUAL_INT(1, bookPick);
    TEST_ASSERT_TRUE(paints.back().library);
    TEST_ASSERT_TRUE(paints.back().full);
    TEST_ASSERT_EQUAL_STRING("/walden.epub", paints.back().text.c_str());

    const int shown = paints.size();
    tap(gesture(Input::Press::None, Input::Press::None, true));
    tap(gesture(Input::Press::Long, Input::Press::None));
    TEST_ASSERT_EQUAL_INT(shown, paints.size());
    TEST_ASSERT_TRUE(inLibrary);
}

extern "C" void test_library_navigation_is_partial_and_opens_the_saved_place(void) {
    boot();
    tap(gesture(Input::Press::None, Input::Press::Short));
    setRoot({{"/walden.epub", false}, {"/aaa.epub", false}});
    tap(gesture(Input::Press::None, Input::Press::None, true));
    TEST_ASSERT_EQUAL_INT(1, bookPick);

    tap(gesture(Input::Press::Short, Input::Press::None));
    TEST_ASSERT_EQUAL_INT(0, bookPick);
    TEST_ASSERT_TRUE(paints.back().library);
    TEST_ASSERT_FALSE(paints.back().full);
    TEST_ASSERT_EQUAL_STRING("/aaa.epub", paints.back().text.c_str());

    tap(gesture(Input::Press::Short, Input::Press::None));
    TEST_ASSERT_EQUAL_INT(1, bookPick);
    TEST_ASSERT_FALSE(paints.back().full);

    tap(gesture(Input::Press::None, Input::Press::Short));
    TEST_ASSERT_EQUAL_INT(0, bookPick);
    tap(gesture(Input::Press::None, Input::Press::Long));
    TEST_ASSERT_FALSE(inLibrary);
    TEST_ASSERT_EQUAL_STRING("/aaa.epub", bookPath.c_str());
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
    TEST_ASSERT_EQUAL_INT(0, pageIndex);
    TEST_ASSERT_FALSE(paints.back().library);
    TEST_ASSERT_TRUE(paints.back().full);

    tap(gesture(Input::Press::None, Input::Press::None, true));
    tap(gesture(Input::Press::None, Input::Press::Short));
    TEST_ASSERT_EQUAL_STRING("/walden.epub", books[bookPick].c_str());
    tap(gesture(Input::Press::None, Input::Press::Long));
    TEST_ASSERT_EQUAL_STRING("/walden.epub", bookPath.c_str());
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
    TEST_ASSERT_EQUAL_INT(1, pageIndex);
    TEST_ASSERT_TRUE(paints.back().full);
    TEST_ASSERT_TRUE(positionsFile().find("/walden.epub\t0\t1\n") != std::string::npos);
    TEST_ASSERT_TRUE(positionsFile().find("/aaa.epub\t0\t0\n") != std::string::npos);
}

extern "C" void test_single_book_library_only_opens_on_long_next(void) {
    boot();
    tap(gesture(Input::Press::None, Input::Press::Short));
    setRoot({{"/walden.epub", false}});
    tap(gesture(Input::Press::None, Input::Press::None, true));
    const int shown = paints.size();

    tap(gesture(Input::Press::Short, Input::Press::None));
    tap(gesture(Input::Press::None, Input::Press::Short));
    tap(gesture(Input::Press::Long, Input::Press::None));
    TEST_ASSERT_EQUAL_INT(shown, paints.size());
    TEST_ASSERT_TRUE(inLibrary);

    tap(gesture(Input::Press::None, Input::Press::Long));
    TEST_ASSERT_FALSE(inLibrary);
    TEST_ASSERT_EQUAL_INT(0, chapterIndex);
    TEST_ASSERT_EQUAL_INT(1, pageIndex);
    TEST_ASSERT_TRUE(paints.back().full);
}

extern "C" void test_unreadable_book_returns_to_the_library(void) {
    boot();
    setRoot({{"/broken.epub", false}});
    tap(gesture(Input::Press::None, Input::Press::None, true));
    paints.clear();

    tap(gesture(Input::Press::None, Input::Press::Long));
    TEST_ASSERT_TRUE(inLibrary);
    TEST_ASSERT_EQUAL_STRING("/broken.epub", bookPath.c_str());
    TEST_ASSERT_EQUAL_INT(1, paints.size());
    TEST_ASSERT_TRUE(paints.back().library);
    TEST_ASSERT_TRUE(paints.back().full);
}

extern "C" void test_card_library_lists_author_folders_and_restores_the_place(void) {
    catalog["/Austen/pride.epub"] = {{kChapter0, true}, {kChapter1, true}};
    catalog["/Dickens/tale.epub"] = {{"other", true}};

    HostRootEntry austen;
    austen.path = "/Austen";
    austen.directory = true;
    austen.children.push_back({"pride.epub", false, {}});
    austen.children.push_back({"/Austen/notes.txt", false, {}});
    austen.children.push_back({"/Austen/series", true, {}});
    HostRootEntry dickens;
    dickens.path = "/Dickens";
    dickens.directory = true;
    dickens.children.push_back({"/Dickens/tale.epub", false, {}});
    hostCard().root.push_back(austen);
    hostCard().root.push_back(dickens);
    hostCard().root.push_back({"/loose.epub", false, {}});
    hostCard().root.push_back({"/readme.txt", false, {}});
    hostCard().rootSet = true;
    hostCardOn = true;
    bookPath = "/Austen/pride.epub";

    appSetup();
    TEST_ASSERT_FALSE(inLibrary);
    TEST_ASSERT_EQUAL_STRING("/Austen/pride.epub", bookPath.c_str());
    TEST_ASSERT_EQUAL_INT(0, pageIndex);

    tap(gesture(Input::Press::None, Input::Press::Short));
    TEST_ASSERT_EQUAL_INT(1, pageIndex);

    tap(gesture(Input::Press::None, Input::Press::None, true));
    TEST_ASSERT_TRUE(inLibrary);
    TEST_ASSERT_EQUAL_INT(2, books.size());
    TEST_ASSERT_EQUAL_STRING("/Austen/pride.epub", books[0].c_str());
    TEST_ASSERT_EQUAL_STRING("/Dickens/tale.epub", books[1].c_str());
    TEST_ASSERT_EQUAL_INT(0, bookPick);
    TEST_ASSERT_TRUE(positionsFile().find("/Austen/pride.epub\t0\t1\n") != std::string::npos);

    tap(gesture(Input::Press::None, Input::Press::Short));
    TEST_ASSERT_EQUAL_STRING("/Dickens/tale.epub", paints.back().text.c_str());
    tap(gesture(Input::Press::None, Input::Press::Long));
    TEST_ASSERT_FALSE(inLibrary);
    TEST_ASSERT_EQUAL_STRING("/Dickens/tale.epub", bookPath.c_str());
    TEST_ASSERT_EQUAL_INT(0, pageIndex);

    tap(gesture(Input::Press::None, Input::Press::None, true));
    TEST_ASSERT_TRUE(positionsFile().find("/Dickens/tale.epub\t0\t0\n") != std::string::npos);
    TEST_ASSERT_TRUE(positionsFile().find("/Austen/pride.epub\t0\t1\n") != std::string::npos);

    tap(gesture(Input::Press::Short, Input::Press::None));
    tap(gesture(Input::Press::None, Input::Press::Long));
    TEST_ASSERT_EQUAL_STRING("/Austen/pride.epub", bookPath.c_str());
    TEST_ASSERT_EQUAL_INT(1, pageIndex);
    TEST_ASSERT_FALSE(paints.back().library);
}

extern "C" void test_card_library_finds_epubs_inside_title_folders(void) {
    catalog["/Austen/Pride/pride.epub"] = {{"pride", true}};
    catalog["/Austen/emma.epub"] = {{"emma", true}};

    HostRootEntry pride;
    pride.path = "/Austen/Pride";
    pride.directory = true;
    pride.children.push_back({"/Austen/Pride/pride.epub", false, {}});
    pride.children.push_back({"/Austen/Pride/cover.jpg", false, {}});
    HostRootEntry austen;
    austen.path = "/Austen";
    austen.directory = true;
    austen.children.push_back(pride);
    austen.children.push_back({"/Austen/emma.epub", false, {}});
    hostCard().root.push_back(austen);
    hostCard().rootSet = true;
    hostCardOn = true;
    bookPath = "/Austen/Pride/pride.epub";

    appSetup();
    TEST_ASSERT_FALSE(inLibrary);
    TEST_ASSERT_EQUAL_STRING("/Austen/Pride/pride.epub", bookPath.c_str());

    tap(gesture(Input::Press::None, Input::Press::None, true));
    TEST_ASSERT_EQUAL_INT(2, books.size());
    TEST_ASSERT_EQUAL_STRING("/Austen/Pride/pride.epub", books[0].c_str());
    TEST_ASSERT_EQUAL_STRING("/Austen/emma.epub", books[1].c_str());
}

extern "C" void test_missing_card_book_opens_the_library_without_forgetting_a_place(void) {
    hostFs().files["/positions.txt"] = "/walden.epub\t2\t1\n";
    HostRootEntry austen;
    austen.path = "/Austen";
    austen.directory = true;
    austen.children.push_back({"pride.epub", false, {}});
    hostCard().root.push_back(austen);
    hostCard().rootSet = true;
    hostCardOn = true;
    catalog["/Austen/pride.epub"] = {{"other", true}};

    appSetup();
    TEST_ASSERT_TRUE(inLibrary);
    TEST_ASSERT_EQUAL_INT(1, books.size());
    TEST_ASSERT_EQUAL_STRING("/Austen/pride.epub", books[0].c_str());
    TEST_ASSERT_EQUAL_STRING("/walden.epub\t2\t1\n", positionsFile().c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_filename_suffix);
    RUN_TEST(test_positions_skip_bad_lines_and_keep_carriage_return);
    RUN_TEST(test_boot_opens_the_saved_page_with_a_full_refresh);
    RUN_TEST(test_boot_clamps_a_saved_page_past_the_end);
    RUN_TEST(test_boot_reports_failure_without_a_book);
    RUN_TEST(test_saved_chapter_past_the_end_does_not_paint);
    RUN_TEST(test_short_presses_turn_pages_and_cross_chapters);
    RUN_TEST(test_long_press_changes_chapter_from_the_first_page);
    RUN_TEST(test_failed_chapter_load_paints_an_empty_full_page);
    RUN_TEST(test_empty_chapter_is_a_full_refresh);
    RUN_TEST(test_library_lists_epubs_and_ignores_other_files);
    RUN_TEST(test_library_navigation_is_partial_and_opens_the_saved_place);
    RUN_TEST(test_single_book_library_only_opens_on_long_next);
    RUN_TEST(test_unreadable_book_returns_to_the_library);
    RUN_TEST(test_card_library_lists_author_folders_and_restores_the_place);
    RUN_TEST(test_card_library_finds_epubs_inside_title_folders);
    RUN_TEST(test_missing_card_book_opens_the_library_without_forgetting_a_place);
    return UNITY_END();
}
