#define LGFX_USE_V1

#include <Wire.h>

#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

#include <ArduinoJson.h>

// ==========================================
// DATA STRUCTURE FOR 9-SECTION SCOREBOARD
// ==========================================
struct ScoreboardData {
  // System state (for border/banner color themes)
  String   state        = "STATE_IDLE";
  bool     systemHealth = true;

  // Row 1 (3 Sections)
  String   playerName   = "PLAYER 1";
  uint8_t  multiplier   = 1;
  uint32_t bonusPoints  = 0;
  uint16_t timeLeft     = 60;

  // Row 2 (1 Section)
  uint32_t currentScore = 0;

  // Row 3 (1 Section)
  String   bannerMsg    = "INSERT 1 PENNY TO START";

  // Row 4 (4 Sections)
  uint32_t highScore    = 1000;
  uint16_t bellHits     = 0;
  uint16_t bumperHits   = 0;
  uint16_t pathHits     = 0;
};

ScoreboardData boardData;

// Helper to draw a clean bordered card with a top label
void drawCardBox(int x, int y, int w, int h, uint16_t borderColor, uint16_t bgColor, const String& label, uint16_t labelColor) {
  canvas.fillRoundRect(x, y, w, h, 8, bgColor);
  canvas.drawRoundRect(x, y, w, h, 8, borderColor);
  canvas.setTextDatum(top_center);
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(labelColor);
  canvas.drawString(label, x + (w / 2), y + 6);
}

// ==========================================
// RENDER ALL 9 SECTIONS TO 800x480 CANVAS
// ==========================================
void drawScoreboard(const ScoreboardData& d) {
  canvas.fillScreen(TFT_BLACK);

  // Dynamic theme accent color based on active game state
  uint16_t accent = TFT_CYAN;
  if (d.state == "STATE_IDLE")            accent = TFT_MAGENTA;
  else if (d.state == "STATE_READY")      accent = TFT_YELLOW;
  else if (d.state == "STATE_BALL_IN_PLUNGER") accent = TFT_GREEN;
  else if (d.state == "STATE_BALL_IN_PLAY")    accent = TFT_CYAN;
  else if (d.state == "STATE_GAMEOVER")   accent = TFT_RED;

  uint16_t cardBg = 0x10A2; // Dark slate gray card background

  // ---------------------------------------------------------
  // ROW 1 (Y = 10..95): Player Name | Bonus/Mult | Time Left
  // ---------------------------------------------------------
  // Section 1: Player Name (X: 10..270, W: 260)
  drawCardBox(10, 10, 260, 85, accent, cardBg, "PLAYER", TFT_LIGHTGREY);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::Font4);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(d.playerName, 140, 62);

  // Section 2: Bonus / Multiplier (X: 276..524, W: 248)
  drawCardBox(276, 10, 248, 85, accent, cardBg, "MULTIPLIER / BONUS", TFT_LIGHTGREY);
  canvas.setFont(&fonts::Font4);
  canvas.setTextColor(TFT_CYAN);
  String bonusStr = String(d.multiplier) + "x  (+" + String(d.bonusPoints) + ")";
  canvas.drawString(bonusStr, 400, 62);

  // Section 3: Time Left (X: 530..790, W: 260)
  uint16_t timeColor = (d.timeLeft <= 10 && d.state == "STATE_BALL_IN_PLAY") ? TFT_RED : TFT_GREENYELLOW;
  drawCardBox(530, 10, 260, 85, accent, cardBg, "TIME LEFT", TFT_LIGHTGREY);
  canvas.setFont(&fonts::Font4);
  canvas.setTextColor(timeColor);
  canvas.drawString(String(d.timeLeft) + " SEC", 660, 62);

  // ---------------------------------------------------------
  // ROW 2 (Y = 101..265): Current Score (Full Width)
  // ---------------------------------------------------------
  // Section 4: Current Score (X: 10..790, W: 780, H: 164)
  drawCardBox(10, 101, 780, 164, accent, 0x0841, "CURRENT SCORE", accent);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::Font8); // Large 75px arcade numeric font
  canvas.setTextColor(TFT_YELLOW);
  canvas.drawNumber(d.currentScore, 400, 195);

  // ---------------------------------------------------------
  // ROW 3 (Y = 271..355): Banner Message / Tips (Full Width)
  // ---------------------------------------------------------
  // Section 5: Banner Message (X: 10..790, W: 780, H: 84)
  drawCardBox(10, 271, 780, 84, TFT_ORANGE, 0x2104, "NEXT STEP / GAME TIP", TFT_ORANGE);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::Font4);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(d.bannerMsg, 400, 324);

  // ---------------------------------------------------------
  // ROW 4 (Y = 361..470): High Score | Bell | Bumper | Path
  // ---------------------------------------------------------
  // Section 6: Machine High Score (X: 10..200, W: 190)
  drawCardBox(10, 361, 190, 109, TFT_GOLD, cardBg, "HIGH SCORE", TFT_GOLD);
  canvas.setFont(&fonts::Font4);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawNumber(d.highScore, 105, 426);

  // Section 7: Bell Hits (X: 206..396, W: 190)
  drawCardBox(206, 361, 190, 109, TFT_SKYBLUE, cardBg, "BELL HITS", TFT_SKYBLUE);
  canvas.setFont(&fonts::Font4);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawNumber(d.bellHits, 301, 426);

  // Section 8: Bumper Hits (X: 402..592, W: 190)
  drawCardBox(402, 361, 190, 109, TFT_GREENYELLOW, cardBg, "BUMPER HITS", TFT_GREENYELLOW);
  canvas.setFont(&fonts::Font4);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawNumber(d.bumperHits, 497, 426);

  // Section 9: Path Hits (X: 598..790, W: 192)
  drawCardBox(598, 361, 192, 109, TFT_VIOLET, cardBg, "PATH HITS", TFT_VIOLET);
  canvas.setFont(&fonts::Font4);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawNumber(d.pathHits, 694, 426);

  // Push entire frame to IPS display at once (zero flicker)
  canvas.pushSprite(0, 0);
}

