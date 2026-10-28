#pragma once

// E-paper and the microSD reader share SCLK and MOSI.
constexpr int kEpdCs = 16;
constexpr int kEpdDc = 17;
constexpr int kEpdRst = 21;
constexpr int kEpdBusy = 22;
constexpr int kSpiSclk = 18;
constexpr int kSpiMosi = 23;
constexpr int kSpiMiso = 19;
constexpr int kSdCs = 25;
