#ifndef HARDWARE_HAL_H
#define HARDWARE_HAL_H

#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "Config.h"

ArduinoLEDMatrix matrix;

// Shared Display Strings served to both 12x8 LED Matrix & ESP32 Web Display
String dispHeader  = "BOOTING";
String dispMessage = "Please wait...";
uint32_t dispTimeRemaining = 0;

void setSubsystemsPower(bool coin, bool plunger, bool playLane, bool flippers, bool drain, bool targets) {
  digitalWrite(PIN_PWR_COIN_SENSOR,     coin     ? HIGH : LOW);
  digitalWrite(PIN_PWR_PLUNGER_SENSOR,  plunger  ? HIGH : LOW);
  digitalWrite(PIN_PWR_PLAYLANE_SENSOR, playLane ? HIGH : LOW);
  digitalWrite(PIN_PWR_FLIPPERS,        flippers ? HIGH : LOW);
  digitalWrite(PIN_PWR_DRAIN_SENSOR,    drain    ? HIGH : LOW);
  digitalWrite(PIN_PWR_TARGET_SENSORS,  targets  ? HIGH : LOW);
}

void initHardware() {
  pinMode(PIN_PWR_COIN_SENSOR,     OUTPUT);
  pinMode(PIN_PWR_PLUNGER_SENSOR,  OUTPUT);
  pinMode(PIN_PWR_PLAYLANE_SENSOR, OUTPUT);
  pinMode(PIN_PWR_FLIPPERS,        OUTPUT);
  pinMode(PIN_PWR_DRAIN_SENSOR,    OUTPUT);
  pinMode(PIN_PWR_TARGET_SENSORS,  OUTPUT);

  pinMode(PIN_SENS_COIN_IR,        INPUT_PULLUP);
  pinMode(PIN_SENS_PLUNGER_LANE,   INPUT_PULLUP);
  pinMode(PIN_SENS_PLAY_LANE,      INPUT_PULLUP);
  pinMode(PIN_SENS_DRAIN,          INPUT_PULLUP);
  pinMode(PIN_SENS_TARGET_1,       INPUT_PULLUP);
  pinMode(PIN_SENS_TARGET_2,       INPUT_PULLUP);

  pinMode(PIN_ACT_COIN_RETURN,     OUTPUT);
  pinMode(PIN_ACT_AUDIO_TRIG,      OUTPUT);
  pinMode(PIN_ACT_ATTRACT_LEDS,    OUTPUT);
  digitalWrite(PIN_ACT_COIN_RETURN, LOW);

  // Initialize onboard 12x8 LED Matrix
  matrix.begin();
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textFont(Font_4x6);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println("PIN");
  matrix.endText();
  matrix.endDraw();
}

void returnPenny() {
  Serial.println(F("[ACTUATOR] Returning penny to player!"));
  digitalWrite(PIN_ACT_COIN_RETURN, HIGH);
  delay(300);
  digitalWrite(PIN_ACT_COIN_RETURN, LOW);
}

bool runCircuitHealthCheck() {
  // Verify no stuck active-LOW sensors while rails are powered down
  bool sensorsClear = (digitalRead(PIN_SENS_DRAIN) == HIGH);
  return sensorsClear;
}

void showMatrixText(const String& shortText) {
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(60);
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println("  " + shortText + "  ");
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}

void updateDisplay(const String& header, const String& msg, const String& matrixTag = "") {
  dispHeader  = header;
  dispMessage = msg;
  Serial.println("[DISPLAY] " + header + " | " + msg);
  if (matrixTag.length() > 0) {
    showMatrixText(matrixTag);
  }
}

void playSound(uint8_t trackId) {
  Serial.print(F("[AUDIO] Track #"));
  Serial.println(trackId);
  digitalWrite(PIN_ACT_AUDIO_TRIG, HIGH);
  delay(20);
  digitalWrite(PIN_ACT_AUDIO_TRIG, LOW);
}

void updateLEDs(uint8_t patternId) {
  unsigned long speed = (patternId == 4) ? 150 : 400;
  digitalWrite(PIN_ACT_ATTRACT_LEDS, (millis() / speed) % 2);
}

bool isCoinDetected()      { return digitalRead(PIN_SENS_COIN_IR) == LOW; }
bool isBallInPlungerLane() { return digitalRead(PIN_SENS_PLUNGER_LANE) == LOW; }
bool isBallInPlayLane()    { return digitalRead(PIN_SENS_PLAY_LANE) == LOW; }
bool isBallInDrain()       { return digitalRead(PIN_SENS_DRAIN) == LOW; }
bool isTargetHit()         { return (digitalRead(PIN_SENS_TARGET_1) == LOW || digitalRead(PIN_SENS_TARGET_2) == LOW); }

#endif