#include "paginate.h"

namespace {

bool isWordByte(char c) {
    return c != ' ' && c != '\n' && c != '\r';
}

}  // namespace

PageLayout layoutPage(const char* text, size_t length, size_t offset, int columns, int rows) {
    PageLayout page;
    page.next = offset;

    if (text == nullptr || columns < 1 || rows < 1 || offset > length) {
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
                const size_t wordLen = wordEnd - look;

                if (wordLen > static_cast<size_t>(columns) ||
                    static_cast<size_t>(col) + static_cast<size_t>(spaces) + wordLen >
                        static_cast<size_t>(columns)) {
                    break;
                }

                col += spaces;
                i = look;
                continue;
            }

            size_t wordEnd = i;
            while (wordEnd < length && isWordByte(text[wordEnd])) {
                wordEnd++;
            }
            const size_t wordLen = wordEnd - i;

            if (wordLen > static_cast<size_t>(columns)) {
                if (col > 0) {
                    break;
                }
                i += static_cast<size_t>(columns);
                break;
            }

            if (col > 0 && static_cast<size_t>(col) + wordLen > static_cast<size_t>(columns)) {
                break;
            }

            col += static_cast<int>(wordLen);
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

std::vector<size_t> pageOffsets(const char* text, size_t length, int columns, int rows) {
    std::vector<size_t> pages;
    if (text == nullptr) {
        return pages;
    }

    size_t offset = 0;
    while (offset < length) {
        pages.push_back(offset);
        const size_t next = layoutPage(text, length, offset, columns, rows).next;
        if (next <= offset) {
            break;
        }
        offset = next;
    }
    return pages;
}
