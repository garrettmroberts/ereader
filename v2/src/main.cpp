#include <Arduino.h>

#include "input.h"
#include "display.h"
#include "card.h"

// put function declarations here:
int myFunction(int, int);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Input::init();
  Display::init();
  Card::init();
}

void loop() {
  static Input::Gesture previous;
  const Input::Gesture gesture = Input::read();
  if (gesture != previous) {
    Input::print(gesture);
    previous = gesture;
  }
}