#include "card.h"
#include "pins.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include <dirent.h>
#include <driver/gpio.h>
#include <esp32-hal-dac.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef DT_DIR
#define DT_UNKNOWN 0
#define DT_REG 1
#define DT_DIR 2
#endif

namespace {

bool mounted = false;

constexpr uint32_t kSdSpiHz = 15000000;

void idleSelect() {
    pinMode(kEpdCs, OUTPUT);
    digitalWrite(kEpdCs, HIGH);
    pinMode(kSdCs, OUTPUT);
    digitalWrite(kSdCs, HIGH);
}

}  // namespace

void releaseDisplayBus() {
    idleSelect();
}

bool mountCard() {
    releaseDisplayBus();
    dacDisable(kSdCs);
    gpio_pullup_en(static_cast<gpio_num_t>(kSpiMiso));

    mounted = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (attempt > 0) {
            delay(200);
        }
        if (SD.begin(kSdCs, SPI, kSdSpiHz, "/sd", 10)) {
            mounted = true;
            Serial.println("microSD mounted");
            return true;
        }
    }

    Serial.println("No microSD card");
    return false;
}

bool cardMounted() {
    return mounted;
}

FS& bookFs() {
    return SD;
}

namespace {

#ifdef UNIT_TEST
const char* cardVfsRoot = "/sd";
#else
constexpr const char* cardVfsRoot = "/sd";
#endif

void cardVfsPath(const char* path, char* vfs, size_t size) {
    const bool root = path == nullptr || path[0] == '\0' || (path[0] == '/' && path[1] == '\0');
    snprintf(vfs, size, "%s%s", cardVfsRoot, root ? "" : path);
}

bool joinChild(const char* parent, const char* name, char* out, size_t size) {
    if (parent == nullptr || name == nullptr || size == 0) {
        return false;
    }
    const size_t parentLen = strlen(parent);
    const size_t nameLen = strlen(name);
    if (parentLen + 1 + nameLen + 1 > size) {
        return false;
    }
    memcpy(out, parent, parentLen);
    out[parentLen] = '/';
    memcpy(out + parentLen + 1, name, nameLen + 1);
    return true;
}

}  // namespace

bool readCardDirectory(const char* path, std::vector<CardEntry>& out) {
    releaseDisplayBus();
    char vfs[768];
    cardVfsPath(path, vfs, sizeof(vfs));

    DIR* dir = opendir(vfs);
    if (dir == nullptr) {
        Serial.print("opendir failed ");
        Serial.println(vfs);
        return false;
    }

    // Take every name before opening or stating anything. The Arduino
    // directory iterator opens each child while the folder is still open,
    // and those opens fail, so the scan stops before any epub is recorded.
    std::vector<String> names;
    std::vector<uint8_t> types;
    while (dirent* entry = readdir(dir)) {
        if (entry->d_name[0] == '\0' || entry->d_name[0] == '.') {
            continue;
        }
        names.push_back(String(entry->d_name));
        types.push_back(entry->d_type);
        Serial.print(" raw ");
        Serial.print(vfs);
        Serial.print('/');
        Serial.print(entry->d_name);
        Serial.print(" d_type ");
        Serial.println(static_cast<int>(entry->d_type));
    }
    closedir(dir);

    for (size_t i = 0; i < names.size(); ++i) {
        CardEntry item;
        item.name = names[i];
        item.directory = types[i] == DT_DIR;
        if (types[i] != DT_DIR && types[i] != DT_REG) {
            char child[1024];
            struct stat info;
            if (joinChild(vfs, names[i].c_str(), child, sizeof(child)) && stat(child, &info) == 0) {
                item.directory = S_ISDIR(info.st_mode);
            }
        }
        out.push_back(item);
    }
    return true;
}

#ifdef UNIT_TEST
void cardSetRootForTest(const char* root) {
    cardVfsRoot = (root == nullptr || root[0] == '\0') ? "/sd" : root;
}
#endif
