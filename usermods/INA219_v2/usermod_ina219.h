#pragma once

#include "wled.h"
#include <Wire.h>
#include <math.h>

#ifdef USERMOD_BATTERY
#include "../Battery/usermod_v2_Battery.h"

#ifndef INA219_BATTERY_TYPE
#define INA219_BATTERY_TYPE -1
#endif
#ifndef INA219_BATTERY_MIN_VOLTAGE
#define INA219_BATTERY_MIN_VOLTAGE NAN
#endif
#ifndef INA219_BATTERY_MAX_VOLTAGE
#define INA219_BATTERY_MAX_VOLTAGE NAN
#endif
#ifndef INA219_BATTERY_CALIBRATION
#define INA219_BATTERY_CALIBRATION NAN
#endif
#ifndef INA219_BATTERY_VOLTAGE_MULTIPLIER
#define INA219_BATTERY_VOLTAGE_MULTIPLIER NAN
#endif
#endif

#ifndef INA219_DEFAULT_ADDRESS
#define INA219_DEFAULT_ADDRESS 0x41
#endif
#ifndef INA219_DEFAULT_CHECK_INTERVAL
#define INA219_DEFAULT_CHECK_INTERVAL 60000
#endif

enum class INA219CalibrationProfile : uint8_t {
  Profile32V2A,
  Profile32V1A,
  Profile16V400mA
};

namespace usermod_ina219 {

class INA219SimpleDriver {
public:
  INA219SimpleDriver() = default;

  void configurePins(int sdaPin, int sclPin) {
    _sdaPin = sdaPin;
    _sclPin = sclPin;
    _wireInitialized = false;
  }

  void reset() {
    _initialized = false;
    _address = 0xFF;
  }

  bool begin(uint8_t address) {
    _address = address;
    if (!ensureWire()) return false;
    if (!probe(address)) return false;
    _initialized = true;
    return true;
  }

  bool configure(INA219CalibrationProfile profile, uint8_t busVoltageRangeVolts,
                 uint16_t maxCurrentMilliAmps) {
    if (!_initialized) return false;
    computeCalibration(profile, busVoltageRangeVolts, maxCurrentMilliAmps);
    if (!writeRegister(kRegCalibration, _calibrationValue)) return false;
    if (!writeRegister(kRegConfig, _configValue)) return false;
    return true;
  }

  bool readMeasurements(float &currentAmps, float &busVoltage, float &powerWatts,
                        float &shuntVoltage) {
    if (!_initialized) return false;

    uint16_t raw = 0;

    if (!readRegister(kRegBusVoltage, raw)) return false;
    const int16_t busRaw = static_cast<int16_t>((raw >> 3) * 4);
    busVoltage = busRaw * 0.001f;

    if (!readRegister(kRegShuntVoltage, raw)) return false;
    const float shuntMilliVolts = static_cast<int16_t>(raw) * 0.01f;
    shuntVoltage = shuntMilliVolts / 1000.0f;

    if (!writeRegister(kRegCalibration, _calibrationValue)) return false;
    if (!readRegister(kRegCurrent, raw)) return false;
    const float currentMilliAmps = static_cast<int16_t>(raw) /
                                   static_cast<float>(_currentDivider_mA);
    currentAmps = currentMilliAmps / 1000.0f;

    if (!writeRegister(kRegCalibration, _calibrationValue)) return false;
    if (!readRegister(kRegPower, raw)) return false;
    const float powerMilliWatts = static_cast<int16_t>(raw) * _powerMultiplier_mW;
    powerWatts = powerMilliWatts / 1000.0f;

    return true;
  }

  bool initialized() const {
    return _initialized;
  }

  uint8_t address() const {
    return _address;
  }

private:
  static constexpr uint8_t kRegConfig = 0x00;
  static constexpr uint8_t kRegShuntVoltage = 0x01;
  static constexpr uint8_t kRegBusVoltage = 0x02;
  static constexpr uint8_t kRegPower = 0x03;
  static constexpr uint8_t kRegCurrent = 0x04;
  static constexpr uint8_t kRegCalibration = 0x05;

