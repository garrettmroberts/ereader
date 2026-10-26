#include <Arduino.h>
#include "display.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  initDisplay();
  displayText("When I wrote the following pages, or rather the bulk of them, I lived alone, in the woods, a mile from any neighbor, in a house which I had built myself, on the shore of Walden Pond, in Concord, Massachusetts, and earned my living by the labor of my hands only. I lived there two years and two months. At present I am a sojourner in civilized life again.");
}

void loop() {
}