#pragma once
#include <Arduino.h>

void handleCommandJson(const String& payload);
void publishUhfStatus(bool retained);
void publishCommandResult(const char* type, bool ok, const char* message);
void serviceUhfDeferred();
extern bool pendingScan;
