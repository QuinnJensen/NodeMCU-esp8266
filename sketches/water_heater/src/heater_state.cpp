// heater_state.cpp
#include "heater_state.h"
#include <math.h>
#include <LittleFS.h>
#include "display_ui.h"

#define WH_SSR_SIM_PIN D1
#define WH_CAL_FILE "/cal.json"
#define WH_TIMER1_DIVIDER TIM_DIV16
#define WH_TIMER1_TICKS ((5000000UL / WH_MODULATOR_HZ) - 1)

// Global state for Bresenham modulator (volatile and explicitly in DRAM)
volatile uint8_t  isrPowerPct    __attribute__((section(".iram.data"))) = 0;
volatile uint8_t  isrOutputState __attribute__((section(".iram.data"))) = 0;
volatile bool     isrThermalHalt __attribute__((section(".iram.data"))) = false;
volatile uint32_t simTickCount   __attribute__((section(".iram.data"))) = 0;
volatile uint32_t simOnTickCount __attribute__((section(".iram.data"))) = 0;

// Variables used by ISR MUST be in DRAM (not Flash) on ESP8266.
static volatile int32_t bresAcc            __attribute__((section(".iram.data"))) = 0;
static volatile uint8_t isrGateTimer       __attribute__((section(".iram.data"))) = 0;

int requestedPowerPct = 0;
int priorPowerPct = 0;
int displayedPowerWatts = 0;
unsigned long lastCommandMs = 0;
unsigned long powerLevelChangedMs = 0;
float calTable[WH_CAL_POINTS];

void initHeaterIo() {
  Serial.print("[MOD] Initializing SSR Pin (D1/GPIO5)... ");
  pinMode(WH_SSR_SIM_PIN, OUTPUT);
  digitalWrite(WH_SSR_SIM_PIN, LOW);
  
  noInterrupts();
  bresAcc = 0;
  simTickCount = 0;
  simOnTickCount = 0;
  isrPowerPct = 0;
  isrThermalHalt = false;
  isrGateTimer = 0;
  interrupts();
  Serial.println("done.");
}

static void IRAM_ATTR modulatorIsr() {
  simTickCount++;
  
  // 1. De-assertion logic (One-shot window timing)
  // We handle this FIRST to ensure a clean transition if a new 'ON' starts this same tick.
  if (isrGateTimer > 0) {
    if (--isrGateTimer == 0) {
      GPOC = (1 << 5); // GPIO5 (D1) LOW
      isrOutputState = 0;
    }
  }

  // 2. Bresenham decision logic runs at 60Hz (every 2nd tick of the 120Hz timer)
  if ((simTickCount % 2) == 0) {
    // Safety: ensure isrPowerPct is within valid 0-100 range
    uint8_t p = isrPowerPct;
    if (p > 100) p = 100;
    
    bresAcc += (int32_t)p;
    
    if (bresAcc >= 100) {
      bresAcc -= 100;
      
      // Safety: Prevent accumulator runaway
      if (bresAcc > 100) bresAcc = 0;

      if (!isrThermalHalt && p > 0) {
        // Assert gate high for exactly 2 ticks (2 * 8.333ms = 16.666ms)
        // This is the 'Perfect Pulse' for a 60Hz zero-cross SSR.
        GPOS = (1 << 5); // GPIO5 (D1) HIGH
        isrOutputState = 1;
        isrGateTimer = 2; 
        simOnTickCount++;
      }
    }
  }
}

void serviceModulatorOneShot() {
  // Logic removed; de-assertion is now handled in the 120Hz ISR
}

void startModulator() {
  initHeaterIo();
  timer1_isr_init();
  timer1_attachInterrupt(modulatorIsr);
  timer1_enable(WH_TIMER1_DIVIDER, TIM_EDGE, TIM_LOOP);
  timer1_write(WH_TIMER1_TICKS);
}

void clearCalibrationTable() {
  for (int i = 0; i < WH_CAL_POINTS; i++) calTable[i] = -1.0f;
}

bool hasAnyCalibration() {
  for (int i = 0; i < WH_CAL_POINTS; i++) if (calTable[i] >= 0.0f) return true;
  return false;
}

static int calIndexFromPercent(int pct) {
  pct = constrain(pct, 10, 100);
  int idx = (pct / 10) - 1;
  if (idx < 0) idx = 0;
  if (idx >= WH_CAL_POINTS) idx = WH_CAL_POINTS - 1;
  return idx;
}

static int calPercentFromIndex(int idx) { return (idx + 1) * 10; }

static float nominalWattsForPercent(int pct) {
  pct = constrain(pct, 0, 100);
  return (WH_FULL_SCALE_WATTS * pct) / 100.0f;
}

static float interpolateSegment(int x, int x0, float y0, int x1, float y1) {
  if (x1 == x0) return y0;
  return y0 + ((float)(x - x0) * (y1 - y0)) / (float)(x1 - x0);
}

