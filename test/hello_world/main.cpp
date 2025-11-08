#include <Arduino.h>

#include "INA219.h"

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kSampleDelayMs = 1000;

bool g_ina_ready = false;
uint8_t g_detected_address = INA219_I2C_DEFAULT_ADDRESS;

constexpr uint8_t kCandidateAddresses[] = {0x43, 0x41, 0x40};

void scanI2CBus() {
  Serial.println("[SCAN] I2C devices:");
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("  - 0x");
      if (address < 16) Serial.print('0');
      Serial.print(address, HEX);
      for (const uint8_t addr : kCandidateAddresses) {
        if (addr == address) {
          Serial.print("  (candidate)");
          break;
        }
      }
      Serial.println();
    }
  }
  Serial.println();
}

void setup() {
  Serial.begin(kSerialBaud);
  while (!Serial && millis() < 3000) {
    delay(10);
  }
  Serial.println();
  Serial.println("=== INA219 Validation ===");
  Serial.printf("SDA=%d SCL=%d candidate addresses: 0x43, 0x41, 0x40\n",
                INA219_SDA_PIN, INA219_SCL_PIN);

  for (const uint8_t addr : kCandidateAddresses) {
    Serial.printf("[INIT] Probing 0x%02X...\n", addr);
    if (INA219_init(addr)) {
      g_detected_address = addr;
      g_ina_ready = true;
      break;
    }
  }

  if (g_ina_ready) {
    Serial.printf("[INFO] INA219 ready at 0x%02X\n", g_detected_address);
  }

  if (!g_ina_ready) {
    Serial.println("[ERROR] INA219 not detected at 0x43/0x40/0x41. Bus scan:");
    scanI2CBus();
  }
}

void loop() {
  if (!g_ina_ready) {
    for (const uint8_t addr : kCandidateAddresses) {
      Serial.printf("[RETRY] Probing 0x%02X...\n", addr);
      if (INA219_init(addr)) {
        g_detected_address = addr;
        g_ina_ready = true;
        Serial.printf("[INFO] INA219 detected on retry at 0x%02X\n",
                      g_detected_address);
        break;
      }
    }
    if (!g_ina_ready) {
      Serial.println("[WAIT] INA219 still missing. Check wiring and power.");
      delay(2000);
      return;
    }
  }

  const float bus_voltage = INA219_getBusVoltage_V();
  const float current_mA = INA219_getCurrent_mA();
  const float power_mW = INA219_getPower_mW();
  const float current_A = current_mA / 1000.0f;
  const float power_W = power_mW / 1000.0f;
  float percent = (bus_voltage - 3.0f) / 1.2f * 100.0f;
  percent = constrain(percent, 0.0f, 100.0f);

  Serial.printf("[0x%02X] V=%.3f V | I=%.3f A | P=%.3f W | %%=%.1f\n",
                INA219_getActiveAddress(), bus_voltage, current_A, power_W,
                percent);

  INA219_printChargeStatus();
  Serial.println();

  delay(kSampleDelayMs);
}
