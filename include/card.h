#pragma once

#ifdef UNIT_TEST
#include <LittleFS.h>
#else
#include <FS.h>
#endif

#include <vector>

// Mounts the microSD reader. Every book is read from the card.
bool mountCard();
bool cardMounted();
FS& bookFs();

// Raise the e-paper chip-select so the panel does not answer on the shared SPI bus.
void releaseDisplayBus();

struct CardEntry {
    String name;
    bool directory = false;
};

// Child names of a card directory. `path` is "/" or "/Author".
// Names are collected before any book file is opened.
bool readCardDirectory(const char* path, std::vector<CardEntry>& out);
