#include "reading.h"

#include "display.h"
#include "epub.h"
#include "places.h"

void ReadingPage::show(bool fullRefresh) {
    if (pageStarts.empty()) {
        return;
    }

    Serial.print("Chapter ");
    Serial.print(chapter + 1);
    Serial.print(" page ");
    Serial.print(page + 1);
    Serial.print("/");
    Serial.print(pageStarts.size());
    Serial.print(" @ ");
    Serial.println(pageStarts[page]);
    remember(path, chapter, page);
    paint(pageStarts[page], fullRefresh);
}

bool ReadingPage::open(size_t index, size_t at, bool lastPage) {
    if (index >= epubChapterCount(path.c_str())) {
        return false;
    }

    text = "";
    pageStarts.clear();
    if (!loadEpubChapter(path.c_str(), index, text)) {
        Serial.println("Failed to load chapter");
        paint(0, true);
        return false;
    }

    chapter = index;
    pageStarts = paginate(text);
    if (pageStarts.empty()) {
        page = 0;
        Serial.println("Pages: 0");
        paint(0, true);
        remember(path, chapter, page);
        return true;
    }

    if (lastPage || at >= pageStarts.size()) {
        page = pageStarts.size() - 1;
    } else {
        page = at;
    }
    show(true);
    return true;
}

void ReadingPage::onGesture(const Input::Gesture& gesture) {
    if (gesture.prev == Input::Press::Long) {
        if (chapter > 0) {
            open(chapter - 1, 0, false);
        }
    } else if (gesture.prev == Input::Press::Short) {
        if (page > 0) {
            page--;
            show();
        } else if (chapter > 0) {
            open(chapter - 1, 0, true);
        }
    }

    if (gesture.next == Input::Press::Long) {
        open(chapter + 1, 0, false);
    } else if (gesture.next == Input::Press::Short) {
        if (!pageStarts.empty() && page + 1 < pageStarts.size()) {
            page++;
            show();
        } else {
            open(chapter + 1, 0, false);
        }
    }
}
