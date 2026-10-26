#pragma once

#include <Arduino.h>
#include <vector>

void initDisplay();

// Byte offsets where each screen starts. The next screen starts at the following offset.
std::vector<size_t> paginate(const String& text);

// `fullRefresh` forces a full panel update. Otherwise one full update is followed by two partial updates.
void drawPage(const String& text, size_t offset, bool fullRefresh = false);

// One stored book, with its cover when the EPUB has one. `index` is zero-based.
void drawLibrary(const char* epubPath, size_t index, size_t count, bool fullRefresh = false);
