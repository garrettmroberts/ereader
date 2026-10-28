#include "library.h"

#include "card.h"
#include "places.h"

#include <algorithm>
#include <cstring>
#include <stdio.h>
#include <strings.h>

namespace {

bool hasSuffix(const char* path, size_t length, const char* suffix) {
    const size_t n = strlen(suffix);
    return length >= n && strncasecmp(path + length - n, suffix, n) == 0;
}

bool isHidden(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return true;
    }
    const char* slash = strrchr(path, '/');
    const char* name = slash == nullptr ? path : slash + 1;
    return name[0] == '\0' || name[0] == '.';
}

String nestedPath(const char* parent, const char* name) {
    char combined[512];
    if (parent == nullptr || parent[0] == '\0' || (parent[0] == '/' && parent[1] == '\0')) {
        snprintf(combined, sizeof(combined), "/%s", name == nullptr ? "" : name);
    } else {
        snprintf(combined, sizeof(combined), "%s/%s", parent, name == nullptr ? "" : name);
    }
    return String(combined);
}

}  // namespace

bool endsWithEpub(const char* path) {
    if (path == nullptr) {
        return false;
    }
    size_t length = strlen(path);
    while (length > 0 && (path[length - 1] == ' ' || path[length - 1] == '\r' || path[length - 1] == '\n' ||
                           path[length - 1] == '\t')) {
        --length;
    }
    // A card written without long names stores book1.epub as BOOK1.EPU.
    return hasSuffix(path, length, ".epub") || hasSuffix(path, length, ".epu");
}

void LibraryPage::list() {
    books.clear();
    std::vector<CardEntry> authors;
    if (!readCardDirectory("/", authors)) {
        Serial.println("SD root did not open");
        std::sort(books.begin(), books.end());
        return;
    }

    for (const CardEntry& author : authors) {
        if (!author.directory || isHidden(author.name.c_str())) {
            continue;
        }
        const String authorPath = nestedPath("/", author.name.c_str());
        std::vector<CardEntry> titles;
        if (!readCardDirectory(authorPath.c_str(), titles)) {
            continue;
        }

        Serial.print("Author ");
        Serial.println(authorPath);
        for (const CardEntry& title : titles) {
            if (isHidden(title.name.c_str())) {
                continue;
            }
            const String titlePath = nestedPath(authorPath.c_str(), title.name.c_str());
            if (!title.directory) {
                if (endsWithEpub(title.name.c_str())) {
                    books.push_back(titlePath);
                    Serial.print("Book ");
                    Serial.println(titlePath);
                } else {
                    Serial.print(" skip ");
                    Serial.println(title.name);
                }
                continue;
            }

            // Calibre-style cards store each book as Author/Title/Title.epub.
            std::vector<CardEntry> nested;
            if (!readCardDirectory(titlePath.c_str(), nested)) {
                continue;
            }
            for (const CardEntry& file : nested) {
                if (file.directory || isHidden(file.name.c_str()) || !endsWithEpub(file.name.c_str())) {
                    if (!file.directory && !isHidden(file.name.c_str())) {
                        Serial.print(" skip ");
                        Serial.println(file.name);
                    }
                    continue;
                }
                const String full = nestedPath(titlePath.c_str(), file.name.c_str());
                books.push_back(full);
                Serial.print("Book ");
                Serial.println(full);
            }
        }
    }
    std::sort(books.begin(), books.end());
}

void LibraryPage::select(const String& path) {
    pick = 0;
    for (size_t i = 0; i < books.size(); ++i) {
        if (books[i] == path) {
            pick = i;
            return;
        }
    }
}

bool LibraryPage::has(const String& path) const {
    return path.length() > 0 && pick < books.size() && books[pick] == path;
}

void LibraryPage::show(bool fullRefresh) {
    paint(fullRefresh);
}

void LibraryPage::enter(ReadingPage& reading) {
    remember(reading.path, reading.chapter, reading.page);
    reading.text = "";
    reading.pageStarts.clear();
    list();
    select(reading.path);
    show(true);
}

bool LibraryPage::openSelected(ReadingPage& reading) {
    if (books.empty()) {
        return false;
    }
    reading.path = books[pick];
    size_t chapter = 0;
    size_t page = 0;
    savedPlace(reading.path, chapter, page);
    if (!reading.open(chapter, page, false) && !reading.open(0, 0, false)) {
        show(true);
        return false;
    }
    return true;
}

LibraryAction LibraryPage::onGesture(const Input::Gesture& gesture, ReadingPage& reading) {
    if (books.size() < 2 && gesture.next != Input::Press::Long) {
        return LibraryAction::None;
    }
    if (gesture.prev == Input::Press::Short) {
        pick = (pick + books.size() - 1) % books.size();
        show();
    } else if (gesture.next == Input::Press::Short) {
        pick = (pick + 1) % books.size();
        show();
    } else if (gesture.next == Input::Press::Long) {
        return openSelected(reading) ? LibraryAction::Opened : LibraryAction::None;
    }
    return LibraryAction::None;
}
