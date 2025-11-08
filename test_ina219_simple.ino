#include <Arduino.h>
#include <Wire.h>

// ============================================================
// INA219 Adapted for Adafruit QT Py ESP32-S3
// I2C on GPIO41 (SDA), GPIO40 (SCL)
// INA219 Address: 0x43
// ============================================================

// I2C Configuration for STEMMA QT connector
#define SDA_PIN 41
#define SCL_PIN 40
#define INA219_ADDRESS 0x43

// INA219 Registers
#define INA219_REG_CONFIG 0x00
#define INA219_REG_SHUNTVOLTAGE 0x01
#define INA219_REG_BUSVOLTAGE 0x02
#define INA219_REG_POWER 0x03
#define INA219_REG_CURRENT 0x04
#define INA219_REG_CALIBRATION 0x05

// Configuration for 32V, 2A range
#define INA219_CONFIG_VALUE 0x399F  // 32V range, Gain 8, 12-bit resolution

// Calibration value for 0.1 ohm shunt resistor
// Calibration = 0.04096 / (Current_LSB * R_shunt)
// For 2A max: Current_LSB = 2A / 32767 = 61.035 µA
// Calibration = 0.04096 / (0.000061035 * 0.1) = 6711
#define INA219_CALIBRATION_VALUE 6711

float ina219_currentDivider_mA = 10.0;  // mA per LSB
float ina219_powerMultiplier_mW = 20.0; // mW per LSB

// ============================================================
// Helper Functions
// ============================================================

void INA219_wireWriteRegister(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(INA219_ADDRESS);
    Wire.write(reg);
    Wire.write((value >> 8) & 0xFF);
    Wire.write(value & 0xFF);
    Wire.endTransmission();
}

uint16_t INA219_wireReadRegister(uint8_t reg) {
    uint16_t value[2];
    Wire.beginTransmission(INA219_ADDRESS);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(INA219_ADDRESS, 2);
    value[0] = Wire.read();
    value[1] = Wire.read();
    value[0] = (value[0] << 8) | value[1];
    return value[0];
}

void INA219_setCalibration_32V_2A() {
    // Configure for 32V range, 2A max current
    INA219_wireWriteRegister(INA219_REG_CALIBRATION, INA219_CALIBRATION_VALUE);
    INA219_wireWriteRegister(INA219_REG_CONFIG, INA219_CONFIG_VALUE);
}

void INA219_init() {
    Serial.println("[INA219] Initializing...");
    
    // Initialize Wire on custom pins
    Wire.begin(SDA_PIN, SCL_PIN);
    Serial.print("[INA219] Wire initialized: SDA=GPIO");
    Serial.print(SDA_PIN);
    Serial.print(", SCL=GPIO");
    Serial.println(SCL_PIN);
    
    delay(500);
    
    // Check if device responds
    Serial.print("[INA219] Scanning I2C address 0x");
    Serial.print(INA219_ADDRESS, HEX);
    Serial.println("...");
    
    Wire.beginTransmission(INA219_ADDRESS);
    int error = Wire.endTransmission();
    
    if (error == 0) {
        Serial.println("[INA219] Device found!");
    } else {
        Serial.print("[INA219] ERROR: No device at address 0x");
        Serial.print(INA219_ADDRESS, HEX);
        Serial.print(" (error code: ");
        Serial.print(error);
        Serial.println(")");
        
        // Scan entire bus
        Serial.println("[INA219] Scanning entire I2C bus...");
        int found = 0;
        for (uint8_t addr = 0x08; addr < 0x78; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                Serial.print("[INA219] Found device at 0x");
                Serial.println(addr, HEX);
                found++;
            }
        }
        if (found == 0) {
            Serial.println("[INA219] No devices found on I2C bus!");
        }
        return;
    }
    
    // Configure sensor
    Serial.println("[INA219] Configuring sensor (32V, 2A range)...");
    INA219_setCalibration_32V_2A();
    delay(100);
    
    Serial.println("[INA219] Initialization complete!");
}

float INA219_getBusVoltage_V() {
    uint16_t voltage = INA219_wireReadRegister(INA219_REG_BUSVOLTAGE);
    // Bus voltage is in upper 13 bits, LSB = 4mV
    voltage >>= 3;
    return voltage * 0.004;
}

float INA219_getShuntVoltage_mV() {
    int16_t voltage = (int16_t)INA219_wireReadRegister(INA219_REG_SHUNTVOLTAGE);
    // Shunt voltage LSB = 10µV
    return voltage * 0.01;
}

float INA219_getCurrent_mA() {
    int16_t current = (int16_t)INA219_wireReadRegister(INA219_REG_CURRENT);
    // Current is in mA with the calibration we set
    return current / ina219_currentDivider_mA;
}

float INA219_getPower_mW() {
    uint16_t power = INA219_wireReadRegister(INA219_REG_POWER);
    // Power is in mW with the calibration we set
    return power * ina219_powerMultiplier_mW;
}

// ============================================================
// Arduino Setup & Loop
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n========================================");
    Serial.println("INA219 Power Monitor - QT Py ESP32-S3");
    Serial.println("Address: 0x43, Pins: GPIO40/41 (STEMMA QT)");
    Serial.println("========================================\n");
    
    INA219_init();
}

void loop() {
    Serial.print("Bus Voltage: ");
    Serial.print(INA219_getBusVoltage_V(), 3);
    Serial.print("V | Shunt Voltage: ");
    Serial.print(INA219_getShuntVoltage_mV(), 2);
    Serial.print("mV | Current: ");
    Serial.print(INA219_getCurrent_mA(), 3);
    Serial.print("A | Power: ");
    Serial.print(INA219_getPower_mW(), 2);
    Serial.println("mW");
    
    delay(1000);
}
