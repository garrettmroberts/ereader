#include "display.h"
#include "input.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

String bookText;
std::vector<size_t> pageStarts;
size_t pageIndex = 0;

String readTextFile(const char* path) {
  Serial.println(path);
  File file = LittleFS.open(path, "r");

  if (!file) {
    Serial.println("Failed to open file.");
    return "";
  }

  String contents = file.readString();

  file.close();

  return contents;
}

void showPage() {
  if (pageStarts.empty()) {
    return;
  }

  Serial.print("Page ");
  Serial.print(pageIndex + 1);
  Serial.print("/");
  Serial.print(pageStarts.size());
  Serial.print(" @ ");
  Serial.println(pageStarts[pageIndex]);
  drawPage(bookText, pageStarts[pageIndex]);
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

  bookText = readTextFile("/snippet.txt");
  pageStarts = paginate(bookText);
  pageIndex = 0;

  if (pageStarts.empty()) {
    Serial.println("Pages: 0");
    drawPage(bookText, 0);
    return;
  }

  showPage();
}

void loop() {
  if (Input::button1Pressed() && pageIndex > 0) {
    pageIndex--;
    showPage();
  }

  if (Input::button2Pressed() && pageIndex + 1 < pageStarts.size()) {
    pageIndex++;
    showPage();
  }
}
