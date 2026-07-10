#pragma once

void keypadInit();

void scanKeypad();

void latchKeys();

bool keyPressedEdge(int idx);

bool keyHeld(int idx);
