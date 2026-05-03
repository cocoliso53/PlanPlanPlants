#include <Arduino.h>
#include <BH1750.h>
#include <DHT.h>
#include <esp_sleep.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>

const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
const char* API_URL = "http://192.168.1.74:8080/readings";

constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t DHT_TYPE = DHT11;
constexpr uint8_t MOISTURE_1_PIN = 34;
constexpr uint8_t MOISTURE_2_PIN = 35;
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t LUX_SENSOR_1_ADDRESS = 0x23;
constexpr uint8_t LUX_SENSOR_2_ADDRESS = 0x5C;
constexpr uint64_t SLEEP_DURATION_SECONDS = 60;

struct TestingLogs {
  int moist1;
  int moist2;
  float temp;
  float humidity;
  float lux1;
  float lux2;
  uint64_t timestamp;
};

DHT dht(DHT_PIN, DHT_TYPE);
BH1750 luxSensor1;
BH1750 luxSensor2;
bool luxSensor1Ready = false;
bool luxSensor2Ready = false;
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
         ",\"moist2\":" + String(logs.moist2) +
         ",\"temp\":" + String(logs.temp, 2) +
         ",\"humidity\":" + String(logs.humidity, 2) +
         ",\"lux1\":" + String(logs.lux1, 2) +
         ",\"lux2\":" + String(logs.lux2, 2) +
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

  if (responseCode >= 200 && responseCode < 400) {
    Serial.print("Response success");
  }

  http.end();
}

void enterDeepSleep() {
  Serial.println("Going to deep sleep for 60 seconds");
  Serial.flush();

  esp_sleep_enable_timer_wakeup(SLEEP_DURATION_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
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
  Serial.print("Moisture 1 pin: ");
  Serial.println(MOISTURE_1_PIN);
  Serial.print("Moisture 2 pin: ");
  Serial.println(MOISTURE_2_PIN);
  Serial.print("SDA pin: ");
  Serial.println(SDA_PIN);
  Serial.print("SCL pin: ");
  Serial.println(SCL_PIN);
  Serial.print("Lux sensor 1 address (ADDR -> GND): 0x");
  Serial.println(LUX_SENSOR_1_ADDRESS, HEX);
  Serial.print("Lux sensor 2 address (ADDR -> 3V3): 0x");
  Serial.println(LUX_SENSOR_2_ADDRESS, HEX);

  scanI2CBus();

  luxSensor1Ready = luxSensor1.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, LUX_SENSOR_1_ADDRESS, &Wire);
  luxSensor2Ready = luxSensor2.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, LUX_SENSOR_2_ADDRESS, &Wire);

  if (luxSensor1Ready) {
    Serial.println("BH1750 lux sensor 1 ready at address 0x23");
  } else {
    Serial.println("BH1750 lux sensor 1 init failed. Check ADDR -> GND wiring.");
  }

  if (luxSensor2Ready) {
    Serial.println("BH1750 lux sensor 2 ready at address 0x5C");
  } else {
    Serial.println("BH1750 lux sensor 2 init failed. Check ADDR -> 3V3 wiring.");
  }
}

void loop() {
  readCount++;

  Serial.println("---");
  Serial.print("Read #");
  Serial.println(readCount);

  float humidity = dht.readHumidity();
  float temperatureC = dht.readTemperature();
  int moisture1Value = analogRead(MOISTURE_1_PIN);
  int moisture2Value = analogRead(MOISTURE_2_PIN);
  float lux1Value = -1;
  float lux2Value = -1;
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

  Serial.print("Moisture 1: ");
  Serial.println(moisture1Value);
  Serial.print("Moisture 2: ");
  Serial.println(moisture2Value);

  if (!luxSensor1Ready) {
    Serial.println("BH1750 lux sensor 1 not ready");
  } else {
    lux1Value = luxSensor1.readLightLevel();

    if (lux1Value < 0) {
      Serial.println("BH1750 lux sensor 1 read failed");
    } else {
      Serial.print("Light lux sensor 1: ");
      Serial.print(lux1Value);
      Serial.println(" lx");
    }
  }

  if (!luxSensor2Ready) {
    Serial.println("BH1750 lux sensor 2 not ready");
  } else {
    lux2Value = luxSensor2.readLightLevel();

    if (lux2Value < 0) {
      Serial.println("BH1750 lux sensor 2 read failed");
    } else {
      Serial.print("Light lux sensor 2: ");
      Serial.print(lux2Value);
      Serial.println(" lx");
    }
  }

  if (dhtOk && lux1Value >= 0 && lux2Value >= 0) {
    TestingLogs logs = {
      moisture1Value,
      moisture2Value,
      temperatureC,
      humidity,
      lux1Value,
      lux2Value,
      static_cast<uint64_t>(millis())
    };

    sendLogs(logs);
  } else {
    Serial.println("Skipping POST because one or more sensor reads failed");
  }

  enterDeepSleep();
}
