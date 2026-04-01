#include <Arduino.h>
#include <BH1750.h>
#include <Wire.h>

constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;
BH1750 lightMeter;
bool sensorReady = false;
unsigned long readCount = 0;
unsigned long failedReadCount = 0;

void scanI2CBus() {
  Serial.println("Scanning I2C bus...");

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found device at 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("GY-302 / BH1750 debug test");
  Serial.print("SDA pin: ");
  Serial.println(SDA_PIN);
  Serial.print("SCL pin: ");
  Serial.println(SCL_PIN);

  scanI2CBus();

  sensorReady = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);

  if (sensorReady) {
    Serial.println("BH1750 ready at address 0x23");
    return;
  }

  sensorReady = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C, &Wire);

  if (sensorReady) {
    Serial.println("BH1750 ready at address 0x5C");
    return;
  }

  if (!sensorReady) {
    Serial.println("BH1750 init failed. Check wiring, power, and I2C address.");
    return;
  }
}

void loop() {
  readCount++;

  Serial.println("---");
  Serial.print("Read #");
  Serial.println(readCount);

  if (!sensorReady) {
    failedReadCount++;
    Serial.print("Sensor not ready. Failed reads: ");
    Serial.println(failedReadCount);
    delay(2000);
    return;
  }

  float lux = lightMeter.readLightLevel();

  if (lux < 0) {
    failedReadCount++;
    Serial.println("Failed to read lux value from BH1750");
    Serial.print("Failed reads: ");
    Serial.println(failedReadCount);
    delay(2000);
    return;
  }

  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");

  Serial.print("Failed reads so far: ");
  Serial.println(failedReadCount);

  delay(2000);
}
