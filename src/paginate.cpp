#include "paginate.h"

namespace {

bool isWordByte(char c) {
    return c != ' ' && c != '\n' && c != '\r';
}

int measure(const char* text, size_t begin, size_t end, int (*advance)(unsigned char)) {
    int width = 0;
    for (size_t i = begin; i < end; i++) {
        width += advance(static_cast<unsigned char>(text[i]));
    }
    return width;
}

size_t fitWidth(const char* text, size_t begin, size_t end, int maxWidth, int (*advance)(unsigned char)) {
    size_t i = begin;
    int used = 0;
    while (i < end) {
        const int width = advance(static_cast<unsigned char>(text[i]));
        if (i > begin && used + width > maxWidth) {
            break;
        }
        used += width;
        i++;
        if (width > 0 && used >= maxWidth) {
            break;
        }
    }
    if (i == begin && begin < end) {
        i++;
    }
    return i;
}

int unitAdvance(unsigned char c) {
    return c == '\r' ? 0 : 1;
}

}  // namespace

PageLayout layoutFitted(const char* text, size_t length, size_t offset, int maxWidth, int rows,
                        int (*advance)(unsigned char)) {
    PageLayout page;
    page.next = offset;

    if (text == nullptr || advance == nullptr || maxWidth < 1 || rows < 1 || offset > length) {
        return page;
    }

    size_t i = offset;
    int row = 0;

    while (row < rows && i < length) {
        while (i < length && text[i] == '\r') {
            i++;
        }
        if (i >= length) {
            break;
        }

        if (text[i] == '\n') {
            page.lines.push_back(TextLine{i, i});
            i++;
            row++;
            continue;
        }

        size_t lineBegin = i;
        int col = 0;

        while (i < length && text[i] != '\n') {
            if (text[i] == '\r') {
                i++;
                continue;
            }

            if (text[i] == ' ') {
                size_t look = i;
                int spaces = 0;
                while (look < length && (text[look] == ' ' || text[look] == '\r')) {
                    if (text[look] == ' ') {
                        spaces++;
                    }
                    look++;
                }

                if (look >= length || text[look] == '\n') {
                    i = look;
                    break;
                }

                if (col == 0) {
                    i = look;
                    lineBegin = i;
                    continue;
                }

                size_t wordEnd = look;
                while (wordEnd < length && isWordByte(text[wordEnd])) {
                    wordEnd++;
                }
                const int wordWidth = measure(text, look, wordEnd, advance);
                const int gap = advance(' ') * spaces;

                if (wordWidth > maxWidth || col + gap + wordWidth > maxWidth) {
                    break;
                }

                col += gap;
                i = look;
                continue;
            }

            size_t wordEnd = i;
            while (wordEnd < length && isWordByte(text[wordEnd])) {
                wordEnd++;
            }
            const int wordWidth = measure(text, i, wordEnd, advance);

            if (wordWidth > maxWidth) {
                if (col > 0) {
                    break;
                }
                i = fitWidth(text, i, wordEnd, maxWidth, advance);
                break;
            }

            if (col > 0 && col + wordWidth > maxWidth) {
                break;
            }

            col += wordWidth;
            i = wordEnd;
        }

        size_t lineEnd = i;
        while (lineEnd > lineBegin && (text[lineEnd - 1] == ' ' || text[lineEnd - 1] == '\r')) {
            lineEnd--;
        }

        const bool atNewline = i < length && text[i] == '\n';
        if (lineEnd > lineBegin || atNewline) {
            page.lines.push_back(TextLine{lineBegin, lineEnd});
            row++;
            if (atNewline) {
                i++;
            }
        } else {
            break;
        }
    }

    while (i < length && (text[i] == ' ' || text[i] == '\r')) {
        i++;
    }

    page.next = i;
    return page;
}

PageLayout layoutPage(const char* text, size_t length, size_t offset, int columns, int rows) {
    return layoutFitted(text, length, offset, columns, rows, unitAdvance);
}

std::vector<size_t> pageOffsetsFitted(const char* text, size_t length, int maxWidth, int rows,
                                      int (*advance)(unsigned char)) {
    std::vector<size_t> pages;
    if (text == nullptr) {
        return pages;
    }

    size_t offset = 0;
    while (offset < length) {
        pages.push_back(offset);
        const size_t next = layoutFitted(text, length, offset, maxWidth, rows, advance).next;
        if (next <= offset) {
            break;
        }
        offset = next;
    }
    return pages;
}

std::vector<size_t> pageOffsets(const char* text, size_t length, int columns, int rows) {
    return pageOffsetsFitted(text, length, columns, rows, unitAdvance);
}
