#include <EEPROM.h>
#include <WiFiS3.h>
#include "config.h"
#include "hardware_hal.h"

//### How to Test the ESP32-S3 Display
//1. Upload the sketch to your **Arduino UNO R4 WiFi** [1.3.4].
//2. Open Serial Monitor (`115200` baud) to verify the IP address (usually `192.168.4.1`).
//3. On your phone or laptop, connect to Wi-Fi network **`PB1`** (password: **`pinball123`**) and 
//browse to **`http://192.168.4.1`** to view the live auto-refreshing backbox 
//display while the onboard 12×8 LED matrix shows quick status cues.

WiFiServer server(80);

GameState        currentState;
StateTelemetry   telemetry;
GameStats        stats;
PersistentMemory nvMem;

// Sub-timers for non-blocking delays and periodic updates
unsigned long lastPeriodicTick = 0;
unsigned long exitDelayStart   = 0;
bool          waitingExitDelay = false;

// ==========================================
// ESP32-S3 WEB DISPLAY SERVER
// ==========================================
String getStateName(GameState s) {
  switch (s) {
    case STATE_IDLE:            return "STATE_IDLE";
    case STATE_READY:           return "STATE_READY";
    case STATE_BALL_IN_PLUNGER: return "STATE_BALL_IN_PLUNGER";
    case STATE_BALL_IN_PLAY:    return "STATE_BALL_IN_PLAY";
    case STATE_GAMEOVER:        return "STATE_GAMEOVER";
    default:                    return "UNKNOWN";
  }
}

void initWiFiDisplay() {
  Serial.println(F("[ESP32-S3] Starting Wi-Fi Scoreboard AP..."));
  int status = WiFi.beginAP(AP_SSID, AP_PASS);
  if (status != WL_AP_LISTENING) {
    Serial.println(F("[ESP32-S3] AP failed to start!"));
    return;
  }
  delay(2000);
  server.begin();
  Serial.print(F("[ESP32-S3] Connect to Wi-Fi '"));
  Serial.print(AP_SSID);
  Serial.print(F("' and open http://"));
  Serial.println(WiFi.localIP());
}

