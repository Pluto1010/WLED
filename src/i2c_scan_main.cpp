#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n========== I2C Scanner ==========");
  Serial.println("Initializing Wire on GPIO41(SDA), GPIO40(SCL)");
  
  Wire.begin(41, 40);
  delay(100);
  
  Serial.println("Scanning I2C addresses...\n");
  
  byte nDevices = 0;
  for(byte addr = 0x08; addr < 0x78; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();
    
    if(error == 0) {
      Serial.print("Device found at: 0x");
      if(addr < 0x10) Serial.print("0");
      Serial.println(addr, HEX);
      nDevices++;
    }
  }
  
  Serial.print("\nTotal devices found: ");
  Serial.println(nDevices);
  Serial.println("==================================\n");
}

void loop() {
  delay(10000);
}