  static constexpr uint16_t kConfigBusVoltageRange32V = 0x2000;
  static constexpr uint16_t kConfigBusVoltageRange16V = 0x0000;
  static constexpr uint16_t kConfigGain40mV = 0x0000;
  static constexpr uint16_t kConfigGain320mV = 0x1800;
  static constexpr uint16_t kConfigBusAdc12Bit = 0x0180;
  static constexpr uint16_t kConfigShuntAdc12Bit32Samples = 0x0068;
  static constexpr uint16_t kConfigModeShuntAndBusContinuous = 0x0007;

  bool ensureWire() {
    if (!_wireInitialized) {
      if (_sdaPin >= 0 && _sclPin >= 0) {
        Wire.begin(_sdaPin, _sclPin);
      } else {
        Wire.begin();
      }
      _wireInitialized = true;
    }
    Wire.setClock(400000);
    return true;
  }

  bool probe(uint8_t address) {
    if (!ensureWire()) return false;
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
  }

  bool writeRegister(uint8_t reg, uint16_t value) {
    if (!ensureWire()) return false;
    Wire.beginTransmission(_address);
    Wire.write(reg);
    Wire.write(static_cast<uint8_t>((value >> 8) & 0xFF));
    Wire.write(static_cast<uint8_t>(value & 0xFF));
    return Wire.endTransmission() == 0;
  }

  bool readRegister(uint8_t reg, uint16_t &value) {
    if (!ensureWire()) return false;
    Wire.beginTransmission(_address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
      return false;
    }
    if (Wire.requestFrom(static_cast<int>(_address), 2) != 2) {
      return false;
    }
    value = static_cast<uint16_t>(Wire.read()) << 8;
    value |= static_cast<uint16_t>(Wire.read());
    return true;
  }

  void computeCalibration(INA219CalibrationProfile profile,
                          uint8_t busVoltageRangeVolts,
                          uint16_t /*maxCurrentMilliAmps*/) {
    const float shuntOhms = 0.01f; // INA219 on UPS module mini uses 10 mΩ shunt
    float currentLsbA = 0.001f;     // default 1 mA per bit

    switch (profile) {
      case INA219CalibrationProfile::Profile16V400mA:
        currentLsbA = 0.00025f; // 0.25 mA per bit
        break;
      case INA219CalibrationProfile::Profile32V1A:
        currentLsbA = 0.0005f; // 0.5 mA per bit
        break;
      case INA219CalibrationProfile::Profile32V2A:
      default:
        currentLsbA = 0.001f; // 1 mA per bit
        break;
    }

    const float calibration = 0.04096f / (currentLsbA * shuntOhms);
    _calibrationValue = static_cast<uint16_t>(roundf(calibration));

    const float divider = 1.0f / (currentLsbA * 1000.0f);
    _currentDivider_mA = static_cast<uint16_t>(divider < 1.0f ? 1.0f : roundf(divider));

    _powerMultiplier_mW = 20.0f * currentLsbA * 1000.0f;

    _configValue = kConfigBusAdc12Bit | kConfigShuntAdc12Bit32Samples |
                   kConfigModeShuntAndBusContinuous;

    if (busVoltageRangeVolts <= 16) {
      _configValue |= kConfigBusVoltageRange16V;
    } else {
      _configValue |= kConfigBusVoltageRange32V;
    }

    if (profile == INA219CalibrationProfile::Profile16V400mA) {
      _configValue |= kConfigGain40mV;
    } else {
      _configValue |= kConfigGain320mV;
    }
  }

  int _sdaPin = -1;
  int _sclPin = -1;
  uint8_t _address = 0xFF;
  bool _wireInitialized = false;
  bool _initialized = false;
  uint16_t _calibrationValue = 0;
  uint16_t _configValue = 0;
  uint16_t _currentDivider_mA = 1;
  float _powerMultiplier_mW = 20.0f;
};

} // namespace usermod_ina219

class UsermodINA219 : public Usermod {
private:
  using CalibrationProfile = INA219CalibrationProfile;

  static const char _name[];

  unsigned long _lastLoopCheck = 0;
  bool _settingEnabled : 1;
  bool _mqttPublish : 1;
  bool _mqttPublishAlways : 1;
  bool _mqttHomeAssistant : 1;
  bool _initDone : 1;
  bool _inError : 1;

  uint8_t _i2cAddress;
  uint16_t _checkInterval;          // milliseconds (config stored as seconds)
  float _decimalFactor;             // 10^decimals used to truncate readings
  uint16_t _maxCurrentMilliAmps;    // expected maximum current in mA
  uint8_t _busVoltageRangeVolts;    // 16 or 32

