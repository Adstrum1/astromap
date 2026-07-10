#include <Arduino.h>
#include "config.h"
#include "keypad.h"

static bool keyRaw[16];
static bool keyStable[16];
static bool prevStable[16];
static unsigned long keyChangeMs[16];

void keypadInit() {
  for (int i = 0; i < 4; i++) pinMode(COL_PINS[i], INPUT_PULLUP);
  for (int i = 0; i < 4; i++) { pinMode(ROW_PINS[i], OUTPUT); digitalWrite(ROW_PINS[i], HIGH); }
  for (int i = 0; i < 16; i++) {
    keyRaw[i] = false;
    keyStable[i] = false;
    prevStable[i] = false;
    keyChangeMs[i] = 0;
  }
}

void scanKeypad() {
  for (int r = 0; r < 4; r++) {

    for (int rr = 0; rr < 4; rr++) {
      pinMode(ROW_PINS[rr], OUTPUT);
      digitalWrite(ROW_PINS[rr], rr == r ? LOW : HIGH);
    }
    delayMicroseconds(20);

    for (int c = 0; c < 4; c++) {
      bool pressedRaw = (digitalRead(COL_PINS[c]) == LOW);
      int idx = r * 4 + c;
      if (pressedRaw != keyRaw[idx]) {
        keyChangeMs[idx] = millis();
        keyRaw[idx] = pressedRaw;
      }
      if ((millis() - keyChangeMs[idx]) > KEY_DEBOUNCE_MS) {
        keyStable[idx] = pressedRaw;
      }
    }
  }

  for (int rr = 0; rr < 4; rr++) digitalWrite(ROW_PINS[rr], HIGH);
}

bool keyPressedEdge(int idx) {
  bool nowP = keyStable[idx];
  bool edge = nowP && !prevStable[idx];
  return edge;
}

bool keyHeld(int idx) {
  return keyStable[idx];
}

void latchKeys() {
  for (int i = 0; i < 16; i++) prevStable[i] = keyStable[i];
}
