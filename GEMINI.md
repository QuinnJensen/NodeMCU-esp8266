# Project: Firmware Mission Control
**Hardware Platform:** NodeMCU ESP8266 V3
**Framework:** Arduino / PlatformIO

## 1. Environment Configuration
* **Platform:** `esp8266`
* **Board:** `nodemcuv2`
* **Framework:** `arduino`
* **Toolchain Specs:** Standard GCC for Xtensa, managed via PlatformIO.

## 2. Library Dependencies
* **OneWire:** Required for DS18B20 communication.
* **DallasTemperature:** High-level control for the temperature probes.
* **ESP8266WiFi:** For connectivity features.

## 3. Hardware Pin Assignments
* **A0:** Water Probe (Analog input).
* **D0:** Service Barrel Level Probe.
* **Control Output:** SSR Relay (SPST-NO 25A) for system switching.
* **Power:** 3.3V Rail.

## 4. Persistent Memory & AI Instructions
* **Architecture:** Focus on asynchronous, non-blocking code to ensure the level probes and temperature sensors remain responsive.
* **Safety:** Implement a fail-safe to open the SSR relay if temperature readings from the DS18B20 sensors are out of bounds or if a sensor is disconnected.
* **Usage Context:** This firmware supports the monitoring project.
