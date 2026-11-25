#define LGFX_USE_V1

#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

#include <ArduinoJson.h> // Install "ArduinoJson" from Library Manager

// ==========================================
// CROWPANEL 7.0" (800x480) RGB PANEL CONFIG
// ==========================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_RGB _panel_instance;
  lgfx::Bus_RGB   _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;
      cfg.pin_d0  = GPIO_NUM_15; // B0
      cfg.pin_d1  = GPIO_NUM_7;  // B1
      cfg.pin_d2  = GPIO_NUM_6;  // B2
      cfg.pin_d3  = GPIO_NUM_5;  // B3
      cfg.pin_d4  = GPIO_NUM_4;  // B4
      cfg.pin_d5  = GPIO_NUM_9;  // G0
      cfg.pin_d6  = GPIO_NUM_46; // G1
      cfg.pin_d7  = GPIO_NUM_3;  // G2
      cfg.pin_d8  = GPIO_NUM_8;  // G3
      cfg.pin_d9  = GPIO_NUM_16; // G4
      cfg.pin_d10 = GPIO_NUM_1;  // G5
      cfg.pin_d11 = GPIO_NUM_14; // R0
      cfg.pin_d12 = GPIO_NUM_21; // R1
      cfg.pin_d13 = GPIO_NUM_47; // R2
      cfg.pin_d14 = GPIO_NUM_48; // R3
      cfg.pin_d15 = GPIO_NUM_45; // R4

      cfg.pin_henable = GPIO_NUM_41;
      cfg.pin_vsync   = GPIO_NUM_40;
      cfg.pin_hsync   = GPIO_NUM_39;
      cfg.pin_pclk    = GPIO_NUM_0;
      cfg.freq_write  = 15000000;

      cfg.hsync_polarity    = 0;
      cfg.hsync_front_porch = 40;
      cfg.hsync_pulse_width = 48;
      cfg.hsync_back_porch  = 40;
      cfg.vsync_polarity    = 0;
      cfg.vsync_front_porch = 1;
      cfg.vsync_pulse_width = 31;
      cfg.vsync_back_porch  = 13;
      cfg.pclk_active_neg   = 1;
      cfg.de_idle_high      = 0;
      cfg.pclk_idle_high    = 0;
      _bus_instance.config(cfg);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width  = 800;
      cfg.memory_height = 480;
      cfg.panel_width   = 800;
      cfg.panel_height  = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = GPIO_NUM_2; // Backlight pin on CrowPanel 7.0"
      _light_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);
    _panel_instance.setLight(&_light_instance);
    setPanel(&_panel_instance);
  }
};

LGFX lcd;
LGFX_Sprite canvas(&lcd); // Off-screen PSRAM sprite for zero-flicker rendering

// Scoreboard State Variables
String curState  = "STATE_IDLE";
String curHeader = "INSERT PENNY";
String curMsg    = "Waiting for Arduino UNO R4...";
uint32_t curScore = 0;
uint32_t curHigh  = 1000;
uint8_t  curMult  = 1;
uint16_t curHits  = 0;
uint32_t curTime  = 0;
bool     curHealth = true;

void drawScoreboard() {
  canvas.fillScreen(TFT_BLACK);

  // Outer Neon Arcade Frame
  uint16_t accentColor = TFT_CYAN;
  if (curState == "STATE_IDLE")            accentColor = TFT_MAGENTA;
  if (curState == "STATE_READY")           accentColor = TFT_YELLOW;
  if (curState == "STATE_BALL_IN_PLUNGER") accentColor = TFT_GREEN;
  if (curState == "STATE_BALL_IN_PLAY")    accentColor = TFT_CYAN;
  if (curState == "STATE_GAMEOVER")        accentColor = TFT_RED;

  canvas.drawRoundRect(10, 10, 780, 460, 16, accentColor);
  canvas.drawRoundRect(14, 14, 772, 452, 14, accentColor);

  // Top Status Bar
  canvas.setTextDatum(top_left);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.setFont(&fonts::Font2);
  canvas.drawString("MODE: " + curState, 30, 24);

  canvas.setTextDatum(top_right);
  canvas.setTextColor(curHealth ? TFT_GREEN : TFT_RED);
  canvas.drawString(curHealth ? "SYS HEALTH: OK" : "SYS HEALTH: FAULT", 770, 24);

  // Main Banner Header (e.g., INSERT PENNY, PULL PLUNGER, GAME OVER)
  canvas.setTextDatum(top_center);
  canvas.setTextColor(accentColor);
  canvas.setFont(&fonts::Font4);
  canvas.drawString(curHeader, 400, 62);

  // Sub-message
  canvas.setTextColor(TFT_WHITE);
  canvas.setFont(&fonts::Font2);
  canvas.drawString(curMsg, 400, 105);

  // Giant Center Score Readout
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_YELLOW);
  canvas.setFont(&fonts::Font8); // Large 75px numeric font
  canvas.drawNumber(curScore, 400, 225);

  // Bottom Stats Boxes (HIGH SCORE | MULTIPLIER | HITS | TIMER)
  canvas.drawFastHLine(25, 330, 750, TFT_DARKGREY);

  canvas.setTextDatum(top_center);
  canvas.setFont(&fonts::Font4);

  // High Score
  canvas.setTextColor(TFT_ORANGE);
  canvas.drawString("HIGH SCORE", 140, 350);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawNumber(curHigh, 140, 395);

  // Multiplier
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("MULT", 340, 350);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(String(curMult) + "x", 340, 395);

  // Targets Hit
  canvas.setTextColor(TFT_GREENYELLOW);
  canvas.drawString("HITS", 500, 350);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawNumber(curHits, 500, 395);

  // Countdown Timer
  canvas.setTextColor(TFT_MAGENTA);
  canvas.drawString("TIME", 670, 350);
  canvas.setTextColor(curTime <= 10 && curState == "STATE_BALL_IN_PLAY" ? TFT_RED : TFT_WHITE);
  canvas.drawString(String(curTime) + "s", 670, 395);

  // Push sprite to screen in one instant frame
  canvas.pushSprite(0, 0);
}

void setup() {
  // UART0 connected to UNO R4 WiFi (RX=44, TX=43)
  Serial.begin(115200);

  lcd.init();
  lcd.setBrightness(220);
  canvas.setColorDepth(16);
  canvas.createSprite(800, 480);

  drawScoreboard();
}

void loop() {
  if (Serial.available()) {
    String jsonLine = Serial.readStringUntil('\n');
    jsonLine.trim();
    if (jsonLine.startsWith("{")) {
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, jsonLine);
      if (!err) {
        curState  = doc["state"]  | curState;
        curHeader = doc["header"] | curHeader;
        curMsg    = doc["msg"]    | curMsg;
        curScore  = doc["score"]  | curScore;
        curHigh   = doc["high"]   | curHigh;
        curMult   = doc["mult"]   | curMult;
        curHits   = doc["hits"]   | curHits;
        curTime   = doc["time"]   | curTime;
        curHealth = (doc["health"] | 1) == 1;
        drawScoreboard();
      }
    }
  }
}