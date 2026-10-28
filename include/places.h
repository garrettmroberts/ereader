#pragma once

#include <Arduino.h>

void loadPlaces();
void remember(const String& path, size_t chapter, size_t page);
void savedPlace(const String& path, size_t& chapter, size_t& page);