  CalibrationProfile _calibrationProfile;

  float _lastCurrent = 0.0f;
  float _lastVoltage = 0.0f;
  float _lastPower = 0.0f;
  float _lastShuntVoltage = 0.0f;

#ifndef WLED_DISABLE_MQTT
  float _lastCurrentSent = 0.0f;
  float _lastVoltageSent = 0.0f;
  float _lastPowerSent = 0.0f;
  float _lastShuntVoltageSent = 0.0f;
#endif

  usermod_ina219::INA219SimpleDriver _driver;
  int _sdaPin = -1;
  int _sclPin = -1;
  unsigned long _lastInitAttempt = 0;

#ifdef USERMOD_BATTERY
  UsermodBattery *_batteryMod = nullptr;
  bool _batteryConfigApplied = false;
#endif

  static constexpr uint32_t INIT_RETRY_DELAY_MS = 5000;

  static float truncateDecimals(float value, float factor) {
    return roundf(value * factor) / factor;
  }

  CalibrationProfile pickCalibrationProfile() const {
    if (_busVoltageRangeVolts <= 16 && _maxCurrentMilliAmps <= 400) {
      return CalibrationProfile::Profile16V400mA;
    }
    if (_maxCurrentMilliAmps <= 1000) {
      return CalibrationProfile::Profile32V1A;
    }
    return CalibrationProfile::Profile32V2A;
  }

  bool applyCalibration() {
    _calibrationProfile = pickCalibrationProfile();
    if (!_driver.initialized()) return false;
    return _driver.configure(_calibrationProfile, _busVoltageRangeVolts, _maxCurrentMilliAmps);
  }

  void initializeINA219() {
    if (!_settingEnabled) {
      DEBUG_PRINTLN(F("INA219: disabled via settings, skipping init"));
      return;
    }

    DEBUG_PRINTLN(F("INA219: starting detection sequence"));

    static constexpr uint8_t PROBE_ATTEMPTS = 5;
    static constexpr uint16_t PROBE_DELAY_MS = 250;

    const uint8_t fallbackAddresses[] = {0x41, 0x40, 0x43};
    uint8_t candidates[1 + sizeof(fallbackAddresses)] = {};
    size_t candidateCount = 0;

    candidates[candidateCount++] = _i2cAddress;
    for (uint8_t address : fallbackAddresses) {
      bool duplicate = false;
      for (size_t i = 0; i < candidateCount; ++i) {
        if (candidates[i] == address) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) {
        candidates[candidateCount++] = address;
      }
    }

    _driver.configurePins(_sdaPin, _sclPin);

    bool detected = false;
    uint8_t detectedAddress = _i2cAddress;

    for (size_t idx = 0; idx < candidateCount && !detected; ++idx) {
      detectedAddress = candidates[idx];
      DEBUG_PRINTF_P(PSTR("INA219: candidate 0x%02X\n"), detectedAddress);

      for (uint8_t attempt = 0; attempt < PROBE_ATTEMPTS && !detected; ++attempt) {
        DEBUG_PRINTF_P(PSTR("INA219: probing 0x%02X (try %u/%u)\n"), detectedAddress, attempt + 1, PROBE_ATTEMPTS);

        if (!_driver.begin(detectedAddress)) {
          if (attempt + 1 < PROBE_ATTEMPTS) {
            delay(PROBE_DELAY_MS);
          }
          continue;
        }

        if (applyCalibration()) {
          detected = true;
          break;
        }

        _driver.reset();
        if (attempt + 1 < PROBE_ATTEMPTS) {
          delay(PROBE_DELAY_MS);
        }
      }
    }

    if (!detected) {
      DEBUG_PRINTLN(F("INA219: device not found; check wiring and address"));
      _driver.reset();
      _inError = true;
      _lastInitAttempt = millis();
      return;
    }

    _i2cAddress = detectedAddress;
    DEBUG_PRINTF_P(PSTR("INA219: detected at 0x%02X\n"), _i2cAddress);

    _inError = false;
    _lastLoopCheck = 0;
    fetchAndStoreValues();
    _lastLoopCheck = _inError ? 0 : millis();
    _lastInitAttempt = millis();
  }