void handleWebDisplayClient() {
  WiFiClient client = server.available();
  if (!client) return;

  String req = client.readStringUntil('\r');
  client.flush();

  // JSON endpoint polled by browser every 500ms for flicker-free updates
  if (req.indexOf("GET /data") >= 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close\r\n");
    client.print("{\"state\":\"" + getStateName(currentState) + "\",");
    client.print("\"header\":\"" + dispHeader + "\",");
    client.print("\"msg\":\"" + dispMessage + "\",");
    client.print("\"score\":" + String(stats.currentScore) + ",");
    client.print("\"high\":" + String(stats.highScore) + ",");
    client.print("\"mult\":" + String(stats.multiplier) + ",");
    client.print("\"hits\":" + String(stats.targetsHit) + ",");
    client.print("\"time\":" + String(dispTimeRemaining) + ",");
    client.print("\"health\":" + String(telemetry.circuitsHealthy ? "true" : "false") + "}");
  } else {
    // Full Backbox Scoreboard UI
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close\r\n");
    client.println(F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"));
    client.println(F("<style>body{background:#111;color:#0f0;font-family:monospace;text-align:center;padding:20px}"));
    client.println(F(".box{border:2px solid #0f0;border-radius:10px;padding:20px;max-width:480px;margin:auto;background:#1a1a1a}"));
    client.println(F("h1{color:#ff0;font-size:2.2em;margin:10px 0}.score{font-size:3.5em;color:#fff;margin:10px 0}"));
    client.println(F(".row{display:flex;justify-content:space-around;font-size:1.3em;margin-top:15px}</style></head><body>"));
    client.println(F("<div class='box'><div id='st'>STATE_IDLE</div><h1 id='hd'>INSERT PENNY</h1><div id='ms'>Ready</div>"));
    client.println(F("<div class='score' id='sc'>0</div><div class='row'><div>HIGH: <span id='hi'>0</span></div>"));
    client.println(F("<div>MULT: <span id='mu'>1</span>x</div><div>TIME: <span id='tm'>0</span>s</div></div>"));
    client.println(F("<div style='margin-top:15px;font-size:0.9em;color:#888'>HITS: <span id='ht'>0</span> | HEALTH: <span id='hl'>OK</span></div></div>"));
    client.println(F("<script>setInterval(()=>fetch('/data').then(r=>r.json()).then(d=>{"));
    client.println(F("document.getElementById('st').innerText=d.state;document.getElementById('hd').innerText=d.header;"));
    client.println(F("document.getElementById('ms').innerText=d.msg;document.getElementById('sc').innerText=d.score;"));
    client.println(F("document.getElementById('hi').innerText=d.high;document.getElementById('mu').innerText=d.mult;"));
    client.println(F("document.getElementById('tm').innerText=d.time;document.getElementById('ht').innerText=d.hits;"));
    client.println(F("document.getElementById('hl').innerText=d.health?'OK':'FAULT';}),500);</script></body></html>"));
  }
  client.stop();
}

// ==========================================
// EEPROM POWER-LOSS RECOVERY HELPERS
// ==========================================
void savePersistentState(bool escrowActive) {
  nvMem.magicHeader    = EEPROM_MAGIC;
  nvMem.pennyEscrowed  = escrowActive;
  nvMem.savedHighScore = stats.highScore;
  EEPROM.put(EEPROM_ADDR, nvMem);
}

void loadAndRecoverPersistentState() {
  EEPROM.get(EEPROM_ADDR, nvMem);
  if (nvMem.magicHeader != EEPROM_MAGIC) {
    // First-time boot initialization
    stats.highScore = 1000;
    savePersistentState(false);
  } else {
    stats.highScore = nvMem.savedHighScore;
    // Check if previous game crashed or lost power while holding a penny
    if (nvMem.pennyEscrowed) {
      Serial.println(F("[RECOVERY] Power loss/crash detected during active session!"));
      returnPenny();
      memset(&telemetry, 0, sizeof(telemetry)); // Clear stored telemetry
      savePersistentState(false);
    }
  }
}

// ==========================================
// STATE TRANSITION ENGINE
// ==========================================
void transitionTo(GameState nextState) {
  unsigned long now = millis();
  telemetry.stateExitTime = now;

  // --- 1. EXECUTE EXIT ACTIONS FOR CURRENT STATE ---
  switch (currentState) {
    case STATE_IDLE:
      digitalWrite(PIN_PWR_COIN_SENSOR, LOW); // Turn off Coin IR sensor
      updateDisplay("COIN ACCEPTED!", "Loading ball...", "COIN OK");
      break;

    case STATE_READY:
      digitalWrite(PIN_PWR_PLUNGER_SENSOR, LOW);
      updateDisplay("BALL READY!", "Starting in 10s...", "WAIT 10S");
      break;

    case STATE_BALL_IN_PLUNGER:
      digitalWrite(PIN_PWR_PLUNGER_SENSOR, LOW);
      digitalWrite(PIN_PWR_PLAYLANE_SENSOR, LOW);
      updateDisplay("BALL LAUNCHED!", "Good Luck!", "GO!");
      break;

    case STATE_BALL_IN_PLAY:
      setSubsystemsPower(false, false, false, false, false, false);
      if (stats.currentScore > stats.highScore) {
        stats.highScore = stats.currentScore;
        savePersistentState(false);
        playSound(10); // High scorer special audio
        updateDisplay("NEW HIGH SCORE!", String(stats.highScore), "NEW HI!");
      } else {
        updateDisplay("ROUND OVER", "Final Score: " + String(stats.currentScore), "OVER");
      }
      break;

    case STATE_GAMEOVER:
      Serial.println(F("[EXIT] Game Over timer expired."));
      break;
  }

  // --- 2. SWITCH STATE & EXECUTE ENTRY ACTIONS ---
  currentState = nextState;
  telemetry.stateEntryTime = millis();
  waitingExitDelay = false;

  switch (currentState) {
    case STATE_IDLE:
      Serial.println(F("\n=== ENTERING STATE_IDLE ==="));
      // Coin sensor ON; all other sensors/actuators in low power
      setSubsystemsPower(true, false, false, false, false, false);
      telemetry.circuitsHealthy = runCircuitHealthCheck();
      telemetry.lastHealthCheckTime = millis();
      updateDisplay("INSERT PENNY", "High Score: " + String(stats.highScore), "1 CENT");
      playSound(1);
      break;

    case STATE_READY:
      Serial.println(F("\n=== ENTERING STATE_READY ==="));
      // Plunger lane sensor ON; others low power
      setSubsystemsPower(false, true, false, false, false, false);
      dispTimeRemaining = 60;
      updateDisplay("LOAD BALL", "High Score: " + String(stats.highScore), "LOAD");
      playSound(2);
      break;

    case STATE_BALL_IN_PLUNGER:
      Serial.println(F("\n=== ENTERING STATE_BALL_IN_PLUNGER ==="));
      // Keep plunger sensor ON + turn ON Ball-in-Play lane sensor
      setSubsystemsPower(false, true, true, false, false, false);
      dispTimeRemaining  = 60;
      updateDisplay("PULL PLUNGER!", "Launch ball to begin. High Score: " + String(stats.highScore), "LAUNCH");
      playSound(3);
      break;

    case STATE_BALL_IN_PLAY:
      Serial.println(F("\n=== ENTERING STATE_BALL_IN_PLAY ==="));
      // Flippers, Drain, and Target circuits ON; lane sensors OFF
      setSubsystemsPower(false, false, false, true, true, true);
      stats.currentScore = 0;
      stats.targetsHit   = 0;
      stats.multiplier   = 1;
      dispTimeRemaining  = 60;
      playSound(4); // 1, 2, 3 countdown audio
      updateDisplay("3.. 2.. 1.. GO!", "Hit targets to score!", "PLAY");
      lastPeriodicTick = millis();

      if (isTargetHit()) {
        stats.targetsHit++;
        stats.multiplier = 1 + (stats.targetsHit / 5);
        stats.currentScore += (100 * stats.multiplier);
        triggerTargetHitLEDs(); // <-- Triggers 180ms WS2812B gold flash
        playSound(6);
        delay(40);
      }

      break;

    case STATE_GAMEOVER:
      Serial.println(F("\n=== ENTERING STATE_GAMEOVER ==="));
      
      updateLEDs((stats.currentScore >= stats.highScore) ? 6 : 5); // <-- High score celebration vs standard
      
      dispTimeRemaining = (TIME_GAMEOVER_DURATION - (now - telemetry.stateEntryTime)) / 1000;
      if (now - telemetry.stateEntryTime >= TIME_GAMEOVER_DURATION) {
        transitionTo(STATE_IDLE);
      }

      // Turn off all sensor & actuator circuits
      setSubsystemsPower(false, false, false, false, false, false);
      savePersistentState(false); // Game completed cleanly; clear penny escrow
      if (stats.currentScore >= stats.highScore) {
        playSound(11); // High score celebration
        updateDisplay("NEW CHAMPION!", "HI: " + String(stats.highScore), "WIN!");
      } else {
        playSound(5);
        updateDisplay("GAME OVER", "Score: " + String(stats.currentScore), "END");
      }
      break;
  }
}

// ==========================================
// SETUP & MAIN LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  initHardware();
  initWiFiDisplay();

  // Check for unclean shutdown / power loss before entering STATE_IDLE
  loadAndRecoverPersistentState();

  // Force initial entry into STATE_IDLE
  currentState = STATE_GAMEOVER;
  transitionTo(STATE_IDLE);
}

