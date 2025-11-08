#pragma once

#include "wled.h"
#include <cstring>

class UsermodBatteryStatus : public Usermod {
private:
  static const char _name[];

  int8_t _segmentId = -1;
  uint8_t _yellowThreshold = 60;
  uint8_t _redThreshold = 30;
  uint16_t _updateIntervalMs = 1000;
  unsigned long _nextUpdate = 0;
  float _deadbandA = 0.05f;
  uint16_t _autoOffSeconds = 60;
  unsigned long _autoOffDeadline = 0;

  bool _telemetryValid = false;
  float _lastLevel = -1.0f;
  float _lastCurrent = 0.0f;
  bool _hasLevel = false;
  bool _hasCurrent = false;
  bool _segmentOn = false;
  bool _autoHidden = false;
  bool _pendingShow = false;
  bool _lastGreenState = false;
  bool _lastChargingState = false;
  bool _wledWasOn = false;

  uint8_t _lastAppliedFg[4] = {0, 0, 0, 0};
  uint8_t _lastAppliedBg[4] = {0, 0, 0, 0};
  uint8_t _lastAppliedEffect = 255;
  bool _hasAppliedColors = false;
  bool _wasCharging = false;

  struct Telemetry {
    bool levelValid = false;
    bool currentValid = false;
    float level = -1.0f;
    float current = 0.0f;
  };

  bool acquireTelemetry(Telemetry &telemetry) {
    if (!requestJSONBufferLock(USERMOD_ID_BATTERY_STATUS)) {
      return false;
    }

    JsonObject info = pDoc->to<JsonObject>();
    serializeInfo(info);

    JsonObject user = info["u"];
    if (!user.isNull()) {
      JsonArray levelArr = user[F("Battery level")];
      if (!levelArr.isNull() && levelArr.size() > 0 && levelArr[0].is<float>()) {
        telemetry.level = levelArr[0].as<float>();
        telemetry.levelValid = true;
      } else if (!levelArr.isNull() && levelArr.size() > 0 && levelArr[0].is<int>()) {
        telemetry.level = static_cast<float>(levelArr[0].as<int>());
        telemetry.levelValid = true;
      }

      JsonArray currentArr = user[F("Current")];
      if (!currentArr.isNull() && currentArr.size() > 0) {
        if (currentArr[0].is<float>()) {
          telemetry.current = currentArr[0].as<float>();
          telemetry.currentValid = true;
        } else if (currentArr[0].is<int>()) {
          telemetry.current = static_cast<float>(currentArr[0].as<int>());
          telemetry.currentValid = true;
        }
      }
    }

    releaseJSONBufferLock();
    return telemetry.levelValid || telemetry.currentValid;
  }

  void chooseColor(float level, uint8_t (&rgb)[3]) {
    level = constrain(level, 0.0f, 100.0f);
    if (level < static_cast<float>(_redThreshold)) {
      rgb[0] = 255; rgb[1] = 0; rgb[2] = 0;
    } else if (level < static_cast<float>(_yellowThreshold)) {
      rgb[0] = 255; rgb[1] = 170; rgb[2] = 0;
    } else {
      rgb[0] = 0; rgb[1] = 255; rgb[2] = 0;
    }
  }

  uint8_t chooseEffect(bool charging) {
    return charging ? FX_MODE_BREATH : FX_MODE_STATIC;
  }

  void buildSegmentColors(const uint8_t base[3], uint8_t (&fg)[4], uint8_t (&bg)[4]) {
    for (uint8_t i = 0; i < 3; i++) {
      fg[i] = base[i];
      bg[i] = (static_cast<uint16_t>(base[i]) * 50u + 50u) / 100u; // rounded 50%
    }
    fg[3] = 0; // W channel stays off
    bg[3] = 0;
  }

  bool isGreenLevel(float level) const {
    return level >= static_cast<float>(_yellowThreshold);
  }

  void startAutoOffTimer(unsigned long now) {
    if (_autoOffSeconds == 0) {
      _autoOffDeadline = 0;
      return;
    }
    _autoOffDeadline = now + static_cast<unsigned long>(_autoOffSeconds) * 1000UL;
  }

  bool autoOffDue(unsigned long now) const {
    return _autoOffDeadline > 0 && static_cast<long>(now - _autoOffDeadline) >= 0;
  }

