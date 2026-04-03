#include <Arduino.h>
#include <BH1750.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>

const char* WIFI_SSID = "INFINITUM7180";
const char* WIFI_PASSWORD = "4ahxH7gKth";
const char* API_URL = "http://192.168.1.74:8080/echo?source=arduino&mode=test";

constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t DHT_TYPE = DHT11;
constexpr uint8_t MOISTURE_PIN = 34;
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;

struct TestingLogs {
  int moist1;
  int temp;
  int humidity;
  int lux;
  uint64_t timestamp;
};

DHT dht(DHT_PIN, DHT_TYPE);
BH1750 lightMeter;
bool lightSensorReady = false;
unsigned long readCount = 0;

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

void connectToWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
}

String createPayload(const TestingLogs& logs) {
  return "{\"moist1\":" + String(logs.moist1) +
         ",\"temp\":" + String(logs.temp) +
         ",\"humidity\":" + String(logs.humidity) +
         ",\"lux\":" + String(logs.lux) +
         ",\"timestamp\":" + String((unsigned long long) logs.timestamp) + "}";
}

void sendLogs(const TestingLogs& logs) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  HTTPClient http;
  String payload = createPayload(logs);

  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");

  int responseCode = http.POST(payload);

  Serial.print("Sent payload: ");
  Serial.println(payload);
  Serial.print("HTTP response code: ");
  Serial.println(responseCode);

  if (responseCode > 0) {
    Serial.print("Response body: ");
    Serial.println(http.getString());
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  connectToWifi();

  Serial.println("Multi-sensor debug test");
  Serial.print("DHT11 pin: ");
  Serial.println(DHT_PIN);
  Serial.print("Moisture pin: ");
  Serial.println(MOISTURE_PIN);
  Serial.print("SDA pin: ");
  Serial.println(SDA_PIN);
  Serial.print("SCL pin: ");
  Serial.println(SCL_PIN);

  scanI2CBus();

  lightSensorReady = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);

  if (lightSensorReady) {
    Serial.println("BH1750 ready at address 0x23");
  } else {
    lightSensorReady = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C, &Wire);

    if (lightSensorReady) {
      Serial.println("BH1750 ready at address 0x5C");
    } else {
      Serial.println("BH1750 init failed. Check wiring, power, and I2C address.");
    }
  }
}

void loop() {
  readCount++;

  Serial.println("---");
  Serial.print("Read #");
  Serial.println(readCount);

  float humidity = dht.readHumidity();
  float temperatureC = dht.readTemperature();
  int moistureValue = analogRead(MOISTURE_PIN);
  float luxValue = -1;
  bool dhtOk = !isnan(humidity) && !isnan(temperatureC);

  if (!dhtOk) {
    Serial.println("DHT11 read failed");
  } else {
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" C");
  }

  Serial.print("Moisture: ");
  Serial.println(moistureValue);

  if (!lightSensorReady) {
    Serial.println("BH1750 not ready");
  } else {
    luxValue = lightMeter.readLightLevel();

    if (luxValue < 0) {
      Serial.println("BH1750 read failed");
    } else {
      Serial.print("Light: ");
      Serial.print(luxValue);
      Serial.println(" lx");
    }
  }

  if (dhtOk && luxValue >= 0) {
    TestingLogs logs = {
      moistureValue,
      static_cast<int>(temperatureC),
      static_cast<int>(humidity),
      static_cast<int>(luxValue),
      static_cast<uint64_t>(millis())
    };

    sendLogs(logs);
  } else {
    Serial.println("Skipping POST because one or more sensor reads failed");
  }

  delay(3000);
}