  bool readSensorValues(float &currentAmps, float &busVoltage, float &powerWatts, float &shuntVoltage) {
    if (!_driver.initialized()) return false;
    return _driver.readMeasurements(currentAmps, busVoltage, powerWatts, shuntVoltage);
  }

  void fetchAndStoreValues() {
    float current, voltage, power, shunt;
    if (!readSensorValues(current, voltage, power, shunt)) {
      _inError = true;
      return;
    }

    _lastCurrent = truncateDecimals(current, _decimalFactor);
    _lastVoltage = truncateDecimals(voltage, _decimalFactor);
    _lastPower = truncateDecimals(power, _decimalFactor);
    _lastShuntVoltage = truncateDecimals(shunt, _decimalFactor);
    _inError = false;

#ifdef USERMOD_BATTERY
    ensureBatteryBridge();
    if (_batteryMod && _lastVoltage > 0.0f) {
      _batteryMod->registerExternalVoltage(_lastVoltage);
    }
#endif

    long busMilliVolts = lroundf(_lastVoltage * 1000.0f);
    long currentMilliAmps = lroundf(_lastCurrent * 1000.0f);
    long powerMilliWatts = lroundf(_lastPower * 1000.0f);
    DEBUG_PRINTF_P(PSTR("INA219: %ldmV %ldmA %ldmW @0x%02X\n"), busMilliVolts, currentMilliAmps, powerMilliWatts, _i2cAddress);

#ifndef WLED_DISABLE_MQTT
    mqttPublishIfChanged(F("current"), _lastCurrentSent, _lastCurrent, 0.01f);
    mqttPublishIfChanged(F("voltage"), _lastVoltageSent, _lastVoltage, 0.01f);
    mqttPublishIfChanged(F("power"), _lastPowerSent, _lastPower, 0.05f);
    mqttPublishIfChanged(F("shunt_voltage"), _lastShuntVoltageSent, _lastShuntVoltage, 0.01f);
#endif
  }

    ensureBatteryBridge();
  #endif
  if (unitOfMeasurement.length() != 0) doc[F("unit_of_measurement")] = unitOfMeasurement;
  if (deviceClass.length() != 0) doc[F("device_class")] = deviceClass;
#ifdef USERMOD_BATTERY
  void ensureBatteryBridge() {
    if (!_batteryMod) {
      if (Usermod *batteryUm = UsermodManager::lookup(USERMOD_ID_BATTERY)) {
        _batteryMod = static_cast<UsermodBattery *>(batteryUm);
      }
    }

    if (_batteryMod && !_batteryConfigApplied) {
      int typeOverride = INA219_BATTERY_TYPE;
      uint8_t typeValue = (typeOverride < 0) ? 0xFF : static_cast<uint8_t>(typeOverride);
      _batteryMod->configureExternalBattery(
        typeValue,
        INA219_BATTERY_MIN_VOLTAGE,
        INA219_BATTERY_MAX_VOLTAGE,
        INA219_BATTERY_CALIBRATION,
        INA219_BATTERY_VOLTAGE_MULTIPLIER
      );
      _batteryConfigApplied = true;
    }
  }
#endif

  static const __FlashStringHelper *calibrationProfileName(CalibrationProfile profile) {
    switch (profile) {
      case CalibrationProfile::Profile16V400mA:
        return F("16V_400mA");
      case CalibrationProfile::Profile32V1A:
        return F("32V_1A");
      case CalibrationProfile::Profile32V2A:
      default:
        return F("32V_2A");
    }
  }

#ifndef WLED_DISABLE_MQTT
  void mqttInitialize() {
    if (!WLED_MQTT_CONNECTED || !_mqttPublish || !_mqttHomeAssistant) return;

    char topic[128];
    snprintf_P(topic, sizeof(topic), PSTR("%s/current"), mqttDeviceTopic);
    mqttCreateHassSensor(F("Current"), topic, F("current"), F("A"));

    snprintf_P(topic, sizeof(topic), PSTR("%s/voltage"), mqttDeviceTopic);
    mqttCreateHassSensor(F("Voltage"), topic, F("voltage"), F("V"));

    snprintf_P(topic, sizeof(topic), PSTR("%s/power"), mqttDeviceTopic);
    mqttCreateHassSensor(F("Power"), topic, F("power"), F("W"));

    snprintf_P(topic, sizeof(topic), PSTR("%s/shunt_voltage"), mqttDeviceTopic);
    mqttCreateHassSensor(F("Shunt Voltage"), topic, F("voltage"), F("V"));
  }

