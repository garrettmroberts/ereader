#pragma once

struct HostSpi {
    int begins = 0;
    int sclk = -1;
    int miso = -1;
    int mosi = -1;
    int ss = -1;

    void reset() { *this = {}; }
};

inline HostSpi hostSpi;

class SPIClass {
public:
    void begin(int sclk, int miso, int mosi, int ss) {
        hostSpi.begins++;
        hostSpi.sclk = sclk;
        hostSpi.miso = miso;
        hostSpi.mosi = mosi;
        hostSpi.ss = ss;
    }
};

inline SPIClass SPI;