float estimateCorrectedWatts(int pct) {
  pct = constrain(pct, 0, 100);
  if (pct == 0) return 0.0f;
  if (!hasAnyCalibration()) return nominalWattsForPercent(pct);

  if (pct % 10 == 0) {
    int exactIdx = calIndexFromPercent(pct);
    if (calTable[exactIdx] >= 0.0f) return calTable[exactIdx];
  }

  int lowerIdx = -1, upperIdx = -1;
  int exactBucket = pct / 10;
  for (int i = exactBucket - 1; i >= 0; i--) {
    if (i >= 1 && calTable[i - 1] >= 0.0f) { lowerIdx = i - 1; break; }
  }
  for (int i = (pct + 9) / 10; i <= 10; i++) {
    if (i >= 1 && i <= 10 && calTable[i - 1] >= 0.0f) { upperIdx = i - 1; break; }
  }
  if (lowerIdx >= 0 && upperIdx >= 0 && lowerIdx != upperIdx) {
    return interpolateSegment(pct, calPercentFromIndex(lowerIdx), calTable[lowerIdx],
                              calPercentFromIndex(upperIdx), calTable[upperIdx]);
  }
  if (lowerIdx >= 0) {
    int x0 = calPercentFromIndex(lowerIdx);
    float y0 = calTable[lowerIdx];
    int prevIdx = -1;
    for (int i = lowerIdx - 1; i >= 0; i--) if (calTable[i] >= 0.0f) { prevIdx = i; break; }
    if (prevIdx >= 0) return interpolateSegment(pct, calPercentFromIndex(prevIdx), calTable[prevIdx], x0, y0);
    return interpolateSegment(pct, 0, 0.0f, x0, y0);
  }
  if (upperIdx >= 0) {
    int x1 = calPercentFromIndex(upperIdx);
    float y1 = calTable[upperIdx];
    int nextIdx = -1;
    for (int i = upperIdx + 1; i < WH_CAL_POINTS; i++) if (calTable[i] >= 0.0f) { nextIdx = i; break; }
    if (nextIdx >= 0) return interpolateSegment(pct, x1, y1, calPercentFromIndex(nextIdx), calTable[nextIdx]);
    return interpolateSegment(pct, 0, 0.0f, x1, y1);
  }
  return nominalWattsForPercent(pct);
}

float estimateCurrentAmps(float estWatts) {
  if (WH_NOMINAL_VRMS <= 0.0f) return 0.0f;
  return estWatts / WH_NOMINAL_VRMS;
}

void refreshDisplayedPower() {
  float w = estimateCorrectedWatts(requestedPowerPct);
  if (w < 0.0f) w = 0.0f;
  displayedPowerWatts = (int)lroundf(w);
}

void updatePowerValues(int pct) {
  pct = constrain(pct, 0, 100);
  if (pct != requestedPowerPct) {
    priorPowerPct = requestedPowerPct;
    requestedPowerPct = pct;
    powerLevelChangedMs = millis();
  } else {
    requestedPowerPct = pct;
  }
  refreshDisplayedPower();
  noInterrupts();
  isrPowerPct = requestedPowerPct;
  interrupts();
}

bool loadCalibration() {
  clearCalibrationTable();
  if (!LittleFS.exists(WH_CAL_FILE)) return false;
  File f = LittleFS.open(WH_CAL_FILE, "r");
  if (!f) return false;
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  JsonArray arr = doc["points"].as<JsonArray>();
  if (arr.isNull()) return false;
  for (int i = 0; i < WH_CAL_POINTS && i < (int)arr.size(); i++) {
    calTable[i] = arr[i] | -1.0f;
  }
  return true;
}

bool saveCalibration() {
  StaticJsonDocument<256> doc;
  JsonArray arr = doc.createNestedArray("points");
  for (int i = 0; i < WH_CAL_POINTS; i++) arr.add(calTable[i]);
  File f = LittleFS.open(WH_CAL_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

bool purgeCalibrationFile() {
  clearCalibrationTable();
  if (LittleFS.exists(WH_CAL_FILE) && !LittleFS.remove(WH_CAL_FILE)) {
    setStatusMessage("cal purge fail", 2500);
    return false;
  }
  refreshDisplayedPower();
  setStatusMessage("cal purged", 2000);
  return true;
}

bool handleCalibrationRequest(float actualWatts) {
  int pct = requestedPowerPct;
  if (pct < 10) { setStatusMessage("set pwr >=10", 2500); return false; }
  if (actualWatts <= 0.0f) { setStatusMessage("bad cal watts", 2500); return false; }
  int bucketPct = ((pct + 5) / 10) * 10;
  bucketPct = constrain(bucketPct, 10, 100);
  int idx = calIndexFromPercent(bucketPct);
  calTable[idx] = actualWatts;
  if (!saveCalibration()) { setStatusMessage("cal save fail", 2500); return false; }
  refreshDisplayedPower();
  setStatusMessage("cal stored", 2000);
  return true;
}

void appendHeaterStateToJson(JsonDocument& doc) {
  uint8_t  simState; uint32_t ticks; uint32_t onTicks;
  noInterrupts();
  simState = isrOutputState;
  ticks = simTickCount;
  onTicks = simOnTickCount;
  interrupts();

  float estWatts = estimateCorrectedWatts(requestedPowerPct);
  doc["power_percent"] = requestedPowerPct;
  doc["prior_power_percent"] = priorPowerPct;
  doc["est_power_watts"] = estWatts;
  doc["est_current_amps"] = estimateCurrentAmps(estWatts);
  doc["seconds_since_last_command"] = (millis() - lastCommandMs) / 1000UL;
  doc["seconds_at_current_power_level"] = (millis() - powerLevelChangedMs) / 1000UL;
  doc["sim_output"] = simState;
  doc["sim_ticks"] = ticks;
  doc["sim_on_ticks"] = onTicks;
  doc["isr_thermal_halt"] = isrThermalHalt;
  doc["calibration_enabled"] = hasAnyCalibration();
  JsonArray cal = doc.createNestedArray("calibration_points");
  for (int i = 0; i < WH_CAL_POINTS; i++) cal.add(calTable[i]);
}
