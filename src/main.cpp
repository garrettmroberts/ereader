#include "display.h"
#include "epub.h"
#include "input.h"
#include <Arduino.h>
#include <LittleFS.h>

#include <algorithm>
#include <cstring>
#include <strings.h>
#include <vector>

String bookPath = "/walden.epub";
String bookText;
std::vector<size_t> pageStarts;
size_t chapterIndex = 0;
size_t pageIndex = 0;

bool inLibrary = false;
std::vector<String> books;
size_t bookPick = 0;

struct Place {
  String path;
  size_t chapter = 0;
  size_t page = 0;
};

std::vector<Place> places;

bool endsWithEpub(const char* path) {
  const size_t length = strlen(path);
  return length >= 5 && strcasecmp(path + length - 5, ".epub") == 0;
}

void loadPlaces() {
  places.clear();
  File file = LittleFS.open("/positions.txt", "r");
  if (!file) {
    return;
  }
  while (file.available()) {
    const String line = file.readStringUntil('\n');
    const int first = line.indexOf('\t');
    const int second = first < 0 ? -1 : line.indexOf('\t', first + 1);
    if (first <= 0 || second < 0) {
      continue;
    }
    Place place;
    place.path = line.substring(0, first);
    place.chapter = static_cast<size_t>(line.substring(first + 1, second).toInt());
    place.page = static_cast<size_t>(line.substring(second + 1).toInt());
    places.push_back(place);
  }
  file.close();
}

void savePlaces() {
  File file = LittleFS.open("/positions.txt", "w");
  if (!file) {
    Serial.println("Failed to save position");
    return;
  }
  for (const Place& place : places) {
    file.print(place.path);
    file.print('\t');
    file.print(place.chapter);
    file.print('\t');
    file.println(place.page);
  }
  file.close();
}

void remember() {
  for (Place& place : places) {
    if (place.path == bookPath) {
      place.chapter = chapterIndex;
      place.page = pageIndex;
      savePlaces();
      return;
    }
  }
  places.push_back(Place{bookPath, chapterIndex, pageIndex});
  savePlaces();
}

void savedPlace(const String& path, size_t& chapter, size_t& page) {
  chapter = 0;
  page = 0;
  for (const Place& place : places) {
    if (place.path == path) {
      chapter = place.chapter;
      page = place.page;
      return;
    }
  }
}

void showPage(bool fullRefresh = false) {
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
  remember();
  drawPage(bookText, pageStarts[pageIndex], fullRefresh);
}

bool openChapter(size_t index, size_t page, bool lastPage) {
  if (index >= epubChapterCount(bookPath.c_str())) {
    return false;
  }

  bookText = "";
  pageStarts.clear();
  if (!loadEpubChapter(bookPath.c_str(), index, bookText)) {
    Serial.println("Failed to load chapter");
    drawPage(bookText, 0, true);
    return false;
  }

  chapterIndex = index;
  pageStarts = paginate(bookText);
  if (pageStarts.empty()) {
    pageIndex = 0;
    Serial.println("Pages: 0");
    drawPage(bookText, 0, true);
    remember();
    return true;
  }

  if (lastPage || page >= pageStarts.size()) {
    pageIndex = pageStarts.size() - 1;
  } else {
    pageIndex = page;
  }
  showPage(true);
  return true;
}

void showLibrary(bool fullRefresh = false) {
  if (books.empty()) {
    drawLibrary(nullptr, 0, 0, fullRefresh);
    return;
  }
  drawLibrary(books[bookPick].c_str(), bookPick, books.size(), fullRefresh);
}

void enterLibrary() {
  remember();
  bookText = "";
  pageStarts.clear();
  books.clear();

  File root = LittleFS.open("/");
  if (root) {
    File entry = root.openNextFile();
    while (entry) {
      const char* path = entry.path();
      if (path != nullptr && !entry.isDirectory() && endsWithEpub(path)) {
        String full = path;
        if (!full.startsWith("/")) {
          full = "/" + full;
        }
        books.push_back(full);
      }
      entry = root.openNextFile();
    }
    root.close();
  }

  std::sort(books.begin(), books.end());
  bookPick = 0;
  for (size_t i = 0; i < books.size(); ++i) {
    if (books[i] == bookPath) {
      bookPick = i;
      break;
    }
  }

  inLibrary = true;
  showLibrary(true);
}

void openPickedBook() {
  if (books.empty()) {
    return;
  }
  bookPath = books[bookPick];
  size_t chapter = 0;
  size_t page = 0;
  savedPlace(bookPath, chapter, page);
  inLibrary = false;
  if (!openChapter(chapter, page, false) && !openChapter(0, 0, false)) {
    inLibrary = true;
    showLibrary(true);
  }
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

  loadPlaces();
  size_t chapter = 0;
  size_t page = 0;
  savedPlace(bookPath, chapter, page);
  if (!openChapter(chapter, page, false)) {
    Serial.println("Failed to open epub");
  }
}

void loop() {
  const Input::Gesture gesture = Input::read();
  if (gesture.bothLong) {
    if (!inLibrary) {
      enterLibrary();
    }
    return;
  }

  if (inLibrary) {
    if (books.size() < 2 && gesture.next != Input::Press::Long) {
      return;
    }
    if (gesture.prev == Input::Press::Short) {
      bookPick = (bookPick + books.size() - 1) % books.size();
      showLibrary();
    } else if (gesture.next == Input::Press::Short) {
      bookPick = (bookPick + 1) % books.size();
      showLibrary();
    } else if (gesture.next == Input::Press::Long) {
      openPickedBook();
    }
    return;
  }

  if (gesture.prev == Input::Press::Long) {
    if (chapterIndex > 0) {
      openChapter(chapterIndex - 1, 0, false);
    }
  } else if (gesture.prev == Input::Press::Short) {
    if (pageIndex > 0) {
      pageIndex--;
      showPage();
    } else if (chapterIndex > 0) {
      openChapter(chapterIndex - 1, 0, true);
    }
  }

  if (gesture.next == Input::Press::Long) {
    openChapter(chapterIndex + 1, 0, false);
  } else if (gesture.next == Input::Press::Short) {
    if (!pageStarts.empty() && pageIndex + 1 < pageStarts.size()) {
      pageIndex++;
      showPage();
    } else {
      openChapter(chapterIndex + 1, 0, false);
    }
  }
}
