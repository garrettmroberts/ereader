#include <unity.h>

#include <zlib.h>

#include <fstream>
#include <string>
#include <vector>

#include "../../src/epub.cpp"

FS& bookFs() {
    return LittleFS;
}

void releaseDisplayBus() {}
#include "../../host/miniz/miniz_tinfl.c"

namespace {

void append16(std::string& out, uint16_t value) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
}

void append32(std::string& out, uint32_t value) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
    out.push_back(static_cast<char>((value >> 16) & 0xff));
    out.push_back(static_cast<char>((value >> 24) & 0xff));
}

std::string deflateRaw(const std::string& input) {
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return {};
    }
    std::string out(deflateBound(&stream, input.size()) + 32, '\0');
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = reinterpret_cast<Bytef*>(&out[0]);
    stream.avail_out = static_cast<uInt>(out.size());
    const int result = deflate(&stream, Z_FINISH);
    const size_t produced = stream.total_out;
    deflateEnd(&stream);
    if (result != Z_STREAM_END) {
        return {};
    }
    out.resize(produced);
    return out;
}

struct ZipEntry {
    std::string name;
    std::string data;
    uint16_t method = 0;
    uint16_t flags = 0;
};

std::string buildZip(const std::vector<ZipEntry>& entries) {
    std::string zip;
    for (const ZipEntry& entry : entries) {
        std::string payload = entry.data;
        uint32_t stored = static_cast<uint32_t>(entry.data.size());
        if (entry.method == 8) {
            payload = deflateRaw(entry.data);
            stored = static_cast<uint32_t>(payload.size());
        }
        if ((entry.flags & 0x8) != 0) {
            payload.clear();
            stored = 0;
        }
        append32(zip, 0x04034b50);
        append16(zip, 20);
        append16(zip, entry.flags);
        append16(zip, entry.method);
        append16(zip, 0);
        append16(zip, 0);
        append32(zip, 0);
        append32(zip, stored);
        append32(zip, static_cast<uint32_t>(entry.data.size()));
        append16(zip, static_cast<uint16_t>(entry.name.size()));
        append16(zip, 0);
        zip += entry.name;
        zip += payload;
    }
    return zip;
}

std::string opf(bool metaCover) {
    std::string xml = "<?xml version=\"1.0\"?><package><metadata>";
    if (metaCover) {
        xml += "<meta name=\"cover\" content=\"coverimg\"/>";
    }
    xml += "</metadata><manifest><item id=\"coverimg\" href=\"cover.jpg\" media-type=\"image/jpeg\"";
    if (!metaCover) {
        xml += " properties=\"cover-image\"";
    }
    xml += "/><item id=\"coverdoc\" href=\"cover.xhtml\" media-type=\"application/xhtml+xml\"/>";
    xml += "<item id=\"c1\" href=\"c1.xhtml\" media-type=\"application/xhtml+xml\"/>";
    xml += "<item id=\"c2\" href=\"c2.xhtml\" media-type=\"application/xhtml+xml\"/>";
    xml += "</manifest><spine><itemref idref=\"coverdoc\"/><itemref idref=\"c1\"/><itemref idref=\"c2\"/></spine></package>";
    return xml;
}

const char* kCoverDoc = "<html><HEAD><TITLE>Cover</TITLE></HEAD><body><img src=\"cover.jpg\"/></body></html>";
const char* kChapter1 =
    "<html><HEAD><TITLE>SECRET</TITLE></HEAD><body><!--nope--><style>.x{}</style><script>bad()</script>"
    "<p>Hello&nbsp;&amp; Walden</p></body></html>";
const char* kChapter2 = "<html><body>Second &#65; &zzz; chapter</body></html>";

std::string sampleBook(bool deflateCover, bool metaCover, std::string& jpeg) {
    jpeg = std::string("\xff\xd8", 2) + "fake-jpeg";
    return buildZip({
        {"OEBPS/cover.xhtml", kCoverDoc, 0, 0},
        {"OEBPS/c1.xhtml", kChapter1, 0, 0},
        {"OEBPS/c2.xhtml", kChapter2, 8, 0},
        {"OEBPS/cover.jpg", jpeg, static_cast<uint16_t>(deflateCover ? 8 : 0), 0},
        {"OEBPS/content.opf", opf(metaCover), 0, 0},
    });
}

