#pragma once

#include <cstddef>
#include <vector>

struct TextLine {
    size_t begin;
    size_t end;
};

struct PageLayout {
    std::vector<TextLine> lines;
    size_t next;
};

// Fit text into a grid of character cells. `next` is the byte offset of the following page.
PageLayout layoutPage(const char* text, size_t length, size_t offset, int columns, int rows);

// Byte offset where each page starts.
std::vector<size_t> pageOffsets(const char* text, size_t length, int columns, int rows);
