#include "display.h"
#include "epub.h"
#include "input.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

const char* bookPath = "/walden.epub";
String bookText;
std::vector<size_t> pageStarts;
size_t chapterIndex = 0;
size_t pageIndex = 0;

void showPage() {
  if (pageStarts.empty()) {
    return;
  }

  Serial.print("Chapter ");
  Serial.print(chapterIndex + 1);
  Serial.print(" page ");
  Serial.print(pageIndex + 1);
  Serial.print("/");
  Serial.print(pageStarts.size());
  Serial.print(" @ ");
  Serial.println(pageStarts[pageIndex]);
  drawPage(bookText, pageStarts[pageIndex]);
}

bool openChapter(size_t index, bool lastPage) {
  if (index >= epubChapterCount(bookPath)) {
    return false;
  }

  bookText = "";
  pageStarts.clear();
  if (!loadEpubChapter(bookPath, index, bookText)) {
    Serial.println("Failed to load chapter");
    drawPage(bookText, 0);
    return false;
  }

  chapterIndex = index;
  pageStarts = paginate(bookText);
  if (pageStarts.empty()) {
    pageIndex = 0;
    Serial.println("Pages: 0");
    drawPage(bookText, 0);
    return true;
  }

  pageIndex = lastPage ? pageStarts.size() - 1 : 0;
  showPage();
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Input::init();
  initDisplay();

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }

  if (!openChapter(4, false)) {
    Serial.println("Failed to open epub");
  }
}

void loop() {
  const Input::Press prev = Input::button1();
  if (prev == Input::Press::Long) {
    if (chapterIndex > 0) {
      openChapter(chapterIndex - 1, false);
    }
  } else if (prev == Input::Press::Short) {
    if (pageIndex > 0) {
      pageIndex--;
      showPage();
    } else if (chapterIndex > 0) {
      openChapter(chapterIndex - 1, true);
    }
  }

  const Input::Press next = Input::button2();
  if (next == Input::Press::Long) {
    openChapter(chapterIndex + 1, false);
  } else if (next == Input::Press::Short) {
    if (!pageStarts.empty() && pageIndex + 1 < pageStarts.size()) {
      pageIndex++;
      showPage();
    } else {
      openChapter(chapterIndex + 1, false);
    }
  }
}
