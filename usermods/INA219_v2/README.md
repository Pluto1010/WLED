# Usermod INA219

This usermod samples current, voltage, power, and shunt voltage from an INA219 sensor and exposes the values inside the WLED UI, JSON API, and optional MQTT/Home Assistant entities.

## Features

- Periodic bus voltage, shunt voltage, current, and power readings.
- MQTT publishing with optional Home Assistant discovery payloads.
- Runtime configuration for I²C address, read interval, displayed precision, and measurement profile selection.
- Automatic calibration profile selection (`32V/2A`, `32V/1A`, or `16V/400mA`) based on configured limits.

## Requirements

Add the Adafruit INA219 library to the environment that enables this usermod:

```ini
lib_deps =
  https://github.com/adafruit/Adafruit_INA219.git#2.1.0
```

## Enabling

Define `USERMOD_INA219` in your build configuration, for example:

```ini
[env:ina219_example]
extends = env:esp32dev
build_flags =
  ${common.build_flags} ${esp32.build_flags}
  -D USERMOD_INA219
lib_deps =
  ${esp32.lib_deps}
  https://github.com/adafruit/Adafruit_INA219.git#2.1.0
```

## Configuration

All settings are available from the *Usermods* tab while WLED is running:

| Field              | Description                                                                 |
|--------------------|-----------------------------------------------------------------------------|
| `Enabled`          | Turn the usermod on/off without rebuilding.                                 |
| `I2CAddress`       | INA219 I²C address (decimal). Default is 64 (`0x40`).                        |
| `CheckInterval`    | Seconds between measurements (1–600).                                       |
| `Decimals`         | Number of decimals to display/emit (0–4).                                   |
| `MaxCurrent`       | Expected maximum current in milliamps. Used to choose the calibration profile. |
| `BusVoltageRange`  | Maximum bus voltage in volts; set to 16 to prefer the 16 V calibration.     |
| `MqttPublish`      | Enable periodic MQTT messages.                                              |
| `MqttPublishAlways`| Publish every reading instead of only on change.                            |
| `MqttHomeAssistantDiscovery` | Broadcast discovery payloads for Home Assistant.                  |

### Calibration profiles

The Adafruit driver exposes three built-in calibration sets that assume a 0.1 Ω shunt resistor. The usermod automatically selects the most suitable option based on `MaxCurrent` and `BusVoltageRange`:

- `32V_2A`: default profile for up to ~2 A @ 32 V.
- `32V_1A`: higher precision for loads up to ~1 A @ 32 V.
- `16V_400mA`: highest precision, but limited to 16 V and ~0.4 A.

If your hardware requires custom calibration values, derive a new profile inside the usermod or extend the Adafruit library accordingly.

## MQTT topics

When MQTT is enabled the module publishes under the device topic:

- `current` (A)
- `voltage` (V)
- `power` (W)
- `shunt_voltage` (V)

## Debugging

Enable serial debug output (`WLED_DEBUG`) to receive initialization or runtime error messages such as a failed I²C handshake or read failures.
