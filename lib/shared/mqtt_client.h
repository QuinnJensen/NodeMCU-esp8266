// mqtt_client.h -- pure MQTT transport, no sensor/sketch dependencies
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// Optional display callbacks
struct MqttClientDisplay {
  void (*kickSpinner)(unsigned long durationMs) = nullptr;
  void (*setStatus)(const String& msg, unsigned long holdMs) = nullptr;
};
void setMqttClientDisplayCallbacks(const MqttClientDisplay& cb);

// MQTT message callback type -- sketch registers this to handle incoming messages
typedef void (*MqttMessageHandler)(const String& topic, const String& payload);
void setMqttMessageHandler(MqttMessageHandler handler);

void initMqttClient();
void startMqttIfWifiReady();
void serviceMqttClient();
bool mqttConnect();

// Generic publish -- available to sketch for building domain messages
bool publishJsonDocToTopic(const char* topic, const JsonDocument& doc, bool retained = false);
