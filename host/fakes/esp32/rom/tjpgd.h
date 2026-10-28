#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

struct JDEC {
    unsigned int width = 0;
    unsigned int height = 0;
    void* device = nullptr;
};

typedef struct {
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
} JRECT;

typedef enum {
    JDR_OK = 0,
    JDR_INTR,
    JDR_INP,
    JDR_MEM1,
    JDR_MEM2,
    JDR_PAR,
    JDR_FMT1,
    JDR_FMT2,
    JDR_FMT3
} JRESULT;

extern "C" {
typedef unsigned int (*jpeg_infunc_t)(JDEC*, unsigned char*, unsigned int);
typedef unsigned int (*jpeg_outfunc_t)(JDEC*, void*, JRECT*);
}

// Stand-in for the ROM JPEG decoder. A test image is the bytes TJPG, a
// little-endian width and height, then RGB pixels. `exerciseSkip` asks the
// input callback to discard that many leading bytes with a null buffer.
struct HostJpeg {
    int prepares = 0;
    int decomps = 0;
    bool failPrepare = false;
    bool failDecomp = false;
    unsigned exerciseSkip = 0;
    unsigned srcW = 0;
    unsigned srcH = 0;
    std::vector<uint8_t> rgb;
};

inline HostJpeg hostJpeg;

inline JRESULT jd_prepare(JDEC* jd, jpeg_infunc_t infunc, void*, unsigned int, void* dev) {
    hostJpeg.prepares++;
    jd->device = dev;
    jd->width = 0;
    jd->height = 0;
    hostJpeg.rgb.clear();
    hostJpeg.srcW = 0;
    hostJpeg.srcH = 0;

    infunc(jd, nullptr, 0);
    if (hostJpeg.exerciseSkip > 0 && infunc(jd, nullptr, hostJpeg.exerciseSkip) == 0) {
        return JDR_INP;
    }

    std::string data;
    unsigned char buf[32];
    for (;;) {
        const unsigned int n = infunc(jd, buf, sizeof(buf));
        if (n == 0) {
            break;
        }
        data.append(reinterpret_cast<char*>(buf), n);
    }
    if (hostJpeg.failPrepare || data.size() < 8 || data.compare(0, 4, "TJPG") != 0) {
        return JDR_FMT1;
    }

    const auto raw = reinterpret_cast<const unsigned char*>(data.data());
    const unsigned width = raw[4] | (raw[5] << 8);
    const unsigned height = raw[6] | (raw[7] << 8);
    if (width == 0 || height == 0) {
        return JDR_FMT1;
    }
    jd->width = width;
    jd->height = height;
    hostJpeg.srcW = width;
    hostJpeg.srcH = height;
    hostJpeg.rgb.assign(raw + 8, raw + data.size());
    return JDR_OK;
}

inline JRESULT jd_decomp(JDEC* jd, jpeg_outfunc_t outfunc, uint8_t scale) {
    hostJpeg.decomps++;
    if (hostJpeg.failDecomp) {
        return JDR_INP;
    }

    const unsigned width = jd->width >> scale;
    const unsigned height = jd->height >> scale;
    if (width == 0 || height == 0 || outfunc == nullptr) {
        return JDR_OK;
    }

    std::vector<uint8_t> bitmap(static_cast<size_t>(width) * height * 3, 255);
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            const size_t src = (static_cast<size_t>(y << scale) * hostJpeg.srcW + (x << scale)) * 3;
            const size_t dst = (static_cast<size_t>(y) * width + x) * 3;
            if (src + 2 < hostJpeg.rgb.size()) {
                bitmap[dst] = hostJpeg.rgb[src];
                bitmap[dst + 1] = hostJpeg.rgb[src + 1];
                bitmap[dst + 2] = hostJpeg.rgb[src + 2];
            }
        }
    }

    JRECT rect{0, static_cast<uint16_t>(width - 1), 0, static_cast<uint16_t>(height - 1)};
    if (outfunc(jd, bitmap.data(), &rect) == 0) {
        return JDR_INTR;
    }
    return JDR_OK;
}
