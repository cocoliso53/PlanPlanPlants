#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <time.h>

#include "espnow_common.h"

namespace {

const char* WIFI_SSID = "INFINITUM7180";
const char* WIFI_PASSWORD = "4ahxH7gKth";
const char* API_URL = "http://192.168.1.76:8080/echo";
const char* NTP_SERVER = "pool.ntp.org";
constexpr long GMT_OFFSET_SECONDS = 0;
constexpr int DAYLIGHT_OFFSET_SECONDS = 0;
constexpr unsigned long UPLOAD_INTERVAL_MILLISECONDS = 3UL * 60UL * 1000UL;

unsigned long lastUploadAt = 0;
bool espNowReady = false;

void setWifiChannel(uint8_t channel) {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}

void printMacAddress(const uint8_t* macAddress) {
  for (int i = 0; i < 6; i++) {
    if (macAddress[i] < 16) {
      Serial.print("0");
    }

    Serial.print(macAddress[i], HEX);

    if (i < 5) {
      Serial.print(":");
    }
  }
}

bool ensurePeer(const uint8_t* macAddress) {
  if (esp_now_is_peer_exist(macAddress)) {
    return true;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, macAddress, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return false;
  }

  return true;
}

String currentTimestamp() {
  time_t now;
  time(&now);

  if (now <= 0) {
    return "unsynced";
  }

  return String(static_cast<unsigned long long>(now));
}

bool syncClock() {
  Serial.println("Syncing clock with NTP");
  configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS, NTP_SERVER);

  struct tm timeInfo;
  for (int attempt = 0; attempt < 20; attempt++) {
    if (getLocalTime(&timeInfo, 500)) {
      Serial.print("Clock synced unix: ");
      Serial.println(currentTimestamp());
      return true;
    }

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Failed to sync clock");
  return false;
}

void sendReady(const uint8_t* macAddress, uint32_t nodeId) {
  if (!ensurePeer(macAddress)) {
    return;
  }

  ReadyPacket ready = {
    PACKET_TYPE_READY,
    nodeId
  };

  esp_err_t result = esp_now_send(macAddress, reinterpret_cast<const uint8_t*>(&ready), sizeof(ready));
  Serial.print("Ready send result: ");
  Serial.println(result == ESP_OK ? "queued" : "failed");
}

void onDataReceived(const uint8_t* macAddress, const uint8_t* incomingData, int length) {
  if (length < 1) {
    Serial.println("Ignored empty packet");
    return;
  }

  uint8_t packetType = incomingData[0];

  if (packetType == PACKET_TYPE_HELLO) {
    if (length != sizeof(HelloPacket)) {
      Serial.print("Ignored hello with unexpected size: ");
      Serial.println(length);
      return;
    }

    HelloPacket hello;
    memcpy(&hello, incomingData, sizeof(hello));

    Serial.println("--- Hello received ---");
    Serial.print("timestamp: ");
    Serial.println(currentTimestamp());
    Serial.print("From MAC: ");
    printMacAddress(macAddress);
    Serial.println();
    Serial.print("nodeId: ");
    Serial.println(hello.nodeId);

    sendReady(macAddress, hello.nodeId);
    return;
  }

  if (packetType == PACKET_TYPE_READING) {
    if (length != sizeof(PlantReadingPacket)) {
      Serial.print("Ignored reading with unexpected size: ");
      Serial.println(length);
      return;
    }

    PlantReadingPacket packet;
    memcpy(&packet, incomingData, sizeof(packet));

    Serial.println("--- Reading received ---");
    Serial.print("timestamp: ");
    Serial.println(currentTimestamp());
    Serial.print("From MAC: ");
    printMacAddress(macAddress);
    Serial.println();
    Serial.print("nodeId: ");
    Serial.println(packet.nodeId);
    Serial.print("readingCount: ");
    Serial.println(packet.readingCount);
    Serial.print("uptimeMilliseconds: ");
    Serial.println(packet.uptimeMilliseconds);
    Serial.print("moistureValue: ");
    Serial.println(packet.moistureValue);
    Serial.print("luxValue: ");
    Serial.println(packet.luxValue);
    Serial.print("batteryRawValue: ");
    Serial.println(packet.batteryRawValue);
    return;
  }

  Serial.print("Ignored packet with unknown type: ");
  Serial.println(packetType);
}

bool startEspNow() {
  Serial.println("Starting ESP-NOW setup");
  Serial.println("Setting WiFi STA mode");
  WiFi.mode(WIFI_STA);
  Serial.println("Disconnecting WiFi");
  WiFi.disconnect();
  delay(100);
  Serial.print("Setting ESP-NOW channel: ");
  Serial.println(ESPNOW_CHANNEL);
  setWifiChannel(ESPNOW_CHANNEL);

  Serial.println("Initializing ESP-NOW");
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return false;
  }

  Serial.println("Registering ESP-NOW receive callback");
  esp_now_register_recv_cb(onDataReceived);
  espNowReady = true;

  Serial.println("=== ESP-NOW listening mode ===");
  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("ESP-NOW channel: ");
  Serial.println(ESPNOW_CHANNEL);
  Serial.println("Receiver ready");
  return true;
}

void stopEspNow() {
  if (!espNowReady) {
    return;
  }

  esp_now_deinit();
  espNowReady = false;
  Serial.println("ESP-NOW stopped");
}

void connectToWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("=== WiFi mode ===");
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi channel: ");
  Serial.println(WiFi.channel());
}

void disconnectWifi() {
  WiFi.disconnect(true, true);
  delay(100);
  Serial.println("WiFi disconnected");
}

void sendMockRequest() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping HTTP send");
    return;
  }

  HTTPClient http;
  String payload = "{\"nodeId\":999,\"readingCount\":1,\"uptimeMilliseconds\":" + String(millis()) +
                   ",\"moistureValue\":1234,\"luxValue\":11,\"batteryRawValue\":2048}";

  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");

  Serial.print("HTTP payload: ");
  Serial.println(payload);

  int responseCode = http.POST(payload);

  Serial.print("HTTP response code: ");
  Serial.println(responseCode);

  if (responseCode < 0) {
    Serial.print("HTTP error: ");
    Serial.println(http.errorToString(responseCode));
  } else {
    String responseBody = http.getString();
    Serial.print("HTTP response body: ");
    Serial.println(responseBody);
  }

  http.end();
}

void runUploadCycle() {
  Serial.println("=== Upload cycle start ===");
  stopEspNow();
  connectToWifi();
  syncClock();
  sendMockRequest();
  disconnectWifi();
  startEspNow();
  Serial.println("=== Upload cycle end ===");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== Main node boot ===");
  Serial.print("Build: ");
  Serial.print(__DATE__);
  Serial.print(" ");
  Serial.println(__TIME__);

  if (startEspNow()) {
    lastUploadAt = millis();
  }
}

void loop() {
  if (millis() - lastUploadAt >= UPLOAD_INTERVAL_MILLISECONDS) {
    lastUploadAt = millis();
    runUploadCycle();
  }

  delay(100);
}
