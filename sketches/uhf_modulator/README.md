# uhf_modulator

NodeMCU ESP8266 sketch for a 433 MHz OOK/ASK RF transmitter.
Refactored to use the `lib/shared/` modules introduced by the `sensors` sketch.

## Build

```sh
pio run -e uhf_modulator
pio run -e uhf_modulator -t upload
pio run -e uhf_modulator -t uploadfs   # uploads data/ to LittleFS
```

## Pin assignments

| Pin | GPIO | Function |
|-----|------|----------|
| D1 | 5  | RF DATA output (moved from D3 to avoid GPIO0 boot strap) |
| D2 | 4  | (optional) DS18B20 1-Wire bus |
| D3 | 0  | Force-portal button (FLASH) |
| D4 | 2  | Onboard blue LED (mirrored during TX) |
| D5 | 14 | I2C SDA (SSD1306 OLED) |
| D6 | 12 | I2C SCL (SSD1306 OLED) |

> **Note:** The legacy v1 sketch used D3/GPIO0 as the RF DATA output. That pin is
> a boot-strap pin and conflicts with the FLASH button used by WiFiManager
> portal entry — the refactor moves DATA to D1/GPIO5, which is safe.

## Features

- Stored timing **profiles** (up to 16) defining pulse/sync/zero/one widths
  for OOK protocols.
- Stored **codes** (up to 32), each referencing a profile + bit pattern +
  repeat count.
- Deterministic interrupt-disabled busy-wait waveform transmitter, mirrored
  on the onboard blue LED.
- Optional DS18B20 1-Wire temperature sensors.
- Shared WiFiManager captive portal, MQTT client (non-blocking reconnect),
  Prometheus `/metrics` endpoint, OLED status display, text console with
  ring buffer, LittleFS-backed config/profiles/codes.

## MQTT topics

```
<baseTopic>/<deviceId>/command
<baseTopic>/<deviceId>/status
<baseTopic>/<deviceId>/results
```

## MQTT commands

```json
{"command":"status"}
{"command":"ls"}
{"command":"listprofiles"}
{"command":"listcodes"}
{"command":"getprofile","id":"proto1_350us"}
{"command":"getcode","id":"giandel_on"}
{"command":"upsertprofile","profile":{...}}
{"command":"upsertcode","code":{...}}
{"command":"replaceprofiles","profiles":[...]}
{"command":"replacecodes","codes":[...]}
{"command":"deleteprofile","id":"..."}
{"command":"deletecode","id":"..."}
{"command":"tx","id":"giandel_on"}
{"command":"txraw","profileId":"proto1_350us","value":14199672,"bits":24,"repeat":10}
{"command":"scan"}            // 1-Wire rescan (if enabled)
```

`tx` and `txraw` are queued and run after the MQTT callback returns, so
the broker connection stays responsive during transmission.

## Web endpoints

| Route | Method | Purpose |
|---|---|---|
| `/` | GET | Web UI (LittleFS `index.html`) |
| `/api/status` | GET | Aggregate JSON status |
| `/api/config` | GET | Full config snapshot |
| `/api/profiles` | GET | All timing profiles |
| `/api/codes` | GET | All stored codes |
| `/api/tx` | POST | `id=<codeId>` — queue a TX |
| `/api/sensors/scan` | POST | 1-Wire bus rescan |
| `/api/console/log` | GET | Console ring buffer |
| `/api/console/command` | POST | Send a console / MQTT command |
| `/api/config/services` | POST | Update MQTT/Prom settings |
| `/api/config/time` | POST | Update timezone |
| `/api/config/display` | POST | Toggle blue LED |
| `/api/fs/list`, `/api/fs/file` | GET | LittleFS file browser |

## Persistent files (LittleFS)

- `config.json` — MQTT host/port, base topic, device id, prom port, timezone
- `profiles.json` — timing profile table
- `codes.json` — stored code table
- `sensor_names.json` — DS18B20 friendly-name mapping
- `index.html` — web UI
