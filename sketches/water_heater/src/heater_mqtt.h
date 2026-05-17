#pragma once
#include <Arduino.h>

void handleCommandJson(const String& payload);
void publishHeaterStatus(bool retained);
#ifdef SHARED_LIB_USE_ONEWIRE
void publishPerSensorStatuses();
#endif
void publishFilesystemListing();
void publishCommandResult(const char* type, bool ok, const char* message);
