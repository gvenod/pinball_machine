#ifndef HARDWARE_HAL_H
#define HARDWARE_HAL_H

#include <FastLED.h>
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "Config.h"

ArduinoLEDMatrix matrix;


//Each state gets a distinct, non-blocking RGB lighting sequence (updateLEDs) plus instant visual flash effects (flashTargetHitLEDs and celebrateHighScoreLEDs):
//• STATE_IDLE: Smooth rainbow attract wave.
//• STATE_READY: Pulsing amber/yellow beacon ("Load ball").
//• STATE_BALL_IN_PLUNGER: Running green chase toward the playfield ("Pull plunger").
//• STATE_BALL_IN_PLAY: Dynamic cyan/blue energy pulse that flashes bright gold/white on every target hit.
//• STATE_GAMEOVER: Crimson fade for standard game over, or fast multicolor strobe if a new high score was set.
CRGB leds[NUM_LEDS];

// Shared Display Strings served to both 12x8 LED Matrix & ESP32 Web Display
String dispHeader  = "BOOTING";
String dispMessage = "Please wait...";
uint32_t dispTimeRemaining = 0;

// Non-blocking LED animation state trackers
unsigned long lastLedFrameTime = 0;
unsigned long targetFlashUntil = 0;
uint8_t       ledAnimStep      = 0;

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
  digitalWrite(PIN_ACT_COIN_RETURN, LOW);

  // Initialize WS2812B LED Strip
  FastLED.addLeds<WS2812B, PIN_WS2812B_DATA, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Blue);
  FastLED.show();


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

// Call when a target is hit during STATE_BALL_IN_PLAY
void triggerTargetHitLEDs() {
  targetFlashUntil = millis() + 180; // 180ms bright gold/white flash
}

// Non-blocking WS2812B Animation Engine
// patternId: 1=IDLE, 2=READY, 3=PLUNGER, 4=IN_PLAY, 5=GAMEOVER_NORMAL, 6=GAMEOVER_HIGHSCORE
void updateLEDs(uint8_t patternId) {
  unsigned long now = millis();
  if (now - lastLedFrameTime < 30) return; // ~33 FPS smooth animation
  lastLedFrameTime = now;
  ledAnimStep++;

  // Instant target-hit flash override during gameplay
  if (patternId == 4 && now < targetFlashUntil) {
    fill_solid(leds, NUM_LEDS, CRGB::Gold);
    FastLED.show();
    return;
  }

  switch (patternId) {
    case 1: // STATE_IDLE: Rainbow attract wave
      fill_rainbow(leds, NUM_LEDS, ledAnimStep * 2, 7);
      break;

    case 2: { // STATE_READY: Pulsing Amber ("Load Ball")
      uint8_t breath = beatsin8(30, 40, 255);
      fill_solid(leds, NUM_LEDS, CHSV(32, 255, breath));
      break;
    }

    case 3: { // STATE_BALL_IN_PLUNGER: Green launch chase
      fadeToBlackBy(leds, NUM_LEDS, 60);
      uint16_t pos = ledAnimStep % NUM_LEDS;
      leds[pos] = CRGB::LimeGreen;
      break;
    }

    case 4: { // STATE_BALL_IN_PLAY: Energetic Cyan/Blue wave
      uint8_t pulse = beatsin8(60, 80, 255);
      fill_solid(leds, NUM_LEDS, CHSV(140, 220, pulse));
      leds[ledAnimStep % NUM_LEDS] = CRGB::White;
      break;
    }

    case 5: { // STATE_GAMEOVER (Standard): Red pulse
      uint8_t dim = beatsin8(20, 30, 200);
      fill_solid(leds, NUM_LEDS, CRGB(dim, 0, 0));
      break;
    }

    case 6: { // STATE_GAMEOVER (New High Score): Fast party confetti strobe
      fadeToBlackBy(leds, NUM_LEDS, 40);
      leds[random16(NUM_LEDS)] += CHSV(random8(), 200, 255);
      break;
    }
  }

  FastLED.show();
}

// ==========================================
// NON-BLOCKING PIEZO SOUNDER ENGINE (Pin A1)
// ==========================================
struct Note {
  uint16_t freq;     // Frequency in Hz (0 = rest/silence)
  uint16_t duration; // Duration in ms
};

