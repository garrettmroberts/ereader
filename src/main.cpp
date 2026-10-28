#include <Arduino.h>
#include <LittleFS.h>

#include "card.h"
#include "display.h"
#include "epub.h"
#include "input.h"

#include <algorithm>
#include <cstring>
#include <stdio.h>
#include <strings.h>
#include <vector>

String bookPath;
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

bool hasSuffix(const char* path, size_t length, const char* suffix) {
  const size_t n = strlen(suffix);
  return length >= n && strcasecmp(path + length - n, suffix) == 0;
}

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

void listCardBooks() {
  std::vector<CardEntry> authors;
  if (!readCardDirectory("/", authors)) {
    Serial.println("SD root did not open");
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
}

void listBooks() {
  books.clear();
  listCardBooks();
  std::sort(books.begin(), books.end());
}

void selectCurrentBook() {
  bookPick = 0;
  for (size_t i = 0; i < books.size(); ++i) {
    if (books[i] == bookPath) {
      bookPick = i;
      return;
    }
  }
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
  listBooks();
  selectCurrentBook();
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
  mountCard();

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }

  loadPlaces();
  listBooks();
  selectCurrentBook();
  const bool found = bookPath.length() > 0 && !books.empty() && books[bookPick] == bookPath;
  if (found) {
    size_t chapter = 0;
    size_t page = 0;
    savedPlace(bookPath, chapter, page);
    if (openChapter(chapter, page, false)) {
      return;
    }
  } else {
    bookPick = 0;
  }
  inLibrary = true;
  showLibrary(true);
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
