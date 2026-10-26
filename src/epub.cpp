#include "epub.h"

#include <miniz.h>

#include <Arduino.h>
#include <LittleFS.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <vector>

namespace {

constexpr uint32_t kLocalHeader = 0x04034b50;

bool readExact(File& file, uint8_t* dst, size_t length) {
    size_t got = 0;
    while (got < length) {
        const size_t n = file.read(dst + got, length - got);
        if (n == 0) {
            return false;
        }
        got += n;
    }
    return true;
}

uint16_t readU16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

bool endsWithHtml(const char* name) {
    const char* dot = strrchr(name, '.');
    if (dot == nullptr) {
        return false;
    }
    return strcasecmp(dot, ".xhtml") == 0 || strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0;
}

// Pulls visible text out of an XHTML byte stream.
// `stopAtText` ends the stream at the first visible character, for chapter detection.
class TextPrinter {
public:
    TextPrinter(String* out, bool stopAtText) : out_(out), stopAtText_(stopAtText) {}

    bool full() const { return stop_; }
    bool sawText() const { return sawText_; }

    void feed(uint8_t b) {
        if (full()) {
            return;
        }
        switch (mode_) {
            case Mode::Comment:
                feedComment(b);
                return;
            case Mode::Entity:
                feedEntity(b);
                return;
            case Mode::Tag:
                feedTag(b);
                return;
            case Mode::Text:
                feedText(b);
                return;
        }
    }

private:
    enum class Mode { Text, Tag, Comment, Entity };