void store(const char* path, const std::string& bytes) {
    hostFs().files[path] = bytes;
}

std::string textOf(const String& text) {
    return std::string(text.c_str(), text.length());
}

std::string readDisk(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

extern "C" void setUp(void) {
    hostFs().reset();
    Serial.out.clear();
    epubResetForTest();
}

extern "C" void tearDown(void) {}

extern "C" void test_missing_or_invalid_file_has_no_chapters(void) {
    TEST_ASSERT_EQUAL_INT(0, epubChapterCount("/missing.epub"));
    store("/garbage.epub", "this is not a zip");
    TEST_ASSERT_EQUAL_INT(0, epubChapterCount("/garbage.epub"));

    EpubCover cover;
    TEST_ASSERT_FALSE(epubFindCover("/missing.epub", cover));
    TEST_ASSERT_FALSE(epubFindCover("/garbage.epub", cover));

    String stale("keep");
    TEST_ASSERT_FALSE(loadEpubChapter("/missing.epub", 0, stale));
    TEST_ASSERT_EQUAL_STRING("keep", stale.c_str());
}

extern "C" void test_encrypted_and_data_descriptor_entries_are_rejected(void) {
    store("/encrypted.epub", buildZip({{"OEBPS/c1.xhtml", kChapter1, 0, 0x1}}));
    TEST_ASSERT_EQUAL_INT(0, epubChapterCount("/encrypted.epub"));

    store("/descriptor.epub", buildZip({{"OEBPS/c1.xhtml", kChapter1, 0, 0x8}}));
    TEST_ASSERT_EQUAL_INT(0, epubChapterCount("/descriptor.epub"));

    store("/badmethod.epub", buildZip({
        {"OEBPS/c1.xhtml", kChapter1, 99, 0},
        {"OEBPS/content.opf", opf(false), 0, 0},
    }));
    TEST_ASSERT_EQUAL_INT(0, epubChapterCount("/badmethod.epub"));
}

extern "C" void test_spine_text_skipping_the_cover_page(void) {
    std::string jpeg;
    store("/book.epub", sampleBook(false, false, jpeg));

    TEST_ASSERT_EQUAL_INT(2, epubChapterCount("/book.epub"));

    String text("stale");
    TEST_ASSERT_TRUE(loadEpubChapter("/book.epub", 0, text));
    TEST_ASSERT_EQUAL_STRING("Hello & Walden", text.c_str());
    TEST_ASSERT_TRUE(textOf(text).find("SECRET") == std::string::npos);
    TEST_ASSERT_TRUE(textOf(text).find("nope") == std::string::npos);
    TEST_ASSERT_TRUE(textOf(text).find("bad()") == std::string::npos);

    TEST_ASSERT_TRUE(loadEpubChapter("/book.epub", 1, text));
    TEST_ASSERT_EQUAL_STRING("Second A ? chapter", text.c_str());

    TEST_ASSERT_FALSE(loadEpubChapter("/book.epub", 2, text));
    TEST_ASSERT_EQUAL_STRING("Second A ? chapter", text.c_str());
}

extern "C" void test_stored_cover_round_trip(void) {
    std::string jpeg;
    store("/book.epub", sampleBook(false, false, jpeg));

    EpubCover cover;
    TEST_ASSERT_TRUE(epubFindCover("/book.epub", cover));
    TEST_ASSERT_EQUAL_INT(0, cover.method);
    TEST_ASSERT_EQUAL_INT(jpeg.size(), cover.size);

    TEST_ASSERT_TRUE(epubExtract("/book.epub", cover, "/cover.jpg"));
    TEST_ASSERT_EQUAL_INT(jpeg.size(), hostFs().files["/cover.jpg"].size());
    TEST_ASSERT_TRUE(hostFs().files["/cover.jpg"] == jpeg);
}

extern "C" void test_deflated_cover_from_meta_name(void) {
    std::string jpeg;
    store("/meta.epub", sampleBook(true, true, jpeg));

    EpubCover cover;
    TEST_ASSERT_TRUE(epubFindCover("/meta.epub", cover));
    TEST_ASSERT_EQUAL_INT(8, cover.method);
    TEST_ASSERT_TRUE(cover.size > 0);
    TEST_ASSERT_TRUE(cover.size != jpeg.size());

    TEST_ASSERT_TRUE(epubExtract("/meta.epub", cover, "/cover.jpg"));
    TEST_ASSERT_TRUE(hostFs().files["/cover.jpg"] == jpeg);
}

extern "C" void test_failed_extract_removes_the_destination(void) {
    store("/book.epub", "unused");
    const EpubCover cover{99, 0, 4};
    TEST_ASSERT_FALSE(epubExtract("/book.epub", cover, "/cover.jpg"));
    TEST_ASSERT_TRUE(hostFs().files.find("/cover.jpg") == hostFs().files.end());
}

extern "C" void test_chapter_index_is_cached_until_reset(void) {
    std::string jpeg;
    store("/book.epub", sampleBook(false, false, jpeg));
    String first;
    TEST_ASSERT_TRUE(loadEpubChapter("/book.epub", 0, first));

    const std::string replacement = buildZip({
        {"OEBPS/c1.xhtml", "<html><body>Changed</body></html>", 0, 0},
        {"OEBPS/content.opf",
         "<package><manifest><item id=\"c1\" href=\"c1.xhtml\"/></manifest><spine><itemref idref=\"c1\"/></spine></package>",
         0, 0},
    });
    store("/book.epub", "not a zip anymore");
    TEST_ASSERT_EQUAL_INT(2, epubChapterCount("/book.epub"));
    String stale("keep");
    TEST_ASSERT_FALSE(loadEpubChapter("/book.epub", 0, stale));
    TEST_ASSERT_EQUAL_STRING("", stale.c_str());

    epubResetForTest();
    store("/book.epub", replacement);
    String fresh;
    TEST_ASSERT_TRUE(loadEpubChapter("/book.epub", 0, fresh));
    TEST_ASSERT_EQUAL_STRING("Changed", fresh.c_str());
    TEST_ASSERT_EQUAL_INT(1, epubChapterCount("/book.epub"));
}

extern "C" void test_walden_chapters_and_cover(void) {
    const std::string bytes = readDisk("data/walden.epub");
    TEST_ASSERT_TRUE_MESSAGE(!bytes.empty(), "data/walden.epub is missing");
    store("/walden.epub", bytes);

    TEST_ASSERT_EQUAL_INT(23, epubChapterCount("/walden.epub"));

    String text;
    TEST_ASSERT_TRUE(loadEpubChapter("/walden.epub", 0, text));
    const std::string body = textOf(text);
    TEST_ASSERT_TRUE(body.find("Walden") != std::string::npos);
    TEST_ASSERT_TRUE(body.find("<html") == std::string::npos);
    TEST_ASSERT_TRUE(body.find("<p") == std::string::npos);
    TEST_ASSERT_TRUE(body.size() > 50);

    EpubCover cover;
    TEST_ASSERT_TRUE(epubFindCover("/walden.epub", cover));
    TEST_ASSERT_EQUAL_INT(0, cover.method);
    TEST_ASSERT_EQUAL_UINT32(245255, cover.size);

    TEST_ASSERT_TRUE(epubExtract("/walden.epub", cover, "/cover.jpg"));
    const std::string& image = hostFs().files["/cover.jpg"];
    TEST_ASSERT_EQUAL_UINT32(245255, image.size());
    TEST_ASSERT_EQUAL_INT(0xff, static_cast<unsigned char>(image[0]));
    TEST_ASSERT_EQUAL_INT(0xd8, static_cast<unsigned char>(image[1]));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_missing_or_invalid_file_has_no_chapters);
    RUN_TEST(test_encrypted_and_data_descriptor_entries_are_rejected);
    RUN_TEST(test_spine_text_skipping_the_cover_page);
    RUN_TEST(test_stored_cover_round_trip);
    RUN_TEST(test_deflated_cover_from_meta_name);
    RUN_TEST(test_failed_extract_removes_the_destination);
    RUN_TEST(test_chapter_index_is_cached_until_reset);
    RUN_TEST(test_walden_chapters_and_cover);
    return UNITY_END();
}