  void mqttPublishIfChanged(const __FlashStringHelper *topic, float &lastState, float state, float minChange) {
    if (!WLED_MQTT_CONNECTED || !_mqttPublish) return;
    if (!_mqttPublishAlways && fabsf(lastState - state) <= minChange) return;

    char buffer[128];
    snprintf_P(buffer, sizeof(buffer), PSTR("%s/%s"), mqttDeviceTopic, (const char *)topic);
    mqtt->publish(buffer, 0, false, String(state).c_str());
    lastState = state;
  }

  void mqttCreateHassSensor(const String &name, const String &topic, const String &deviceClass, const String &unitOfMeasurement) {
    StaticJsonDocument<600> doc;

    doc[F("name")] = name;
    doc[F("state_topic")] = topic;
    doc[F("unique_id")] = String(mqttClientID) + name;
    if (unitOfMeasurement.length() != 0) {
      doc[F("unit_of_measurement")] = unitOfMeasurement;
    }
    if (deviceClass.length() != 0) {
      doc[F("device_class")] = deviceClass;
    }
    doc[F("expire_after")] = 1800;

    JsonObject device = doc.createNestedObject(F("device"));
    device[F("name")] = serverDescription;
    device[F("identifiers")] = String(F("wled-sensor-")) + mqttClientID;
    device[F("manufacturer")] = F(WLED_BRAND);
    device[F("model")] = F(WLED_PRODUCT_NAME);
    device[F("sw_version")] = versionString;

    String payload;
    serializeJson(doc, payload);

    String hassTopic = String(F("homeassistant/sensor/")) + mqttClientID + "/" + name + F("/config");
    mqtt->publish(hassTopic.c_str(), 0, true, payload.c_str());
  }
#endif

public:
  UsermodINA219() {
    _settingEnabled = true;
    _mqttPublish = true;
    _mqttPublishAlways = false;
    _mqttHomeAssistant = true;
    _initDone = false;
    _inError = true;

    _i2cAddress = INA219_DEFAULT_ADDRESS;
    _checkInterval = INA219_DEFAULT_CHECK_INTERVAL;
    _decimalFactor = 100.0f; // 2 decimals by default
    _maxCurrentMilliAmps = 2000;
    _busVoltageRangeVolts = 32;
    _calibrationProfile = CalibrationProfile::Profile32V2A;
  }

  ~UsermodINA219() override = default;

  void setup() override {
    if (i2c_sda >= 0) {
      _sdaPin = i2c_sda;
    } else {
#ifdef HW_PIN_SDA
      _sdaPin = HW_PIN_SDA;
#elif defined(SDA)
      _sdaPin = SDA;
#else
      _sdaPin = -1;
#endif
    }

    if (i2c_scl >= 0) {
      _sclPin = i2c_scl;
    } else {
#ifdef HW_PIN_SCL
      _sclPin = HW_PIN_SCL;
#elif defined(SCL)
      _sclPin = SCL;
#else
      _sclPin = -1;
#endif
    }

    DEBUG_PRINT(F("INA219: configured I2C pins SDA="));
    DEBUG_PRINT(_sdaPin);
    DEBUG_PRINT(F(" SCL="));
    DEBUG_PRINTLN(_sclPin);

    _driver.configurePins(_sdaPin, _sclPin);

#ifdef USERMOD_BATTERY
    ensureBatteryBridge();
#endif

    initializeINA219();
    _initDone = true;
#ifndef WLED_DISABLE_MQTT
    mqttInitialize();
#endif
  }

  void loop() override {
    if (!_settingEnabled || strip.isUpdating()) return;

    unsigned long now = millis();

    if ((!_driver.initialized() || _inError) && (now - _lastInitAttempt >= INIT_RETRY_DELAY_MS)) {
      initializeINA219();
    }

    if (!_driver.initialized() || _inError) return;

    if (now - _lastLoopCheck < _checkInterval) return;

    _lastLoopCheck = now;
    fetchAndStoreValues();
  }

#ifndef WLED_DISABLE_MQTT
  void onMqttConnect(bool) override {
    mqttInitialize();
  }
#endif

