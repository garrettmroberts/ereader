#pragma once

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#define HIGH 1
#define LOW 0
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05

struct HostPinMode {
    int pin;
    int mode;
};

inline unsigned long hostMillis = 0;
inline int hostPins[64] = {};
inline std::vector<HostPinMode> hostPinModes;

inline void hostResetPins() {
    for (int& level : hostPins) {
        level = HIGH;
    }
    hostPinModes.clear();
}

inline void hostSetPin(int pin, int level) {
    if (pin >= 0 && pin < static_cast<int>(sizeof(hostPins) / sizeof(hostPins[0]))) {
        hostPins[pin] = level;
    }
}

inline unsigned long millis() {
    return hostMillis;
}

inline void delay(unsigned long) {}

inline int digitalRead(int pin) {
    if (pin < 0 || pin >= static_cast<int>(sizeof(hostPins) / sizeof(hostPins[0]))) {
        return HIGH;
    }
    return hostPins[pin];
}

inline void pinMode(int pin, int mode) {
    hostPinModes.push_back(HostPinMode{pin, mode});
}

class String {
public:
    String() = default;
    String(const char* value) : data_(value == nullptr ? "" : value) {}
    String(const std::string& value) : data_(value) {}

    const char* c_str() const { return data_.c_str(); }
    size_t length() const { return data_.size(); }
    void reserve(size_t bytes) { data_.reserve(bytes); }
    void concat(char c) { data_.push_back(c); }

    bool operator==(const String& other) const { return data_ == other.data_; }
    bool operator<(const String& other) const { return data_ < other.data_; }

    bool startsWith(const char* prefix) const {
        if (prefix == nullptr) {
            return false;
        }
        const size_t n = strlen(prefix);
        return data_.size() >= n && data_.compare(0, n, prefix) == 0;
    }

    int indexOf(char c) const { return indexOf(c, 0); }

    int indexOf(char c, int from) const {
        if (from < 0) {
            from = 0;
        }
        const auto found = data_.find(c, static_cast<size_t>(from));
        return found == std::string::npos ? -1 : static_cast<int>(found);
    }

    String substring(int from) const {
        if (from < 0) {
            from = 0;
        }
        if (static_cast<size_t>(from) >= data_.size()) {
            return String();
        }
        return String(data_.substr(static_cast<size_t>(from)));
    }

    String substring(int from, int to) const {
        if (from < 0) {
            from = 0;
        }
        if (to < from) {
            to = from;
        }
        if (static_cast<size_t>(from) >= data_.size()) {
            return String();
        }
        if (static_cast<size_t>(to) > data_.size()) {
            to = static_cast<int>(data_.size());
        }
        return String(data_.substr(static_cast<size_t>(from), static_cast<size_t>(to - from)));
    }

    long toInt() const { return strtol(data_.c_str(), nullptr, 10); }

    friend String operator+(const char* prefix, const String& suffix) {
        return String(std::string(prefix == nullptr ? "" : prefix) + suffix.data_);
    }

private:
    std::string data_;
};

class SerialClass {
public:
    std::string out;

    void begin(unsigned long) {}

    void print(const char* value) { out += value == nullptr ? "" : value; }
    void print(const String& value) { out += value.c_str(); }
    void print(char value) { out.push_back(value); }
    void print(int value) { out += std::to_string(value); }
    void print(unsigned value) { out += std::to_string(value); }
    void print(long value) { out += std::to_string(value); }
    void print(unsigned long value) { out += std::to_string(value); }
    void print(unsigned long long value) { out += std::to_string(value); }

    template <typename T>
    void println(const T& value) {
        print(value);
        out.push_back('\n');
    }

    void println() { out.push_back('\n'); }
};

inline SerialClass Serial;