// Sound Effect Melodies
const Note SFX_IDLE_CHIME[]     = {{523, 90}, {659, 90}, {784, 140}};                         // Track 1: Attract chime
const Note SFX_COIN_ACCEPTED[]  = {{988, 80}, {1319, 220}};                                   // Track 2: Classic coin ding
const Note SFX_BALL_READY[]     = {{440, 100}, {554, 100}, {659, 150}};                       // Track 3: Ready arpeggio
const Note SFX_COUNTDOWN_GO[]   = {{440, 120}, {0, 80}, {440, 120}, {0, 80}, {880, 260}};     // Track 4: 1-2-3-GO!
const Note SFX_GAME_OVER[]      = {{392, 160}, {349, 160}, {330, 160}, {262, 350}};           // Track 5: Sad descend
const Note SFX_TARGET_HIT[]     = {{784, 50}, {1047, 80}};                                    // Track 6: Fast bumper ping
const Note SFX_PENNY_RETURN[]   = {{659, 100}, {440, 180}};                                   // Track 7: Refund tone
const Note SFX_HIGH_SCORE[]     = {{523, 100}, {659, 100}, {784, 100}, {1047, 150}, {784, 100}, {1047, 300}}; // Track 10/11: Fanfare

const Note* activeMelody     = nullptr;
uint8_t     activeMelodyLen  = 0;
uint8_t     currentNoteIdx   = 0;
unsigned long noteStartTime  = 0;

void playSound(uint8_t trackId) {
  Serial.print(F("[PIEZO] Playing Track #"));
  Serial.println(trackId);

  switch (trackId) {
    case 1:  activeMelody = SFX_IDLE_CHIME;    activeMelodyLen = 3; break;
    case 2:  activeMelody = SFX_COIN_ACCEPTED; activeMelodyLen = 2; break;
    case 3:  activeMelody = SFX_BALL_READY;    activeMelodyLen = 3; break;
    case 4:  activeMelody = SFX_COUNTDOWN_GO;  activeMelodyLen = 5; break;
    case 5:  activeMelody = SFX_GAME_OVER;     activeMelodyLen = 4; break;
    case 6:  activeMelody = SFX_TARGET_HIT;    activeMelodyLen = 2; break;
    case 7:  activeMelody = SFX_PENNY_RETURN;  activeMelodyLen = 2; break;
    case 10:
    case 11: activeMelody = SFX_HIGH_SCORE;    activeMelodyLen = 6; break;
    default: return;
  }

  currentNoteIdx = 0;
  noteStartTime  = millis();

  if (activeMelody[0].freq > 0) {
    tone(PIN_ACT_AUDIO_TRIG, activeMelody[0].freq);
  } else {
    noTone(PIN_ACT_AUDIO_TRIG);
  }
}

// Call this every cycle in loop() so melodies advance without blocking gameplay
void updateAudio() {
  if (activeMelody == nullptr) return;

  unsigned long now = millis();
  if (now - noteStartTime >= activeMelody[currentNoteIdx].duration) {
    currentNoteIdx++;
    if (currentNoteIdx >= activeMelodyLen) {
      noTone(PIN_ACT_AUDIO_TRIG);
      activeMelody = nullptr;
      return;
    }

    noteStartTime = now;
    if (activeMelody[currentNoteIdx].freq > 0) {
      tone(PIN_ACT_AUDIO_TRIG, activeMelody[currentNoteIdx].freq);
    } else {
      noTone(PIN_ACT_AUDIO_TRIG);
    }
  }
}

void returnPenny() {
  Serial.println(F("[ACTUATOR] Returning penny to player!"));
  playSound(7);

  fill_solid(leds, NUM_LEDS, CRGB::OrangeRed);
  FastLED.show();

  digitalWrite(PIN_ACT_COIN_RETURN, HIGH);
  delay(300);
  digitalWrite(PIN_ACT_COIN_RETURN, LOW);
}

bool isCoinDetected()      { return digitalRead(PIN_SENS_COIN_IR) == LOW; }
bool isBallInPlungerLane() { return digitalRead(PIN_SENS_PLUNGER_LANE) == LOW; }
bool isBallInPlayLane()    { return digitalRead(PIN_SENS_PLAY_LANE) == LOW; }
bool isBallInDrain()       { return digitalRead(PIN_SENS_DRAIN) == LOW; }
bool isTargetHit()         { return (digitalRead(PIN_SENS_TARGET_1) == LOW || digitalRead(PIN_SENS_TARGET_2) == LOW); }

#endif