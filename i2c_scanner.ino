#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\nI2C Scanner on GPIO41(SDA), GPIO40(SCL)");
  Serial.println("==========================================");
  
  // Initialize Wire on custom pins
  Wire.begin(41, 40);  // SDA=41, SCL=40
  Serial.println("Wire initialized: SDA=GPIO41, SCL=GPIO40");
  Serial.println("Scanning I2C addresses 0x08 to 0x77...\n");
  
  byte error, address;
  int nDevices = 0;
  
  for(address = 0x08; address < 0x78; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("I2C device found at address: 0x");
      if (address < 0x10) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    }
    else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 0x10) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  
  if (nDevices == 0) {
    Serial.println("No I2C devices found!");
  } else {
    Serial.print("\nFound ");
    Serial.print(nDevices);
    Serial.println(" I2C device(s)");
  }
}

void loop() {
  delay(5000);
  Serial.println("Rescan...");
  
  int nDevices = 0;
  byte error, address;
  
  for(address = 0x08; address < 0x78; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("0x");
      if (address < 0x10) Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" ");
      nDevices++;
    }
  }
  
  if (nDevices > 0) Serial.println();
  else Serial.println("No devices");
}