    void appendCp(uint32_t cp) {
        if (out_ == nullptr || cp == 0) {
            return;
        }
        if (cp < 0x80) {
            out_->concat(static_cast<char>(cp));
            return;
        }

        char utf8[4];
        size_t n = 0;
        if (cp < 0x800) {
            utf8[n++] = static_cast<char>(0xC0 | (cp >> 6));
            utf8[n++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            utf8[n++] = static_cast<char>(0xE0 | (cp >> 12));
            utf8[n++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            utf8[n++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0x10FFFF) {
            utf8[n++] = static_cast<char>(0xF0 | (cp >> 18));
            utf8[n++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            utf8[n++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            utf8[n++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            return;
        }
        for (size_t i = 0; i < n; i++) {
            out_->concat(utf8[i]);
        }
    }

    void emit(uint32_t cp) {
        if (stop_ || cp == 0) {
            return;
        }
        const bool space = cp == ' ' || cp == '\n' || cp == '\r' || cp == '\t';
        if (!space) {
            sawText_ = true;
            if (stopAtText_) {
                stop_ = true;
                return;
            }
        }
        appendCp(cp);
    }

    void feedUtf8(uint8_t b) {
        if (utf8Left_ > 0) {
            if ((b & 0xC0) != 0x80) {
                utf8Left_ = 0;
            } else {
                cp_ = (cp_ << 6) | (b & 0x3F);
                utf8Left_--;
                if (utf8Left_ == 0) {
                    emit(cp_);
                }
                return;
            }
        }

        if (b < 0x80) {
            emit(b);
        } else if ((b & 0xE0) == 0xC0) {
            cp_ = b & 0x1F;
            utf8Left_ = 1;
        } else if ((b & 0xF0) == 0xE0) {
            cp_ = b & 0x0F;
            utf8Left_ = 2;
        } else if ((b & 0xF8) == 0xF0) {
            cp_ = b & 0x07;
            utf8Left_ = 3;
        }
    }

    void feedText(uint8_t b) {
        if (b == '<') {
            mode_ = Mode::Tag;
            tagLen_ = 0;
            closing_ = false;
            takingName_ = true;
            return;
        }
        if (skipDepth_ > 0 || full()) {
            return;
        }
        if (b == '&') {
            mode_ = Mode::Entity;
            entLen_ = 0;
            return;
        }
        feedUtf8(b);
    }

    void feedComment(uint8_t b) {
        if (b == '-') {
            if (dash_ < 2) {
                dash_++;
            }
            return;
        }
        if (b == '>' && dash_ == 2) {
            mode_ = Mode::Text;
        }
        dash_ = 0;
    }

    void feedEntity(uint8_t b) {
        if (b == ';') {
            ent_[entLen_] = '\0';
            if (skipDepth_ == 0) {
                emit(decodeEntity());
            }
            entLen_ = 0;
            mode_ = Mode::Text;
            return;
        }

        if (b != '<' && b != '&' && b != ' ' && entLen_ + 1 < sizeof(ent_)) {
            ent_[entLen_++] = static_cast<char>(b);
            return;
        }

        if (skipDepth_ == 0) {
            emit('&');
            for (size_t i = 0; i < entLen_ && !full(); i++) {
                feedUtf8(static_cast<uint8_t>(ent_[i]));
            }
        }
        entLen_ = 0;
        mode_ = Mode::Text;
        feed(b);
    }

    uint32_t decodeEntity() const {
        if (strcmp(ent_, "amp") == 0) return '&';
        if (strcmp(ent_, "lt") == 0) return '<';
        if (strcmp(ent_, "gt") == 0) return '>';
        if (strcmp(ent_, "quot") == 0) return '"';
        if (strcmp(ent_, "apos") == 0) return '\'';
        if (strcmp(ent_, "nbsp") == 0) return ' ';
        if (ent_[0] == '#') {
            const unsigned long value = (ent_[1] == 'x' || ent_[1] == 'X')
                ? strtoul(ent_ + 2, nullptr, 16)
                : strtoul(ent_ + 1, nullptr, 10);
            if (value == 0 || value > 0x10FFFF) {
                return '?';
            }
            return static_cast<uint32_t>(value);
        }
        return '?';
    }

    void finishTag() {
        tag_[tagLen_] = '\0';
        const bool skips = strcmp(tag_, "head") == 0 || strcmp(tag_, "script") == 0 || strcmp(tag_, "style") == 0;
        if (skips) {
            if (closing_) {
                if (skipDepth_ > 0) {
                    skipDepth_--;
                }
            } else {
                skipDepth_++;
            }
        }
        mode_ = Mode::Text;
        tagLen_ = 0;
        closing_ = false;
        takingName_ = false;
    }

    void feedTag(uint8_t b) {
        if (takingName_) {
            if (b == '/' && tagLen_ == 0) {
                closing_ = true;
                return;
            }

            const bool letter = (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z');
            const bool more = tagLen_ > 0 && (b == '-' || (b >= '0' && b <= '9'));
            const bool bang = b == '!' && tagLen_ == 0;
            if (letter || more || bang) {
                if (tagLen_ + 1 < sizeof(tag_)) {
                    tag_[tagLen_++] = static_cast<char>(tolower(b));
                }
                if (tagLen_ == 3 && tag_[0] == '!' && tag_[1] == '-' && tag_[2] == '-') {
                    mode_ = Mode::Comment;
                    dash_ = 0;
                }
                return;
            }
            takingName_ = false;
        }

        if (b == '>') {
            finishTag();
        }
    }

    String* out_ = nullptr;
    bool stopAtText_ = false;
    bool stop_ = false;
    bool sawText_ = false;
    Mode mode_ = Mode::Text;
    int skipDepth_ = 0;
    bool closing_ = false;
    bool takingName_ = false;
    char tag_[16] = {};
    size_t tagLen_ = 0;
    char ent_[16] = {};
    size_t entLen_ = 0;
    int dash_ = 0;
    uint32_t cp_ = 0;
    int utf8Left_ = 0;
};

struct ZipItem {
    uint16_t method = 0;
    uint32_t compSize = 0;
    uint32_t uncompSize = 0;
    uint32_t dataOffset = 0;
    char name[160] = {};
};

bool readEntry(File& file, ZipItem& entry) {
    uint8_t sig[4];
    if (!readExact(file, sig, sizeof(sig))) {
        return false;
    }
    if (readU32(sig) != kLocalHeader) {
        return false;
    }

    uint8_t hdr[26];
    if (!readExact(file, hdr, sizeof(hdr))) {
        return false;
    }

    const uint16_t flags = readU16(hdr + 2);
    entry.method = readU16(hdr + 4);
    entry.compSize = readU32(hdr + 14);
    entry.uncompSize = readU32(hdr + 18);
    const uint16_t nameLen = readU16(hdr + 22);
    const uint16_t extraLen = readU16(hdr + 24);

    if ((flags & 0x1) != 0) {
        Serial.println("Encrypted zip entry");
        return false;
    }
    if ((flags & 0x8) != 0 && entry.compSize == 0) {
        Serial.println("Zip data descriptor is not supported");
        return false;
    }

    const size_t keep = nameLen < sizeof(entry.name) - 1 ? nameLen : sizeof(entry.name) - 1;
    if (keep > 0 && !readExact(file, reinterpret_cast<uint8_t*>(entry.name), keep)) {
        return false;
    }
    entry.name[keep] = '\0';
    const size_t skip = static_cast<size_t>(nameLen - keep) + extraLen;
    if (skip > 0 && !file.seek(skip, SeekCur)) {
        return false;
    }
    const size_t pos = file.position();
    if (pos == static_cast<size_t>(-1)) {
        return false;
    }
    entry.dataOffset = static_cast<uint32_t>(pos);
    return true;
}

struct ByteBuf {
    char* data = nullptr;
    size_t cap = 0;
    size_t len = 0;

    bool full() const { return len >= cap; }

    void feed(uint8_t b) {
        if (len < cap) {
            data[len++] = static_cast<char>(b);
        }
    }
};

template <typename Sink>
bool feedStored(File& file, uint32_t length, Sink& sink) {
    uint8_t buf[128];
    uint32_t left = length;
    while (left > 0 && !sink.full()) {
        const size_t want = left < sizeof(buf) ? left : sizeof(buf);
        const size_t n = file.read(buf, want);
        if (n == 0) {
            return false;
        }
        left -= n;
        for (size_t i = 0; i < n && !sink.full(); i++) {
            sink.feed(buf[i]);
        }
    }
    if (left > 0 && !file.seek(left, SeekCur)) {
        return false;
    }
    return true;
}

template <typename Sink>
bool feedDeflate(File& file, uint32_t compSize, Sink& sink) {
    if (compSize == 0) {
        return true;
    }

    tinfl_decompressor* decomp = static_cast<tinfl_decompressor*>(malloc(sizeof(tinfl_decompressor)));
    uint8_t* dict = static_cast<uint8_t*>(malloc(TINFL_LZ_DICT_SIZE));
    if (decomp == nullptr || dict == nullptr) {
        free(decomp);
        free(dict);
        Serial.println("Out of memory");
        return false;
    }

    memset(decomp, 0, sizeof(*decomp));
    tinfl_init(decomp);

    // Unconsumed compressed bytes live at the front of inBuf.
    uint8_t inBuf[512];
    size_t inLen = 0;
    size_t dictOfs = 0;
    uint32_t compLeft = compSize;
    bool ok = true;

    auto refill = [&]() -> bool {
        if (compLeft == 0) {
            return inLen > 0;
        }
        if (inLen == sizeof(inBuf)) {
            return true;
        }
        const size_t space = sizeof(inBuf) - inLen;
        const size_t want = compLeft < space ? compLeft : space;
        const size_t n = file.read(inBuf + inLen, want);
        if (n == 0) {
            return false;
        }
        inLen += n;
        compLeft -= static_cast<uint32_t>(n);
        return true;
    };

    if (!refill()) {
        ok = false;
    }

    while (ok && !sink.full()) {
        size_t inBytes = inLen;
        size_t outBytes = TINFL_LZ_DICT_SIZE - dictOfs;
        const mz_uint32 flags = compLeft > 0 ? TINFL_FLAG_HAS_MORE_INPUT : 0;
        const tinfl_status status = tinfl_decompress(
            decomp, inBuf, &inBytes, dict, dict + dictOfs, &outBytes, flags);

        if (inBytes > inLen || (inBytes == 0 && outBytes == 0)) {
            ok = false;
            break;
        }

        if (inBytes < inLen) {
            memmove(inBuf, inBuf + inBytes, inLen - inBytes);
        }
        inLen -= inBytes;

        for (size_t i = 0; i < outBytes && !sink.full(); i++) {
            sink.feed(dict[dictOfs + i]);
        }
        dictOfs = (dictOfs + outBytes) & (TINFL_LZ_DICT_SIZE - 1);

        if (sink.full() || status == TINFL_STATUS_DONE) {
            break;
        }
        if (status == TINFL_STATUS_NEEDS_MORE_INPUT) {
            if (!refill()) {
                ok = false;
            }
            continue;
        }
        if (status != TINFL_STATUS_HAS_MORE_OUTPUT) {
            ok = false;
        }
    }

    if (ok && !sink.full() && compLeft > 0 && !file.seek(compLeft, SeekCur)) {
        ok = false;
    }

    free(decomp);
    free(dict);
    return ok || sink.full();
}

bool skipEntry(File& file, uint32_t compSize) {
    return compSize == 0 || file.seek(compSize, SeekCur);
}

bool endsWith(const char* name, const char* suffix) {
    const size_t nameLen = strlen(name);
    const size_t suffixLen = strlen(suffix);
    return nameLen >= suffixLen && strcasecmp(name + nameLen - suffixLen, suffix) == 0;
}

struct Chapter {
    uint32_t dataOffset = 0;
    uint32_t compSize = 0;
    uint32_t uncompSize = 0;
    uint16_t method = 0;
};

struct ManifestItem {
    char id[48] = {};
    char href[96] = {};
};

std::vector<Chapter> chapters;
bool indexed = false;
String indexedPath;

bool readZipItems(File& file, std::vector<ZipItem>& items) {
    if (!file.seek(0)) {
        return false;
    }
    while (true) {
        ZipItem item;
        if (!readEntry(file, item)) {
            break;
        }
        items.push_back(item);
        if (!skipEntry(file, item.compSize)) {
            break;
        }
    }
    return !items.empty();
}

bool readAttr(const char* start, const char* end, const char* key, char* out, size_t outLen) {
    const size_t keyLen = strlen(key);
    for (const char* p = start; p + keyLen + 2 < end; p++) {
        if (p > start && p[-1] != ' ' && p[-1] != '\n' && p[-1] != '\t' && p[-1] != '\r') {
            continue;
        }
        if (memcmp(p, key, keyLen) != 0 || p[keyLen] != '=' || p[keyLen + 1] != '"') {
            continue;
        }
        const char* value = p + keyLen + 2;
        const void* quote = memchr(value, '"', static_cast<size_t>(end - value));
        if (quote == nullptr) {
            return false;
        }
        size_t n = static_cast<const char*>(quote) - value;
        if (n >= outLen) {
            n = outLen - 1;
        }
        memcpy(out, value, n);
        out[n] = '\0';
        return true;
    }
    return false;
}

bool feedEntry(File& file, const ZipItem& item, TextPrinter& printer) {
    if (!file.seek(item.dataOffset)) {
        return false;
    }
    if (item.method == 0) {
        return feedStored(file, item.compSize, printer);
    }
    if (item.method == 8) {
        return feedDeflate(file, item.compSize, printer);
    }
    Serial.println("Unsupported compression");
    return false;
}

bool entryHasText(File& file, const ZipItem& item) {
    TextPrinter printer(nullptr, true);
    if (!feedEntry(file, item, printer)) {
        return false;
    }
    return printer.sawText();
}

Chapter toChapter(const ZipItem& item) {
    Chapter chapter;
    chapter.dataOffset = item.dataOffset;
    chapter.compSize = item.compSize;
    chapter.uncompSize = item.uncompSize;
    chapter.method = item.method;
    return chapter;
}

const ZipItem* findItem(const std::vector<ZipItem>& items, const char* name) {
    for (const ZipItem& item : items) {
        if (strcmp(item.name, name) == 0) {
            return &item;
        }
    }
    return nullptr;
}

bool joinOpfPath(const char* opfName, const char* href, char* out, size_t outLen) {
    if (href[0] == '\0') {
        return false;
    }
    const char* slash = strrchr(opfName, '/');
    const size_t prefix = slash == nullptr ? 0 : static_cast<size_t>(slash - opfName + 1);
    if (href[0] == '/') {
        href++;
    }
    if (prefix + strlen(href) + 1 > outLen) {
        return false;
    }
    memcpy(out, opfName, prefix);
    memcpy(out + prefix, href, strlen(href) + 1);
    return true;
}

bool readRaw(File& file, const ZipItem& item, char*& out, size_t& outLen) {
    out = nullptr;
    outLen = 0;
    if (item.uncompSize == 0 || item.uncompSize > 256 * 1024) {
        return false;
    }
    out = static_cast<char*>(malloc(item.uncompSize + 1));
    if (out == nullptr) {
        Serial.println("Out of memory");
        return false;
    }
    if (!file.seek(item.dataOffset)) {
        free(out);
        out = nullptr;
        return false;
    }

    ByteBuf buf{out, item.uncompSize, 0};
    bool ok = false;
    if (item.method == 0) {
        ok = feedStored(file, item.compSize, buf);
    } else if (item.method == 8) {
        ok = feedDeflate(file, item.compSize, buf);
    }
    if (!ok) {
        free(out);
        out = nullptr;
        return false;
    }
    out[buf.len] = '\0';
    outLen = buf.len;
    return true;
}

void parseManifest(const char* xml, std::vector<ManifestItem>& manifest) {
    const char* p = xml;
    while ((p = strstr(p, "<item")) != nullptr) {
        if (strncmp(p, "<itemref", 8) == 0) {
            p += 8;
            continue;
        }
        const char* end = strchr(p, '>');
        if (end == nullptr) {
            break;
        }
        ManifestItem item;
        if (readAttr(p, end, "href", item.href, sizeof(item.href)) &&
            readAttr(p, end, "id", item.id, sizeof(item.id)) &&
            endsWithHtml(item.href)) {
            manifest.push_back(item);
        }
        p = end + 1;
    }
}

bool addSpineChapters(File& file, const std::vector<ZipItem>& items, const ZipItem& opf) {
    char* xml = nullptr;
    size_t xmlLen = 0;
    if (!readRaw(file, opf, xml, xmlLen)) {
        return false;
    }
    (void)xmlLen;

    std::vector<ManifestItem> manifest;
    parseManifest(xml, manifest);

    bool sawRef = false;
    const char* p = xml;
    while ((p = strstr(p, "<itemref")) != nullptr) {
        const char* end = strchr(p, '>');
        if (end == nullptr) {
            break;
        }
        sawRef = true;
        char idref[48];
        if (readAttr(p, end, "idref", idref, sizeof(idref))) {
            for (const ManifestItem& doc : manifest) {
                if (strcmp(doc.id, idref) != 0) {
                    continue;
                }
                char full[160];
                if (!joinOpfPath(opf.name, doc.href, full, sizeof(full))) {
                    break;
                }
                const ZipItem* item = findItem(items, full);
                if (item != nullptr && entryHasText(file, *item)) {
                    chapters.push_back(toChapter(*item));
                }
                break;
            }
        }
        p = end + 1;
    }

    free(xml);
    return sawRef;
}

bool ensureIndex(const char* path) {
    if (indexed && indexedPath == path) {
        return true;
    }

    chapters.clear();
    indexed = false;
    indexedPath = path;

    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.println("Failed to open epub.");
        indexedPath = "";
        return false;
    }

    std::vector<ZipItem> items;
    if (!readZipItems(file, items)) {
        file.close();
        indexedPath = "";
        return false;
    }

    const ZipItem* opf = nullptr;
    for (const ZipItem& item : items) {
        if (endsWith(item.name, ".opf")) {
            opf = &item;
            break;
        }
    }

    bool fromSpine = false;
    if (opf != nullptr) {
        fromSpine = addSpineChapters(file, items, *opf);
    }
    if (!fromSpine) {
        for (const ZipItem& item : items) {
            if (endsWithHtml(item.name) && entryHasText(file, item)) {
                chapters.push_back(toChapter(item));
            }
        }
    }

    file.close();
    indexed = true;
    return true;
}

struct FileSink {
    File* out = nullptr;
    uint8_t buf[256] = {};
    size_t len = 0;
    bool ok = true;

    bool full() const { return !ok; }

    void feed(uint8_t b) {
        if (!ok) {
            return;
        }
        buf[len++] = b;
        if (len == sizeof(buf)) {
            flush();
        }
    }

    void flush() {
        if (ok && len > 0 && out->write(buf, len) != len) {
            ok = false;
        }
        len = 0;
    }
};

bool copyStored(File& epub, uint32_t offset, uint32_t size, File& dest) {
    if (!epub.seek(offset)) {
        return false;
    }
    uint8_t buf[256];
    uint32_t left = size;
    while (left > 0) {
        const size_t want = left < sizeof(buf) ? left : sizeof(buf);
        const size_t n = epub.read(buf, want);
        if (n == 0) {
            return false;
        }
        if (dest.write(buf, n) != n) {
            return false;
        }
        left -= n;
    }
    return true;
}

bool writeEntry(File& epub, const ZipItem& item, File& dest) {
    if (item.method == 0) {
        return copyStored(epub, item.dataOffset, item.compSize, dest);
    }
    if (item.method != 8 || !epub.seek(item.dataOffset)) {
        return false;
    }
    FileSink sink;
    sink.out = &dest;
    const bool ok = feedDeflate(epub, item.compSize, sink);
    sink.flush();
    return ok && sink.ok;
}

bool hrefOfId(const char* xml, const char* id, char* href, size_t hrefLen) {
    const char* p = xml;
    while ((p = strstr(p, "<item")) != nullptr) {
        if (strncmp(p, "<itemref", 8) == 0) {
            p += 8;
            continue;
        }
        const char* end = strchr(p, '>');
        if (end == nullptr) {
            break;
        }
        char itemId[48];
        if (readAttr(p, end, "id", itemId, sizeof(itemId)) && strcmp(itemId, id) == 0) {
            return readAttr(p, end, "href", href, hrefLen);
        }
        p = end + 1;
    }
    return false;
}

bool findCoverHref(const char* xml, char* href, size_t hrefLen) {
    const char* p = xml;
    while ((p = strstr(p, "<item")) != nullptr) {
        if (strncmp(p, "<itemref", 8) == 0) {
            p += 8;
            continue;
        }
        const char* end = strchr(p, '>');
        if (end == nullptr) {
            break;
        }
        char props[80];
        if (readAttr(p, end, "properties", props, sizeof(props)) && strstr(props, "cover-image") != nullptr &&
            readAttr(p, end, "href", href, hrefLen)) {
            return true;
        }
        p = end + 1;
    }

    p = xml;
    while ((p = strstr(p, "<meta")) != nullptr) {
        const char* end = strchr(p, '>');
        if (end == nullptr) {
            break;
        }
        char name[32];
        char content[48];
        if (readAttr(p, end, "name", name, sizeof(name)) && strcmp(name, "cover") == 0 &&
            readAttr(p, end, "content", content, sizeof(content)) &&
            hrefOfId(xml, content, href, hrefLen)) {
            return true;
        }
        p = end + 1;
    }
    return false;
}

bool findCover(const char* path, EpubCover& cover) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }

    std::vector<ZipItem> items;
    if (!readZipItems(file, items)) {
        file.close();
        return false;
    }

    const ZipItem* opf = nullptr;
    for (const ZipItem& item : items) {
        if (endsWith(item.name, ".opf")) {
            opf = &item;
            break;
        }
    }
    if (opf == nullptr) {
        file.close();
        return false;
    }

    char* xml = nullptr;
    size_t xmlLen = 0;
    if (!readRaw(file, *opf, xml, xmlLen)) {
        file.close();
        return false;
    }

    char href[96];
    const bool foundHref = findCoverHref(xml, href, sizeof(href));
    free(xml);
    if (!foundHref) {
        file.close();
        return false;
    }

    char full[160];
    if (!joinOpfPath(opf->name, href, full, sizeof(full))) {
        file.close();
        return false;
    }
    const ZipItem* image = findItem(items, full);
    file.close();
    if (image == nullptr || image->compSize == 0) {
        return false;
    }

    cover.method = image->method;
    cover.offset = image->dataOffset;
    cover.size = image->compSize;
    return true;
}

bool extractCover(const char* epubPath, const EpubCover& cover, const char* destPath) {
    File epub = LittleFS.open(epubPath, "r");
    if (!epub) {
        return false;
    }
    File dest = LittleFS.open(destPath, "w");
    if (!dest) {
        epub.close();
        return false;
    }

    ZipItem item;
    item.method = cover.method;
    item.compSize = cover.size;
    item.dataOffset = cover.offset;
    const bool ok = writeEntry(epub, item, dest);
    dest.close();
    epub.close();
    if (!ok) {
        LittleFS.remove(destPath);
    }
    return ok;
}

}  // namespace

size_t epubChapterCount(const char* path) {
    if (!ensureIndex(path)) {
        return 0;
    }
    return chapters.size();
}

bool loadEpubChapter(const char* path, size_t index, String& text) {
    if (!ensureIndex(path) || index >= chapters.size()) {
        return false;
    }

    const Chapter& chapter = chapters[index];
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.println("Failed to open epub.");
        return false;
    }

    text = "";
    if (chapter.uncompSize > 0) {
        text.reserve(chapter.uncompSize);
    }

    ZipItem item;
    item.method = chapter.method;
    item.compSize = chapter.compSize;
    item.uncompSize = chapter.uncompSize;
    item.dataOffset = chapter.dataOffset;
    TextPrinter printer(&text, false);
    const bool ok = feedEntry(file, item, printer);
    file.close();
    if (!ok) {
        text = "";
        Serial.println("Failed to read chapter");
    }
    return ok;
}

bool epubFindCover(const char* path, EpubCover& cover) {
    return findCover(path, cover);
}

bool epubExtract(const char* epubPath, const EpubCover& cover, const char* destPath) {
    return extractCover(epubPath, cover, destPath);
}

#ifdef UNIT_TEST
void epubResetForTest() {
    chapters.clear();
    indexed = false;
    indexedPath = "";
}
#endif