  void powerSegmentOff() {
    if (_segmentId < 0 || !_segmentOn) return;
    if (!requestJSONBufferLock(USERMOD_ID_BATTERY_STATUS)) {
      return;
    }

    JsonObject state = pDoc->to<JsonObject>();
    JsonArray segments = state.createNestedArray("seg");
    JsonObject seg0 = segments.createNestedObject();
    seg0["id"] = _segmentId;
    seg0["on"] = false;

    deserializeState(state, CALL_MODE_DIRECT_CHANGE);
    releaseJSONBufferLock();

    _segmentOn = false;
    _autoHidden = true;
  }

  void pushSegmentUpdate(uint8_t effect, const uint8_t fg[4], const uint8_t bg[4]) {
    if (_segmentId < 0) return;

    if (!requestJSONBufferLock(USERMOD_ID_BATTERY_STATUS)) {
      return;
    }

    JsonObject state = pDoc->to<JsonObject>();
    JsonArray segments = state.createNestedArray("seg");
    JsonObject seg0 = segments.createNestedObject();
    seg0["id"] = _segmentId;
    seg0["fx"] = effect;
    seg0["pal"] = 0;
    seg0["on"] = true;

    JsonArray colors = seg0.createNestedArray("col");
    JsonArray primary = colors.createNestedArray();
    for (uint8_t i = 0; i < 4; i++) primary.add(fg[i]);

    JsonArray secondary = colors.createNestedArray();
    for (uint8_t i = 0; i < 4; i++) secondary.add(bg[i]);

    JsonArray tertiary = colors.createNestedArray();
    for (uint8_t i = 0; i < 4; i++) tertiary.add(0);

    deserializeState(state, CALL_MODE_DIRECT_CHANGE);
    releaseJSONBufferLock();

    memcpy(_lastAppliedFg, fg, sizeof(_lastAppliedFg));
    memcpy(_lastAppliedBg, bg, sizeof(_lastAppliedBg));
    _lastAppliedEffect = effect;
    _hasAppliedColors = true;
    _segmentOn = true;
    _autoHidden = false;
    _pendingShow = false;
  }

  bool shouldUpdateSegment(uint8_t effect, const uint8_t fg[4], const uint8_t bg[4]) const {
    if (!_segmentOn) return true;
    if (_lastAppliedEffect != effect) return true;
    if (!_hasAppliedColors) return true;
    for (uint8_t i = 0; i < 4; i++) {
      if (_lastAppliedFg[i] != fg[i]) return true;
      if (_lastAppliedBg[i] != bg[i]) return true;
    }
    return false;
  }

public:
  void setup() override {
    _nextUpdate = millis();
  }

  void loop() override {
    if (strip.isUpdating()) return;
    if (_segmentId < 0) return;
    unsigned long now = millis();
    if (now < _nextUpdate) return;
    _nextUpdate = now + _updateIntervalMs;

    Telemetry telemetry;
    if (!acquireTelemetry(telemetry)) {
      return;
    }

    if (telemetry.levelValid) {
      _lastLevel = telemetry.level;
      _hasLevel = true;
    }
    if (telemetry.currentValid) {
      _lastCurrent = telemetry.current;
      _hasCurrent = true;
    }

    _telemetryValid = _hasLevel;
    if (!_telemetryValid) return;

    bool charging = _hasCurrent ? (_lastCurrent >= _deadbandA) : _wasCharging;
    bool discharging = _hasCurrent ? (_lastCurrent <= -_deadbandA) : false;
    bool idle = !charging && !discharging;
    (void)idle;

    uint8_t rgb[3];
    chooseColor(_lastLevel, rgb);

    uint8_t fg[4];
    uint8_t bg[4];
    buildSegmentColors(rgb, fg, bg);

    uint8_t effect = chooseEffect(charging);

    bool greenLevel = isGreenLevel(_lastLevel);
    bool lowLevel = !greenLevel;
    bool shouldDisplay = true;

    // Auto-hide when battery is healthy (green) and not actively charging
    if (_autoOffSeconds == 0) {
      _autoOffDeadline = 0;
      _autoHidden = false;
    } else {
      if (_pendingShow) {
        _autoHidden = false;
        if (!charging && greenLevel) {
          startAutoOffTimer(now);
        } else {
          _autoOffDeadline = 0;
        }
      }

      if (charging || lowLevel) {
        _autoHidden = false;
        _autoOffDeadline = 0;
      } else if (!_autoHidden) {
        if (_autoOffDeadline == 0 || !_lastGreenState || _lastChargingState) {
          startAutoOffTimer(now);
        } else if (autoOffDue(now)) {
          powerSegmentOff();
        }
      }

      shouldDisplay = !_autoHidden;
    }

    if (shouldDisplay) {
      if (shouldUpdateSegment(effect, fg, bg)) {
        pushSegmentUpdate(effect, fg, bg);
      }
    } else if (_segmentOn) {
      powerSegmentOff();
    }

    _pendingShow = false;
    _lastChargingState = charging;
    _lastGreenState = greenLevel;
    _wasCharging = charging;
  }

  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray status = user.createNestedArray(F("Battery status"));
    if (_segmentId < 0) {
      status.add(F("disabled"));
      status.add(F(" segment"));
      return;
    }