  uint16_t getId() override {
    return USERMOD_ID_INA219;
  }

  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray jsonCurrent = user.createNestedArray(F("Current"));
    JsonArray jsonVoltage = user.createNestedArray(F("Voltage"));
    JsonArray jsonPower = user.createNestedArray(F("Power"));
    JsonArray jsonShuntVoltage = user.createNestedArray(F("Shunt Voltage"));
    JsonArray jsonCalibration = user.createNestedArray(F("Calibration"));

    if (_lastLoopCheck == 0) {
      jsonCurrent.add(F("Not read yet"));
      jsonVoltage.add(F("Not read yet"));
      jsonPower.add(F("Not read yet"));
      jsonShuntVoltage.add(F("Not read yet"));
      jsonCalibration.add(calibrationProfileName(_calibrationProfile));
      return;
    }

    if (_inError) {
      jsonCurrent.add(F("An error occurred"));
      jsonVoltage.add(F("An error occurred"));
      jsonPower.add(F("An error occurred"));
      jsonShuntVoltage.add(F("An error occurred"));
      jsonCalibration.add(calibrationProfileName(_calibrationProfile));
      return;
    }

    jsonCurrent.add(_lastCurrent);
    jsonCurrent.add(F("A"));

    jsonVoltage.add(_lastVoltage);
    jsonVoltage.add(F("V"));

    jsonPower.add(_lastPower);
    jsonPower.add(F("W"));

    jsonShuntVoltage.add(_lastShuntVoltage);
    jsonShuntVoltage.add(F("V"));

    jsonCalibration.add(calibrationProfileName(_calibrationProfile));
  }

  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[F("Enabled")] = _settingEnabled;
    top[F("I2CAddress")] = _i2cAddress;
    top[F("CheckInterval")] = _checkInterval / 1000;
    top[F("Decimals")] = log10f(_decimalFactor);
    top[F("MaxCurrent")] = _maxCurrentMilliAmps;
    top[F("BusVoltageRange")] = _busVoltageRangeVolts;
#ifndef WLED_DISABLE_MQTT
    top[F("MqttPublish")] = _mqttPublish;
    top[F("MqttPublishAlways")] = _mqttPublishAlways;
    top[F("MqttHomeAssistantDiscovery")] = _mqttHomeAssistant;
#endif
    DEBUG_PRINTLN(F("INA219 config saved."));
  }

  bool readFromConfig(JsonObject &root) override {
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) return false;

    bool configComplete = true;

    bool enabled;
    if (getJsonValue(top[F("Enabled")], enabled)) {
      _settingEnabled = enabled;
    } else {
      configComplete = false;
    }
    configComplete &= getJsonValue(top[F("I2CAddress")], _i2cAddress);

    uint16_t tempInterval;
    if (getJsonValue(top[F("CheckInterval")], tempInterval)) {
      if (tempInterval >= 1 && tempInterval <= 600) {
        _checkInterval = tempInterval * 1000UL;
      } else {
        _checkInterval = INA219_DEFAULT_CHECK_INTERVAL;
      }
    } else {
      configComplete = false;
    }

    float decimals;
    if (getJsonValue(top[F("Decimals")], decimals)) {
      if (decimals >= 0.0f && decimals <= 4.0f) {
        _decimalFactor = powf(10.0f, decimals);
      } else {
        _decimalFactor = 100.0f;
      }
    } else {
      configComplete = false;
    }

    configComplete &= getJsonValue(top[F("MaxCurrent")], _maxCurrentMilliAmps);
    configComplete &= getJsonValue(top[F("BusVoltageRange")], _busVoltageRangeVolts);

#ifndef WLED_DISABLE_MQTT
    bool tmpBool;
    if (getJsonValue(top[F("MqttPublish")], tmpBool)) {
      _mqttPublish = tmpBool;
    } else {
      configComplete = false;
    }

    if (getJsonValue(top[F("MqttPublishAlways")], tmpBool)) {
      _mqttPublishAlways = tmpBool;
    } else {
      configComplete = false;
    }

    if (getJsonValue(top[F("MqttHomeAssistantDiscovery")], tmpBool)) {
      _mqttHomeAssistant = tmpBool;
    } else {
      configComplete = false;
    }
#endif

    if (_initDone) {
      initializeINA219();
#ifndef WLED_DISABLE_MQTT
      mqttInitialize();
#endif
    }

    return configComplete;
  }
};

const char UsermodINA219::_name[] PROGMEM = "INA219";
