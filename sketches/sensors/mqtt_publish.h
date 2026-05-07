// mqtt_publish.h -- sensor/water domain publish functions (sketch-local)
#pragma once
#include <Arduino.h>

void publishAggregateStatus(bool retained = false);
void publishPerSensorStatus(uint8_t i, bool retained = false);
void publishPerSensorStatuses(bool retained = false);
void publishWaterStatus(bool retained = false);
void publishCommandResult(const char* type, bool ok, const char* msg = nullptr);
void initialSampleAndPublish();