    if (!_telemetryValid) {
      status.add(F("no data"));
      status.add(F(" seg"));
      return;
    }

    status.add(_lastLevel);
    status.add(_wasCharging ? F(" % charging") : F(" %"));
  }

  void addToConfig(JsonObject &root) override {
    JsonObject cfg = root.createNestedObject(FPSTR(_name));
    cfg[F("segment")] = _segmentId;
    cfg[F("yellow-threshold")] = _yellowThreshold;
    cfg[F("red-threshold")] = _redThreshold;
    cfg[F("interval-ms")] = _updateIntervalMs;
    cfg[F("auto-off-seconds")] = _autoOffSeconds;
  }

  bool readFromConfig(JsonObject &root) override {
    JsonObject cfg = root[FPSTR(_name)];
    if (cfg.isNull()) return false;

    bool configComplete = true;

    bool hasSegment = cfg.containsKey(F("segment"));
    int segment = cfg[F("segment")] | _segmentId;
    if (segment >= 0 && segment < strip.getMaxSegments()) {
      _segmentId = static_cast<int8_t>(segment);
    } else if (segment < 0) {
      _segmentId = -1;
    }
    if (!hasSegment) configComplete = false;

    bool hasYellow = cfg.containsKey(F("yellow-threshold"));
    uint8_t yellowCand = cfg[F("yellow-threshold")] | _yellowThreshold;
    bool hasRed = cfg.containsKey(F("red-threshold"));
    uint8_t redCand = cfg[F("red-threshold")] | _redThreshold;
    if (yellowCand <= 100) _yellowThreshold = yellowCand;
    if (redCand <= 100) _redThreshold = redCand;
    if (_yellowThreshold <= _redThreshold) {
      _yellowThreshold = _redThreshold < 100 ? _redThreshold + 1 : 100;
      if (_redThreshold >= _yellowThreshold) {
        _redThreshold = _yellowThreshold > 0 ? _yellowThreshold - 1 : 0;
      }
    }
    if (!hasYellow) configComplete = false;
    if (!hasRed) configComplete = false;

    bool hasInterval = cfg.containsKey(F("interval-ms"));
    uint16_t interval = cfg[F("interval-ms")] | _updateIntervalMs;
    if (interval >= 250) _updateIntervalMs = interval;
    if (!hasInterval) configComplete = false;

    bool hasAutoOff = cfg.containsKey(F("auto-off-seconds"));
    JsonVariant autoOffVar = cfg[F("auto-off-seconds")];
    if (!hasAutoOff || autoOffVar.isNull()) {
      configComplete = false;
    }

    int autoOffCand;
    if (hasAutoOff && !autoOffVar.isNull()) {
      autoOffCand = autoOffVar.is<float>() ? static_cast<int>(autoOffVar.as<float>()) : autoOffVar.as<int>();
    } else {
      autoOffCand = static_cast<int>(_autoOffSeconds);
    }
    if (autoOffCand < 0) autoOffCand = 0;
    if (autoOffCand > 300) autoOffCand = 300;
    _autoOffSeconds = static_cast<uint16_t>(autoOffCand);
    if (_autoOffSeconds == 0) {
      _autoOffDeadline = 0;
      _autoHidden = false;
      if (!_segmentOn) _pendingShow = true;
    } else {
      // force the timer to be recalculated with the new value on the next loop
      _autoOffDeadline = 0;
    }

    return configComplete;
  }

  void onStateChange(uint8_t mode) override {
    (void)mode;
    bool wledOn = bri > 0;
    if (wledOn && !_wledWasOn) {
      _pendingShow = true;
      _autoHidden = false;
    }
    _wledWasOn = wledOn;
  }

  uint16_t getId() override {
    return USERMOD_ID_BATTERY_STATUS;
  }
};

const char UsermodBatteryStatus::_name[] PROGMEM = "BatteryStatus";
