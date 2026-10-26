#pragma once

#include <Arduino.h>
#include <stddef.h>

// Text chapters in spine order. Image-only documents are left out. 0 if the file cannot be read.
size_t epubChapterCount(const char* path);

// Replace `text` with the chapter at `index`.
bool loadEpubChapter(const char* path, size_t index, String& text);
