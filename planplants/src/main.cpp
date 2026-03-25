#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

const char* WIFI_SSID = "INFINITUM7180";
const char* WIFI_PASSWORD = "4ahxH7gKth";
const char* API_URL = "http://192.168.1.74:8080/echo";

struct MoistureReading {
  const char* sensorLabel;
  int value;
  unsigned long timestamp;
};

String createPayload(const MoistureReading& reading) {
  return "{\"sensorLabel\":\"" + String(reading.sensorLabel) +
         "\",\"value\":" + String(reading.value) +
         ",\"timestamp\":" + String(reading.timestamp) + "}";
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

void sendReading(const MoistureReading& reading) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  HTTPClient http;
  String payload = createPayload(reading);

  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");

  int responseCode = http.POST(payload);

  http.end();
}

void setup() {
  Serial.begin(115200);
  connectToWifi();
}

void loop() {
  int value = analogRead(34);

  MoistureReading reading = {
    "soil_sensor_1",
    value,
    millis()
  };

  Serial.println(createPayload(reading));
  sendReading(reading);

  delay(3000);
}