void loop() {
  unsigned long now = millis();
  handleWebDisplayClient(); // Serve live ESP32-S3 scoreboard requests
  updateAudio(); // Advance non-blocking piezo sounder melodies

  switch (currentState) {
    // ---------------------------------------------------------
    // 1. STATE_IDLE
    // ---------------------------------------------------------
    case STATE_IDLE:
      updateLEDs(1);

      // Periodic self-check timer
      if (now - telemetry.lastHealthCheckTime >= TIME_HEALTH_CHECK_INTERVAL) {
        telemetry.circuitsHealthy = runCircuitHealthCheck();
        telemetry.lastHealthCheckTime = now;
      }

      // Exit Condition: Penny drop detected
      if (isCoinDetected()) {
        telemetry.pennyDropTime = now;
        savePersistentState(true); // Mark penny escrowed in case of power loss
        transitionTo(STATE_READY);
      }
      break;

    // ---------------------------------------------------------
    // 2. STATE_READY
    // ---------------------------------------------------------
    case STATE_READY:
      updateLEDs(2);

      // Handle 10-second post-detection wait before transitioning
      if (waitingExitDelay) {
        if (now - exitDelayStart >= TIME_READY_EXIT_DELAY) {
          transitionTo(STATE_BALL_IN_PLUNGER);
        }
        break;
      }

      dispTimeRemaining = (TIME_READY_TIMEOUT - (now - telemetry.stateEntryTime)) / 1000;

      // Exit Condition 1: Ball detected in plunger lane
      if (isBallInPlungerLane()) {
        telemetry.plungerBallDetectTime = now;
        waitingExitDelay = true;
        exitDelayStart = now;
        updateDisplay("BALL DETECTED", "Wait 10 seconds...");
      }
      // Timeout Edge Case: No ball detected in 60s -> Return penny & clean up
      else if (now - telemetry.stateEntryTime >= TIME_READY_TIMEOUT) {
        Serial.println(F("[TIMEOUT] No ball in plunger within 60s."));
        returnPenny();
        savePersistentState(false);
        memset(&telemetry, 0, sizeof(telemetry));
        transitionTo(STATE_IDLE);
      }
      break;

    // ---------------------------------------------------------
    // 3. STATE_BALL_IN_PLUNGER
    // ---------------------------------------------------------
    case STATE_BALL_IN_PLUNGER:
      updateLEDs(3);

      dispTimeRemaining = (TIME_PLUNGER_TIMEOUT - (now - telemetry.stateEntryTime)) / 1000;

      // Exit Condition 1: Ball enters Ball-in-Play lane
      if (isBallInPlayLane()) {
        transitionTo(STATE_BALL_IN_PLAY);
      }
      // Edge Case: Ball removed/fell back before launch
      else if (!isBallInPlungerLane()) {
        Serial.println(F("[WARNING] Ball removed from plunger lane!"));
        dispMessage = "Warning: Ball removed from lane!";
      }
      // Timeout Edge Case: Player inactive for 60s -> Return penny & reset
      else if (now - telemetry.stateEntryTime >= TIME_PLUNGER_TIMEOUT) {
        Serial.println(F("[TIMEOUT] Player did not launch within 60s."));
        returnPenny();
        savePersistentState(false);
        memset(&telemetry, 0, sizeof(telemetry));
        transitionTo(STATE_IDLE);
      }
      break;

    // ---------------------------------------------------------
    // 4. STATE_BALL_IN_PLAY
    // ---------------------------------------------------------
    case STATE_BALL_IN_PLAY:
      updateLEDs(4);

      // Check target hits
      if (isTargetHit()) {
        stats.targetsHit++;
        stats.multiplier = 1 + (stats.targetsHit / 5);
        stats.currentScore += (100 * stats.multiplier);
        playSound(6);
        delay(50); // Simple switch debounce
      }

      // Continuous Action: Update display every 1 second
      if (now - lastPeriodicTick >= TIME_DISPLAY_REFRESH) {
        lastPeriodicTick = now;
        
        dispTimeRemaining = (TIME_GAMEPLAY_MAX - (now - telemetry.stateEntryTime)) / 1000;

        unsigned long timeLeft = (TIME_GAMEPLAY_MAX - (now - telemetry.stateEntryTime)) / 1000;
        updateDisplay("SC:" + String(stats.currentScore) + " x" + String(stats.multiplier),
                      "T:" + String(timeLeft) + "s HITS:" + String(stats.targetsHit));
      }

      // Exit Condition: Ball in drain OR 60-second gameplay timer expires
      if (isBallInDrain() || (now - telemetry.stateEntryTime >= TIME_GAMEPLAY_MAX)) {
        transitionTo(STATE_GAMEOVER);
      }
      break;

    // ---------------------------------------------------------
    // 5. STATE_GAMEOVER
    // ---------------------------------------------------------
    case STATE_GAMEOVER:
      updateLEDs(5);

      dispTimeRemaining = (TIME_GAMEOVER_DURATION - (now - telemetry.stateEntryTime)) / 1000;

      // Exit Condition: 10-second in-state timer expires
      if (now - telemetry.stateEntryTime >= TIME_GAMEOVER_DURATION) {
        transitionTo(STATE_IDLE);
      }
      break;
  }
}