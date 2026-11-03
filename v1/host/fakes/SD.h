#pragma once

#include <LittleFS.h>
#include <SPI.h>

#include <stdint.h>
#include <string>

struct HostSdBegin {
    int calls = 0;
    int failRemaining = 0;
    int cs = -1;
    uint32_t hz = 0;
    std::string mount;
    int maxFiles = 0;

    void reset() { *this = {}; }
};

inline HostSdBegin hostSd;

class SDClass : public FS {
public:
    SDClass() : FS(&hostCard()) {}

    bool begin(int cs, SPIClass&, uint32_t hz, const char* mount, int maxFiles) {
        hostSd.calls++;
        hostSd.cs = cs;
        hostSd.hz = hz;
        hostSd.mount = mount == nullptr ? "" : mount;
        hostSd.maxFiles = maxFiles;
        if (hostSd.failRemaining > 0) {
            hostSd.failRemaining--;
            return false;
        }
        return true;
    }
};

inline SDClass SD;
