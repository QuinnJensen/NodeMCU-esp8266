#pragma once
#include <Arduino.h>
#include <DallasTemperature.h>

void initSensorBus();
void scanSensors(bool force = false);
void requestTemperatureConversion();       // async: fires conversion, returns immediately
void collectTemperatureResults();          // async: reads results (call 800ms after request)
void readTemperatures();                   // blocking shim for setup(), feeds WDT safely
void sampleSensors();                      // scan + async-safe read
String sensorAddressString(uint8_t i);
String defaultSensorNameForAddress(const DeviceAddress addr);  // generates sens### fallback name

extern bool conversionPending;
extern unsigned long conversionRequestedMs;
