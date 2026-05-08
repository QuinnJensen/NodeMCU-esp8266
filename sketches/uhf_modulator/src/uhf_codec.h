// uhf_codec.h - timing profile / code table storage and OOK transmitter
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#define UHF_MAX_PROFILES 16
#define UHF_MAX_CODES 32
#define UHF_ID_LEN 32

#define UHF_PROFILES_FILE "/profiles.json"
#define UHF_CODES_FILE    "/codes.json"

struct TimingProfile {
  char id[UHF_ID_LEN];
  uint16_t pulseUs;
  uint16_t syncHigh;
  uint16_t syncLow;
  uint16_t zeroHigh;
  uint16_t zeroLow;
  uint16_t oneHigh;
  uint16_t oneLow;
  uint16_t defaultBits;
  uint16_t defaultRepeat;
  bool active;
};

struct CodeRecord {
  char id[UHF_ID_LEN];
  char profileId[UHF_ID_LEN];
  uint32_t value;
  uint16_t bits;
  uint16_t repeat;
  bool active;
};

extern TimingProfile profiles[UHF_MAX_PROFILES];
extern CodeRecord    codes[UHF_MAX_CODES];
extern uint8_t       profileCount;
extern uint8_t       codeCount;
extern bool          txBusy;
extern uint32_t      txCount;
extern char          lastTxId[UHF_ID_LEN];
extern char          lastTxProfileId[UHF_ID_LEN];
extern uint32_t      lastTxValue;
extern uint16_t      lastTxBits;
extern uint16_t      lastTxRepeat;
extern unsigned long lastTxMs;

void initUhfIo();
void clearProfiles();
void clearCodes();
void seedDefaultProfiles();
void seedDefaultCodes();
bool loadProfiles();
bool saveProfiles();
bool loadCodes();
bool saveCodes();
int  findProfileIndexById(const char* id);
int  findCodeIndexById(const char* id);
bool validateId(const char* s);
bool profileFromJson(JsonObjectConst obj, TimingProfile& out);
bool codeFromJson(JsonObjectConst obj, CodeRecord& out);
void appendProfilesToJson(JsonDocument& doc);
void appendCodesToJson(JsonDocument& doc);
bool upsertProfile(const TimingProfile& p, bool persist = true);
bool upsertCode(const CodeRecord& c, bool persist = true);
bool deleteProfileById(const char* id);
bool deleteCodeById(const char* id);
bool profileIsInUse(const char* profileId);

bool transmitWaveform(const TimingProfile& p, uint32_t value, uint16_t bits, uint16_t repeat);
bool transmitCodeRecord(const CodeRecord& c);
