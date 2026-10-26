#pragma once

#include <Arduino.h>
#include <stddef.h>

// Text chapters in spine order. Image-only documents are left out. 0 if the file cannot be read.
size_t epubChapterCount(const char* path);

// Replace `text` with the chapter at `index`.
bool loadEpubChapter(const char* path, size_t index, String& text);

// JPEG (or other) cover inside the EPUB. `size` is the compressed entry length at `offset`.
struct EpubCover {
    uint16_t method;
    uint32_t offset;
    uint32_t size;
};

bool epubFindCover(const char* path, EpubCover& cover);

// Write a cover entry to `destPath`. Used when the image is deflated inside the zip.
bool epubExtract(const char* epubPath, const EpubCover& cover, const char* destPath);
