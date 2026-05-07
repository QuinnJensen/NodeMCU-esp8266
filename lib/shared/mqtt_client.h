#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

using MqttMessageHandler  = void (*)(const String& topic, const String& payload);
using MqttConnectedHandler = void (*)();

struct MqttClientDisplay {
  void (*kickSpinner)(unsigned long durationMs) = nullptr;
  void (*setStatus)(const String& msg, unsigned long holdMs) = nullptr;
};

void setMqttClientDisplayCallbacks(const MqttClientDisplay& cb);
void setMqttMessageHandler(MqttMessageHandler handler);
void setMqttConnectedHandler(MqttConnectedHandler handler);

bool publishJsonDocToTopic(const char* topic, const JsonDocument& doc, bool retained);
void initMqttClient();
void startMqttIfWifiReady();
void serviceMqttClient();

// Exposed for display -- returns human-readable reconnect state label
const char* mqttReconnectStateLabel();