// ==========================================
// CROWPANEL ADVANCE 7.0" (800x480 IPS) CONFIG
// ==========================================
#define ADVANCE_I2C_SDA 15
#define ADVANCE_I2C_SCL 16

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_RGB _panel_instance;
  lgfx::Bus_RGB   _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;

      // Blue (B0-B4)
      cfg.pin_d0  = GPIO_NUM_21;
      cfg.pin_d1  = GPIO_NUM_47;
      cfg.pin_d2  = GPIO_NUM_48;
      cfg.pin_d3  = GPIO_NUM_45;
      cfg.pin_d4  = GPIO_NUM_38;

      // Green (G0-G5)
      cfg.pin_d5  = GPIO_NUM_9;
      cfg.pin_d6  = GPIO_NUM_10;
      cfg.pin_d7  = GPIO_NUM_11;
      cfg.pin_d8  = GPIO_NUM_12;
      cfg.pin_d9  = GPIO_NUM_13;
      cfg.pin_d10 = GPIO_NUM_14;

      // Red (R0-R4)
      cfg.pin_d11 = GPIO_NUM_7;
      cfg.pin_d12 = GPIO_NUM_17;
      cfg.pin_d13 = GPIO_NUM_18;
      cfg.pin_d14 = GPIO_NUM_3;
      cfg.pin_d15 = GPIO_NUM_46;

      // Control & Sync Pins (Advance 7.0" IPS)
      cfg.pin_henable = GPIO_NUM_42;
      cfg.pin_vsync   = GPIO_NUM_41;
      cfg.pin_hsync   = GPIO_NUM_40;
      cfg.pin_pclk    = GPIO_NUM_39;
      cfg.freq_write  = 21000000;

      cfg.hsync_polarity    = 0;
      cfg.hsync_front_porch = 8;
      cfg.hsync_pulse_width = 4;
      cfg.hsync_back_porch  = 8;
      cfg.vsync_polarity    = 0;
      cfg.vsync_front_porch = 8;
      cfg.vsync_pulse_width = 4;
      cfg.vsync_back_porch  = 8;
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
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);
  }
};

LGFX lcd;
LGFX_Sprite canvas(&lcd);

// Enable Backlight & Reset on CrowPanel Advance 7.0" (supports V1.0 & V1.2+ boards)
void initAdvanceBacklightAndReset() {
  Wire.begin(ADVANCE_I2C_SDA, ADVANCE_I2C_SCL);
  delay(50);

  // V1.2 / V1.3 / V1.4+ STC8H I2C controller at 0x30
  Wire.beginTransmission(0x30);
  Wire.write(0x10); // Set backlight brightness command
  Wire.write(0xFF); // Max brightness (0x00 - 0xFF)
  Wire.endTransmission();

  // V1.0 TCA9534 IO Expander fallback (0x20 / 0x38)
  Wire.beginTransmission(0x20);
  Wire.write(0x03); // Configuration register
  Wire.write(0x00); // All outputs
  Wire.endTransmission();
  Wire.beginTransmission(0x20);
  Wire.write(0x01); // Output port register
  Wire.write(0xFF); // Backlight ON & LCD Reset HIGH
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200); // UART0 connected to UNO R4 WiFi
  initAdvanceBacklightAndReset();

  lcd.init();
  canvas.setColorDepth(16);
  canvas.createSprite(800, 480);

   drawScoreboard();
}