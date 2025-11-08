#include "INA219.h"

namespace {
uint32_t g_calibration_register = 0;
uint32_t g_current_divider_mA = 1;
float g_power_multiplier_mW = 20.0f;
uint8_t g_active_address = INA219_I2C_DEFAULT_ADDRESS;
bool g_wire_initialized = false;
bool g_charge_pin_configured = false;

constexpr uint8_t kRegConfig = 0x00;
constexpr uint8_t kRegShuntVoltage = 0x01;
constexpr uint8_t kRegBusVoltage = 0x02;
constexpr uint8_t kRegPower = 0x03;
constexpr uint8_t kRegCurrent = 0x04;
constexpr uint8_t kRegCalibration = 0x05;

constexpr uint16_t kConfigBusVoltageRange32V = 0x2000;
constexpr uint16_t kConfigGain8_320mV = 0x1800;
constexpr uint16_t kConfigBusAdc12Bit = 0x0180;
constexpr uint16_t kConfigShuntAdc12Bit32Samples = 0x0068;
constexpr uint16_t kConfigModeShuntAndBusContinuous = 0x0007;

void EnsureWireInitialized() {
  if (g_wire_initialized) {
    return;
  }
  Wire.begin(INA219_SDA_PIN, INA219_SCL_PIN);
  Wire.setClock(400000);  // Fast-mode for quicker updates
  g_wire_initialized = true;
}
}  // namespace

void INA219_wireWriteRegister(uint8_t reg, uint16_t value) {
  EnsureWireInitialized();
  Wire.beginTransmission(g_active_address);
  Wire.write(reg);
  Wire.write(static_cast<uint8_t>((value >> 8) & 0xFF));
  Wire.write(static_cast<uint8_t>(value & 0xFF));
  Wire.endTransmission();
}

uint16_t INA219_wireReadRegister(uint8_t reg) {
  EnsureWireInitialized();
  Wire.beginTransmission(g_active_address);
  Wire.write(reg);
  Wire.endTransmission(false);

  Wire.requestFrom(g_active_address, static_cast<uint8_t>(2));
  uint16_t value = static_cast<uint16_t>(Wire.read()) << 8;
  value |= static_cast<uint16_t>(Wire.read());
  return value;
}

void INA219_setCalibration_32V_2A() {
  g_calibration_register = 4096;  // 32V, 2A range with 0.1 ohm shunt
  g_current_divider_mA = 1;       // 100 uA per bit -> 1 mA/bit divider
  g_power_multiplier_mW = 20.0f;  // Datasheet recommended multiplier

  INA219_wireWriteRegister(kRegCalibration, g_calibration_register);

  const uint16_t config = kConfigBusVoltageRange32V | kConfigGain8_320mV |
                          kConfigBusAdc12Bit | kConfigShuntAdc12Bit32Samples |
                          kConfigModeShuntAndBusContinuous;
  INA219_wireWriteRegister(kRegConfig, config);
}

bool INA219_probe(uint8_t address) {
  EnsureWireInitialized();
  Wire.beginTransmission(address);
  const uint8_t error = Wire.endTransmission();
  return error == 0;
}

bool INA219_init(uint8_t address) {
  EnsureWireInitialized();

  if (INA219_CHARGE_PIN >= 0 && !g_charge_pin_configured) {
    pinMode(INA219_CHARGE_PIN, INPUT);
    g_charge_pin_configured = true;
  }

  if (!INA219_probe(address)) {
    Serial.printf("[INA219] Device not found at 0x%02X\n", address);
    return false;
  }

  g_active_address = address;
  INA219_setCalibration_32V_2A();
  Serial.printf("[INA219] Initialized (32V/2A) at 0x%02X\n", g_active_address);
  return true;
}

float INA219_getShuntVoltage_mV() {
  const uint16_t raw = INA219_wireReadRegister(kRegShuntVoltage);
  return static_cast<int16_t>(raw) * 0.01f;
}

float INA219_getBusVoltage_V() {
  const uint16_t raw = INA219_wireReadRegister(kRegBusVoltage);
  return static_cast<int16_t>((raw >> 3) * 4) * 0.001f;
}

float INA219_getCurrent_mA() {
  INA219_wireWriteRegister(kRegCalibration, g_calibration_register);
  const uint16_t raw = INA219_wireReadRegister(kRegCurrent);
  return static_cast<int16_t>(raw) / static_cast<float>(g_current_divider_mA);
}

float INA219_getPower_mW() {
  INA219_wireWriteRegister(kRegCalibration, g_calibration_register);
  const uint16_t raw = INA219_wireReadRegister(kRegPower);
  return static_cast<int16_t>(raw) * g_power_multiplier_mW;
}

void INA219_powerSave(bool on) {
  const uint16_t current = INA219_wireReadRegister(kRegConfig);
  const uint16_t updated = on ? (current & ~kConfigModeShuntAndBusContinuous)
                              : (current | kConfigModeShuntAndBusContinuous);
  INA219_wireWriteRegister(kRegConfig, updated);
}

void INA219_printChargeStatus() {
  if (INA219_CHARGE_PIN < 0) {
    static bool warned = false;
    if (!warned) {
      Serial.println("[INA219] Charge pin not configured");
      warned = true;
    }
    return;
  }
  if (digitalRead(INA219_CHARGE_PIN) == LOW) {
    Serial.println("[INA219] Charging");
  } else {
    Serial.println("[INA219] Discharging");
  }
}

uint8_t INA219_getActiveAddress() {
  return g_active_address;
}
