#pragma once

#include <Arduino.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

enum SeekMode { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };

struct HostRootEntry {
    std::string path;
    bool directory = false;
};

struct HostFs {
    bool mounted = true;
    bool rootSet = false;
    std::map<std::string, std::string> files;
    std::vector<HostRootEntry> root;

    void reset() {
        mounted = true;
        rootSet = false;
        files.clear();
        root.clear();
    }
};

inline HostFs& hostFs() {
    static HostFs fs;
    return fs;
}

class File {
public:
    File() = default;

    explicit operator bool() const { return state_ != nullptr && state_->open; }

    size_t read(uint8_t* dst, size_t length) {
        if (!*this || dst == nullptr || state_->pos >= state_->bytes.size()) {
            return 0;
        }
        const size_t n = length < state_->bytes.size() - state_->pos ? length : state_->bytes.size() - state_->pos;
        memcpy(dst, state_->bytes.data() + state_->pos, n);
        state_->pos += n;
        return n;
    }

    size_t write(const uint8_t* src, size_t length) {
        if (!*this || !state_->writable || src == nullptr) {
            return 0;
        }
        if (state_->pos + length > state_->bytes.size()) {
            state_->bytes.resize(state_->pos + length);
        }
        memcpy(&state_->bytes[state_->pos], src, length);
        state_->pos += length;
        hostFs().files[state_->path] = state_->bytes;
        return length;
    }

    bool seek(uint64_t pos, SeekMode mode = SeekSet) {
        if (!*this) {
            return false;
        }
        size_t next = 0;
        if (mode == SeekSet) {
            next = static_cast<size_t>(pos);
        } else if (mode == SeekCur) {
            next = state_->pos + static_cast<size_t>(pos);
        } else {
            if (pos > state_->bytes.size()) {
                return false;
            }
            next = state_->bytes.size() - static_cast<size_t>(pos);
        }
        if (next > state_->bytes.size()) {
            return false;
        }
        state_->pos = next;
        return true;
    }

    size_t position() const {
        if (!*this) {
            return static_cast<size_t>(-1);
        }
        return state_->pos;
    }

    int available() const {
        if (!*this || state_->pos >= state_->bytes.size()) {
            return 0;
        }
        return static_cast<int>(state_->bytes.size() - state_->pos);
    }

    String readStringUntil(char end) {
        std::string line;
        if (!*this) {
            return String();
        }
        while (state_->pos < state_->bytes.size()) {
            const char c = state_->bytes[state_->pos++];
            if (c == end) {
                break;
            }
            line.push_back(c);
        }
        return String(line);
    }

    const char* path() const {
        if (!*this) {
            return nullptr;
        }
        return state_->path.c_str();
    }

    bool isDirectory() const { return *this && state_->directory; }

    File openNextFile() {
        if (!*this || !state_->directory || state_->child >= state_->children.size()) {
            return File();
        }
        const HostRootEntry& entry = state_->children[state_->child++];
        File next;
        next.state_ = std::make_shared<State>();
        next.state_->open = true;
        next.state_->directory = entry.directory;
        next.state_->path = entry.path;
        if (!entry.directory) {
            const auto found = hostFs().files.find(entry.path);
            if (found != hostFs().files.end()) {
                next.state_->bytes = found->second;
            }
        }
        return next;
    }

    void close() {
        if (state_ != nullptr) {
            state_->open = false;
        }
    }

    void print(const char* value) { write(reinterpret_cast<const uint8_t*>(value), value == nullptr ? 0 : strlen(value)); }
    void print(const String& value) { print(value.c_str()); }
    void print(char value) { write(reinterpret_cast<const uint8_t*>(&value), 1); }
    void print(int value) { printNumber(static_cast<long long>(value)); }
    void print(unsigned value) { printNumber(value); }
    void print(long value) { printNumber(value); }
    void print(unsigned long value) { printNumber(value); }
    void print(unsigned long long value) { printNumber(value); }

    template <typename T>
    void println(const T& value) {
        print(value);
        print('\n');
    }

    static File readable(const std::string& path, const std::string& bytes) {
        File file;
        file.state_ = std::make_shared<State>();
        file.state_->open = true;
        file.state_->path = path;
        file.state_->bytes = bytes;
        return file;
    }

    static File writable(const std::string& path) {
        File file;
        file.state_ = std::make_shared<State>();
        file.state_->open = true;
        file.state_->writable = true;
        file.state_->path = path;
        return file;
    }

    static File directory(const std::vector<HostRootEntry>& children) {
        File file;
        file.state_ = std::make_shared<State>();
        file.state_->open = true;
        file.state_->directory = true;
        file.state_->path = "/";
        file.state_->children = children;
        return file;
    }

private:
    struct State {
        bool open = false;
        bool writable = false;
        bool directory = false;
        std::string path;
        std::string bytes;
        size_t pos = 0;
        size_t child = 0;
        std::vector<HostRootEntry> children;
    };

    template <typename T>
    void printNumber(T value) {
        const std::string text = std::to_string(value);
        write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    }

    std::shared_ptr<State> state_;
};

class FS {
public:
    bool begin() { return hostFs().mounted; }

    File open(const char* path, const char* mode = "r") {
        if (!hostFs().mounted || path == nullptr) {
            return File();
        }
        const bool write = mode != nullptr && mode[0] == 'w';
        if (std::string(path) == "/") {
            if (write) {
                return File();
            }
            std::vector<HostRootEntry> children = hostFs().root;
            if (!hostFs().rootSet) {
                children.clear();
                for (const auto& item : hostFs().files) {
                    children.push_back(HostRootEntry{item.first, false});
                }
            }
            return File::directory(children);
        }
        if (write) {
            hostFs().files[path] = "";
            return File::writable(path);
        }
        const auto found = hostFs().files.find(path);
        if (found == hostFs().files.end()) {
            return File();
        }
        return File::readable(path, found->second);
    }

    bool remove(const char* path) {
        if (path == nullptr) {
            return false;
        }
        return hostFs().files.erase(path) > 0;
    }
};

inline FS LittleFS;
