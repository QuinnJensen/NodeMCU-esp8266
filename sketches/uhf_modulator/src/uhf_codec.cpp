// uhf_codec.cpp
#include "uhf_codec.h"
#include <LittleFS.h>
#include <ctype.h>

#define UHF_TX_PIN  D1   // RF DATA output (D1/GPIO5; safer than D3 boot strap pin)
#define UHF_LED_PIN D4   // onboard blue LED (active LOW)

TimingProfile profiles[UHF_MAX_PROFILES];
CodeRecord    codes[UHF_MAX_CODES];
uint8_t profileCount = 0;
uint8_t codeCount    = 0;
bool    txBusy       = false;
uint32_t txCount     = 0;
char     lastTxId[UHF_ID_LEN]        = "-";
char     lastTxProfileId[UHF_ID_LEN] = "-";
uint32_t lastTxValue = 0;
uint16_t lastTxBits  = 0;
uint16_t lastTxRepeat = 0;
unsigned long lastTxMs = 0;

void initUhfIo() {
  pinMode(UHF_TX_PIN, OUTPUT);
  digitalWrite(UHF_TX_PIN, LOW);
  pinMode(UHF_LED_PIN, OUTPUT);
  digitalWrite(UHF_LED_PIN, HIGH);
}

void clearProfiles() {
  profileCount = 0;
  for (uint8_t i = 0; i < UHF_MAX_PROFILES; i++) profiles[i].active = false;
}

void clearCodes() {
  codeCount = 0;
  for (uint8_t i = 0; i < UHF_MAX_CODES; i++) codes[i].active = false;
}

