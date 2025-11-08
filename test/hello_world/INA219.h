#ifndef INA219_H
#define INA219_H

#include <Arduino.h>
#include <Wire.h>

// Default I2C address used by the UPS Module Mini INA219 breakout
constexpr uint8_t INA219_I2C_DEFAULT_ADDRESS = 0x43;

// STEMMA QT (SDA1/SCL1) pins on Adafruit QT Py ESP32-S3
constexpr int INA219_SDA_PIN = 41;
constexpr int INA219_SCL_PIN = 40;

// Optional charge status input (set < 0 to disable)
constexpr int INA219_CHARGE_PIN = -1;

void INA219_wireWriteRegister(uint8_t reg, uint16_t value);
uint16_t INA219_wireReadRegister(uint8_t reg);
void INA219_setCalibration_32V_2A();
bool INA219_init(uint8_t address = INA219_I2C_DEFAULT_ADDRESS);
bool INA219_probe(uint8_t address);
float INA219_getShuntVoltage_mV();
float INA219_getBusVoltage_V();
float INA219_getCurrent_mA();
float INA219_getPower_mW();
void INA219_powerSave(bool on);
void INA219_printChargeStatus();
uint8_t INA219_getActiveAddress();

#endif  // INA219_H
