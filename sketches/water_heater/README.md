# water_heater

NodeMCU ESP8266 sketch for a hot-water-heater SSR power controller.
Refactored to use the `lib/shared/` modules introduced by the `sensors` sketch.

## Build

```sh
pio run -e water_heater
pio run -e water_heater -t upload
pio run -e water_heater -t uploadfs   # uploads data/ to LittleFS
```

## Pin assignments

| Pin | GPIO | Function |
|-----|------|----------|
| D2 | 4  | (optional) DS18B20 1-Wire bus |
| D3 | 0  | Force-portal button |
| D4 | 2  | Onboard blue LED (status indicator) |
| D5 | 14 | I2C SDA (SSD1306 OLED) |
| D6 | 12 | I2C SCL (SSD1306 OLED) |
| D7 | 13 | SSR simulation output (Bresenham 120 Hz) |

## Features

- Bresenham 120 Hz SSR modulator implemented in Timer1 ISR.
- Power calibration table (10%, 20%, … 100%) with linear interpolation.
- Optional DS18B20 1-Wire temperature sensors (compile-time
  `SHARED_LIB_USE_ONEWIRE`, enabled by default).
- Shared WiFiManager captive portal, MQTT client (non-blocking
  reconnect), Prometheus `/metrics` endpoint, OLED status display,
  text console with ring buffer, LittleFS-backed config.

## MQTT topics

```
<baseTopic>/<deviceId>/command
<baseTopic>/<deviceId>/status
<baseTopic>/<deviceId>/results
```

## MQTT commands

The handler accepts both the legacy `type` field and the standard
`command` field for compatibility:

```json
{"power_percent": 37}
{"command":"status"}
{"command":"ls"}
{"command":"calibrate","actual_power_watts": 612.0}
{"command":"purge_calibration"}
{"command":"scan"}            // 1-Wire rescan (if enabled)
```

## Web endpoints

| Route | Method | Purpose |
|---|---|---|
| `/` | GET | Web UI (LittleFS `index.html`) |
| `/api/status` | GET | Aggregate JSON status |
| `/api/config` | GET | Full config snapshot |
| `/api/heater/status` | GET | Heater-only state |
| `/api/heater/power` | POST | `percent=NN` -- set heater power |
| `/api/heater/calibrate` | POST | `actual_power_watts=NNN` |
| `/api/heater/calibrate/purge` | POST | clear calibration |
| `/api/sensors/scan` | POST | 1-Wire bus rescan |
| `/api/console/log` | GET | Console ring buffer (after=seq) |
| `/api/console/command` | POST | Send a console / MQTT command |
| `/api/config/services` | POST | Update MQTT/Prometheus settings |
| `/api/config/time` | POST | Update timezone |
| `/api/config/display` | POST | Toggle blue LED |
| `/api/fs/list`, `/api/fs/file` | GET | LittleFS file browser |

## Persistent files (LittleFS)

- `config.json` — MQTT host/port, base topic, device id, prom port, LED, timezone
- `cal.json` — power calibration table
- `sensor_names.json` — DS18B20 friendly-name mapping
- `index.html` — web UI

## Display

128×64 OLED shows: device id + SSID + spinner, MQTT/RSSI line, power
percent, power bar, modulator state and (if 1-Wire is configured) the
first sensor reading. Bottom line shows recent status messages.