bool validateId(const char* s) {
  if (!s || !s[0]) return false;
  size_t n = strlen(s);
  if (n >= UHF_ID_LEN) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (!(isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.')) return false;
  }
  return true;
}

int findProfileIndexById(const char* id) {
  for (uint8_t i = 0; i < profileCount; i++) {
    if (profiles[i].active && !strcmp(profiles[i].id, id)) return i;
  }
  return -1;
}

int findCodeIndexById(const char* id) {
  for (uint8_t i = 0; i < codeCount; i++) {
    if (codes[i].active && !strcmp(codes[i].id, id)) return i;
  }
  return -1;
}

void seedDefaultProfiles() {
  clearProfiles();
  TimingProfile p;
  memset(&p, 0, sizeof(p));
  strlcpy(p.id, "proto1_350us", sizeof(p.id));
  p.pulseUs=350; p.syncHigh=1; p.syncLow=31; p.zeroHigh=1; p.zeroLow=3; p.oneHigh=3; p.oneLow=1; p.defaultBits=24; p.defaultRepeat=10; p.active=true;
  upsertProfile(p, false);
  memset(&p, 0, sizeof(p));
  strlcpy(p.id, "proto1_354us_compat", sizeof(p.id));
  p.pulseUs=354; p.syncHigh=1; p.syncLow=31; p.zeroHigh=1; p.zeroLow=3; p.oneHigh=3; p.oneLow=1; p.defaultBits=24; p.defaultRepeat=25; p.active=true;
  upsertProfile(p, false);
  saveProfiles();
}

void seedDefaultCodes() {
  clearCodes();
  CodeRecord c;
  auto add = [&](const char* id, uint32_t v, uint16_t bits, uint16_t rep) {
    memset(&c, 0, sizeof(c));
    strlcpy(c.id, id, sizeof(c.id));
    strlcpy(c.profileId, "proto1_350us", sizeof(c.profileId));
    c.value=v; c.bits=bits; c.repeat=rep; c.active=true;
    upsertCode(c, false);
  };
  add("giandel_on",    14199672UL,  24, 10);
  add("giandel_off",   14199668UL,  24, 10);
  add("heater_on",     757795032UL, 31, 1);
  add("heater_off",    757793412UL, 31, 1);
  add("heater_plus",   757793912UL, 31, 1);
  add("heater_minus",  757793092UL, 31, 1);
  saveCodes();
}

bool profileFromJson(JsonObjectConst obj, TimingProfile& out) {
  const char* id = obj["id"] | "";
  if (!validateId(id)) return false;
  uint32_t pulseUs = obj["pulse_us"] | 0;
  uint32_t syncHigh = obj["sync_high"] | 0;
  uint32_t syncLow  = obj["sync_low"]  | 0;
  uint32_t zeroHigh = obj["zero_high"] | 0;
  uint32_t zeroLow  = obj["zero_low"]  | 0;
  uint32_t oneHigh  = obj["one_high"]  | 0;
  uint32_t oneLow   = obj["one_low"]   | 0;
  uint32_t defaultBits   = obj["default_bits"] | 24;
  uint32_t defaultRepeat = obj["default_repeat"] | 10;
  if (pulseUs < 50 || pulseUs > 5000) return false;
  if (!syncHigh || !syncLow || !zeroHigh || !zeroLow || !oneHigh || !oneLow) return false;
  if (defaultBits < 1 || defaultBits > 64) return false;
  if (defaultRepeat < 1 || defaultRepeat > 100) return false;
  memset(&out, 0, sizeof(out));
  strlcpy(out.id, id, sizeof(out.id));
  out.pulseUs=pulseUs; out.syncHigh=syncHigh; out.syncLow=syncLow;
  out.zeroHigh=zeroHigh; out.zeroLow=zeroLow; out.oneHigh=oneHigh; out.oneLow=oneLow;
  out.defaultBits=defaultBits; out.defaultRepeat=defaultRepeat; out.active=true;
  return true;
}

bool codeFromJson(JsonObjectConst obj, CodeRecord& out) {
  const char* id = obj["id"] | "";
  const char* profileId = obj["profileId"] | "";
  if (!validateId(id) || !validateId(profileId)) return false;
  if (findProfileIndexById(profileId) < 0) return false;
  uint32_t value = obj["value"] | 0;
  uint32_t bits = obj["bits"] | 0;
  uint32_t repeat = obj["repeat"] | 0;
  if (bits < 1 || bits > 64) return false;
  if (repeat > 100) return false;
  memset(&out, 0, sizeof(out));
  strlcpy(out.id, id, sizeof(out.id));
  strlcpy(out.profileId, profileId, sizeof(out.profileId));
  out.value=value; out.bits=bits; out.repeat=repeat; out.active=true;
  return true;
}

bool upsertProfile(const TimingProfile& p, bool persist) {
  int idx = findProfileIndexById(p.id);
  if (idx >= 0) profiles[idx] = p;
  else {
    if (profileCount >= UHF_MAX_PROFILES) return false;
    profiles[profileCount++] = p;
  }
  return persist ? saveProfiles() : true;
}

bool upsertCode(const CodeRecord& c, bool persist) {
  int idx = findCodeIndexById(c.id);
  if (idx >= 0) codes[idx] = c;
  else {
    if (codeCount >= UHF_MAX_CODES) return false;
    codes[codeCount++] = c;
  }
  return persist ? saveCodes() : true;
}

bool profileIsInUse(const char* profileId) {
  for (uint8_t i = 0; i < codeCount; i++) {
    if (codes[i].active && !strcmp(codes[i].profileId, profileId)) return true;
  }
  return false;
}

bool deleteProfileById(const char* id) {
  int idx = findProfileIndexById(id);
  if (idx < 0) return false;
  if (profileIsInUse(id)) return false;
  for (uint8_t i = idx; i + 1 < profileCount; i++) profiles[i] = profiles[i + 1];
  if (profileCount) profileCount--;
  return saveProfiles();
}

bool deleteCodeById(const char* id) {
  int idx = findCodeIndexById(id);
  if (idx < 0) return false;
  for (uint8_t i = idx; i + 1 < codeCount; i++) codes[i] = codes[i + 1];
  if (codeCount) codeCount--;
  return saveCodes();
}

bool loadProfiles() {
  clearProfiles();
  if (!LittleFS.exists(UHF_PROFILES_FILE)) {
    seedDefaultProfiles();
    return true;
  }
  File f = LittleFS.open(UHF_PROFILES_FILE, "r");
  if (!f) return false;
  StaticJsonDocument<3072> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  JsonArrayConst arr = doc["profiles"].as<JsonArrayConst>();
  if (arr.isNull()) return false;
  for (JsonObjectConst obj : arr) {
    if (profileCount >= UHF_MAX_PROFILES) break;
    TimingProfile p;
    if (profileFromJson(obj, p)) profiles[profileCount++] = p;
  }
  if (!profileCount) seedDefaultProfiles();
  return true;
}

bool saveProfiles() {
  StaticJsonDocument<3072> doc;
  JsonArray arr = doc.createNestedArray("profiles");
  for (uint8_t i = 0; i < profileCount; i++) {
    JsonObject p = arr.createNestedObject();
    p["id"]=profiles[i].id; p["pulse_us"]=profiles[i].pulseUs;
    p["sync_high"]=profiles[i].syncHigh; p["sync_low"]=profiles[i].syncLow;
    p["zero_high"]=profiles[i].zeroHigh; p["zero_low"]=profiles[i].zeroLow;
    p["one_high"]=profiles[i].oneHigh;   p["one_low"]=profiles[i].oneLow;
    p["default_bits"]=profiles[i].defaultBits;
    p["default_repeat"]=profiles[i].defaultRepeat;
  }
  File f = LittleFS.open(UHF_PROFILES_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

bool loadCodes() {
  clearCodes();
  if (!LittleFS.exists(UHF_CODES_FILE)) {
    seedDefaultCodes();
    return true;
  }
  File f = LittleFS.open(UHF_CODES_FILE, "r");
  if (!f) return false;
  StaticJsonDocument<4096> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  JsonArrayConst arr = doc["codes"].as<JsonArrayConst>();
  if (arr.isNull()) return false;
  for (JsonObjectConst obj : arr) {
    if (codeCount >= UHF_MAX_CODES) break;
    CodeRecord c;
    if (codeFromJson(obj, c)) codes[codeCount++] = c;
  }
  if (!codeCount) seedDefaultCodes();
  return true;
}

bool saveCodes() {
  StaticJsonDocument<4096> doc;
  JsonArray arr = doc.createNestedArray("codes");
  for (uint8_t i = 0; i < codeCount; i++) {
    JsonObject c = arr.createNestedObject();
    c["id"]=codes[i].id; c["profileId"]=codes[i].profileId;
    c["value"]=codes[i].value; c["bits"]=codes[i].bits; c["repeat"]=codes[i].repeat;
  }
  File f = LittleFS.open(UHF_CODES_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

void appendProfilesToJson(JsonDocument& doc) {
  JsonArray arr = doc.createNestedArray("profiles");
  for (uint8_t i = 0; i < profileCount; i++) {
    JsonObject p = arr.createNestedObject();
    p["id"]=profiles[i].id; p["pulse_us"]=profiles[i].pulseUs;
    p["sync_high"]=profiles[i].syncHigh; p["sync_low"]=profiles[i].syncLow;
    p["zero_high"]=profiles[i].zeroHigh; p["zero_low"]=profiles[i].zeroLow;
    p["one_high"]=profiles[i].oneHigh;   p["one_low"]=profiles[i].oneLow;
    p["default_bits"]=profiles[i].defaultBits;
    p["default_repeat"]=profiles[i].defaultRepeat;
  }
}

void appendCodesToJson(JsonDocument& doc) {
  JsonArray arr = doc.createNestedArray("codes");
  for (uint8_t i = 0; i < codeCount; i++) {
    JsonObject c = arr.createNestedObject();
    c["id"]=codes[i].id; c["profileId"]=codes[i].profileId;
    c["value"]=codes[i].value; c["bits"]=codes[i].bits; c["repeat"]=codes[i].repeat;
  }
}

static void IRAM_ATTR setRfAndLed(bool high) {
  digitalWrite(UHF_TX_PIN, high ? HIGH : LOW);
  digitalWrite(UHF_LED_PIN, high ? LOW : HIGH);
}

static void txDelayUs(uint32_t us) {
  uint32_t start = micros();
  while ((uint32_t)(micros() - start) < us) {
    ESP.wdtFeed();
  }
}

bool transmitWaveform(const TimingProfile& p, uint32_t value, uint16_t bits, uint16_t repeat) {
  if (bits < 1 || bits > 32) return false;
  txBusy = true;
  strlcpy(lastTxProfileId, p.id, sizeof(lastTxProfileId));
  lastTxValue = value; lastTxBits = bits; lastTxRepeat = repeat;
  noInterrupts();
  for (uint16_t r = 0; r < repeat; r++) {
    for (int8_t b = bits - 1; b >= 0; b--) {
      bool one = ((value >> b) & 0x1U) != 0;
      uint32_t highUs = (one ? p.oneHigh : p.zeroHigh) * (uint32_t)p.pulseUs;
      uint32_t lowUs  = (one ? p.oneLow  : p.zeroLow ) * (uint32_t)p.pulseUs;
      setRfAndLed(true);
      txDelayUs(highUs);
      setRfAndLed(false);
      txDelayUs(lowUs);
    }
    setRfAndLed(true);
    txDelayUs((uint32_t)p.syncHigh * p.pulseUs);
    setRfAndLed(false);
    txDelayUs((uint32_t)p.syncLow * p.pulseUs);
  }
  interrupts();
  digitalWrite(UHF_TX_PIN, LOW);
  digitalWrite(UHF_LED_PIN, HIGH);
  txBusy = false;
  txCount++;
  lastTxMs = millis();
  return true;
}

bool transmitCodeRecord(const CodeRecord& c) {
  int pidx = findProfileIndexById(c.profileId);
  if (pidx < 0) return false;
  strlcpy(lastTxId, c.id, sizeof(lastTxId));
  uint16_t bits = c.bits ? c.bits : profiles[pidx].defaultBits;
  uint16_t repeat = c.repeat ? c.repeat : profiles[pidx].defaultRepeat;
  return transmitWaveform(profiles[pidx], c.value, bits, repeat);
}
