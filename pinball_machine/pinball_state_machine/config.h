#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// ARDUINO UNO R4 WIFI PIN DEFINITIONS
// (D0/D1 reserved for ESP32-S3; A4/A5 reserved for I2C/Qwiic)
// ==========================================
// --- Power Enable Rails (MOSFET / Relay control pins for low-power gating) ---
const uint8_t PIN_PWR_COIN_SENSOR      = 2;
const uint8_t PIN_PWR_PLUNGER_SENSOR   = 3;
const uint8_t PIN_PWR_PLAYLANE_SENSOR  = 4;
const uint8_t PIN_PWR_FLIPPERS         = 5;
const uint8_t PIN_PWR_DRAIN_SENSOR     = 6;
const uint8_t PIN_PWR_TARGET_SENSORS   = 7;

// --- Sensor Inputs (Active LOW with INPUT_PULLUP) ---
const uint8_t PIN_SENS_COIN_IR         = 8;
const uint8_t PIN_SENS_PLUNGER_LANE    = 9;
const uint8_t PIN_SENS_PLAY_LANE       = 10;
const uint8_t PIN_SENS_DRAIN           = 11;
const uint8_t PIN_SENS_TARGET_1        = 12;
const uint8_t PIN_SENS_TARGET_2        = 13;

// --- Actuator Outputs ---
const uint8_t PIN_ACT_COIN_RETURN      = A0; // Solenoid/Servo to refund penny
const uint8_t PIN_ACT_AUDIO_TRIG       = A1; // Sound module trigger
const uint8_t PIN_WS2812B_DATA     = A2; // Cabinet LED strip / relay WS2812B Data Pin (via 330-ohm resistor)

// ==========================================
// WS2812B LED STRIP CONFIGURATION
// ==========================================
const uint16_t NUM_LEDS                = 30;  // Change to match your total LED count
const uint8_t  LED_BRIGHTNESS          = 120; // 0-255 (120 keeps current draw safe & bright)

// ==========================================
// WI-FI ACCESS POINT SETTINGS (ESP32-S3 Display)
// ==========================================
const char AP_SSID[] = "PA1";
const char AP_PASS[] = "pinball123"; // Min 8 chars for WPA2

// ==========================================
// TIMING CONSTANTS (milliseconds)
// ==========================================
const unsigned long TIME_HEALTH_CHECK_INTERVAL = 5000;  // 5s self-check in IDLE
const unsigned long TIME_READY_TIMEOUT         = 60000; // 60s timeout if no ball loaded
const unsigned long TIME_READY_EXIT_DELAY      = 10000; // 10s wait before BALL_IN_PLUNGER
const unsigned long TIME_PLUNGER_TIMEOUT       = 60000; // 60s timeout if not launched
const unsigned long TIME_GAMEPLAY_MAX          = 60000; // 60s max gameplay round
const unsigned long TIME_DISPLAY_REFRESH       = 1000;  // 1s display refresh in play
const unsigned long TIME_GAMEOVER_DURATION     = 10000; // 10s game over screen

// ==========================================
// STATE MACHINE & DATA STRUCTURES
// ==========================================
enum GameState {
  STATE_IDLE = 0,
  STATE_READY,
  STATE_BALL_IN_PLUNGER,
  STATE_BALL_IN_PLAY,
  STATE_GAMEOVER
};

struct StateTelemetry {
  unsigned long stateEntryTime;
  unsigned long stateExitTime;
  unsigned long lastHealthCheckTime;
  bool          circuitsHealthy;
  unsigned long pennyDropTime;
  unsigned long plungerBallDetectTime;
};

struct GameStats {
  uint32_t currentScore;
  uint32_t highScore;
  uint16_t targetsHit;
  uint8_t  multiplier;
};

struct PersistentMemory {
  uint16_t magicHeader;
  bool     pennyEscrowed;
  uint32_t savedHighScore;
};

const uint16_t EEPROM_MAGIC = 0xBEEF;
const int      EEPROM_ADDR  = 0;

#endif
