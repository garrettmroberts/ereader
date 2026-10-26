#include "display.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>
#include <tuple>

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

std::tuple<String, int> getParagraph(const String& text) {
    int index = text.indexOf('\n');

    if (index == -1) {
        return std::make_tuple(text, text.length());
    }

    String paragraph = text.substring(0, index);
    paragraph.trim();

    return std::make_tuple(paragraph, index);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  initDisplay();

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }
  
  String text = readTextFile("/snippet.txt");

  int newLineIdx = 0;
  std::vector<String> paragraphs;

  while (newLineIdx < text.length()) {
    auto p = getParagraph(text.substring(newLineIdx));
    paragraphs.push_back(std::get<0>(p));
    newLineIdx += std::get<1>(p) + 1;
  }

  displayParagraphs(paragraphs);
}

void loop() {
}