#pragma once

#include <Arduino.h>
#include <vector>

void initDisplay();

// Byte offsets where each screen starts. The next screen starts at the following offset.
std::vector<size_t> paginate(const String& text);

#ifdef UNIT_TEST
void displayResetForTest();
#endif
