#pragma once

#include "wled.h"
#include <Adafruit_INA219.h>
#include <math.h>

#define INA219_DEFAULT_ADDRESS 0x40
#define INA219_DEFAULT_CHECK_INTERVAL 60000

class UsermodINA219 : public Usermod {
private:
  enum class CalibrationProfile : uint8_t {
    Profile32V2A,
    Profile32V1A,
    Profile16V400mA
  };

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

  Adafruit_INA219 *_ina219 = nullptr;

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

  void applyCalibration() {
    if (!_ina219) return;

    _calibrationProfile = pickCalibrationProfile();
    switch (_calibrationProfile) {
      case CalibrationProfile::Profile16V400mA:
        _ina219->setCalibration_16V_400mA();
        break;
      case CalibrationProfile::Profile32V1A:
        _ina219->setCalibration_32V_1A();
        break;
      case CalibrationProfile::Profile32V2A:
      default:
        _ina219->setCalibration_32V_2A();
        break;
    }
  }

  void initializeINA219() {
    if (_ina219 != nullptr) {
      delete _ina219;
      _ina219 = nullptr;
    }

    _ina219 = new Adafruit_INA219(_i2cAddress);
    if (!_ina219->begin()) {
      DEBUG_PRINTLN(F("INA219 initialization failed"));
      _inError = true;
      return;
    }

    applyCalibration();
    _inError = false;
    _lastLoopCheck = 0;
  }

  bool readSensorValues(float &currentAmps, float &busVoltage, float &powerWatts, float &shuntVoltage) {
    if (!_ina219) return false;

    busVoltage = _ina219->getBusVoltage_V();
    bool ok = _ina219->success();

    shuntVoltage = _ina219->getShuntVoltage_mV() / 1000.0f;
    ok &= _ina219->success();

    currentAmps = _ina219->getCurrent_mA() / 1000.0f;
    ok &= _ina219->success();

    powerWatts = _ina219->getPower_mW() / 1000.0f;
    ok &= _ina219->success();

    if (!ok) {
      DEBUG_PRINTLN(F("INA219 read failed"));
    }

    return ok;
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

#ifndef WLED_DISABLE_MQTT
    mqttPublishIfChanged(F("current"), _lastCurrentSent, _lastCurrent, 0.01f);
    mqttPublishIfChanged(F("voltage"), _lastVoltageSent, _lastVoltage, 0.01f);
    mqttPublishIfChanged(F("power"), _lastPowerSent, _lastPower, 0.05f);
    mqttPublishIfChanged(F("shunt_voltage"), _lastShuntVoltageSent, _lastShuntVoltage, 0.01f);
#endif
  }

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
  if (unitOfMeasurement.length() != 0) doc[F("unit_of_measurement")] = unitOfMeasurement;
  if (deviceClass.length() != 0) doc[F("device_class")] = deviceClass;
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

  ~UsermodINA219() override {
    delete _ina219;
    _ina219 = nullptr;
  }

  void setup() override {
    initializeINA219();
    _initDone = true;
#ifndef WLED_DISABLE_MQTT
    mqttInitialize();
#endif
  }

  void loop() override {
    if (!_settingEnabled || strip.isUpdating()) return;
    if (!_ina219) return;

    unsigned long now = millis();
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
