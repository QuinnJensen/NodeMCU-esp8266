#pragma once
#include <Arduino.h>

void handleCommandJson(const String& payload);
void publishHeaterStatus(bool retained);
void publishFilesystemListing();
void publishCommandResult(const char* type, bool ok, const char* message);
