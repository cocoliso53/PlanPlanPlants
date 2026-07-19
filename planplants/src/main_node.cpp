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
constexpr uint8_t MAX_BUFFERED_BATCHES = 8;

struct BufferedBatch {
  uint32_t timestamp;
  PlantReadingBatchPacket batch;
};

unsigned long lastUploadAt = 0;
bool espNowReady = false;
BufferedBatch bufferedBatches[MAX_BUFFERED_BATCHES];
uint8_t bufferedBatchCount = 0;
uint32_t droppedBatchCount = 0;

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

uint32_t currentTimestamp() {
  time_t now;
  time(&now);

  if (now <= 0) {
    return 0;
  }

  return static_cast<uint32_t>(now);
}

void printTimestamp(uint32_t timestamp) {
  if (timestamp == 0) {
    Serial.println("unsynced");
    return;
  }

  Serial.println(timestamp);
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

uint32_t secondsUntilNextWifi() {
  unsigned long elapsed = millis() - lastUploadAt;

  if (elapsed >= UPLOAD_INTERVAL_MILLISECONDS) {
    return 0;
  }

  unsigned long remaining = UPLOAD_INTERVAL_MILLISECONDS - elapsed;
  return (remaining + 999) / 1000;
}

void sendReady(const uint8_t* macAddress, uint32_t nodeId) {
  if (!ensurePeer(macAddress)) {
    return;
  }

  uint32_t secondsUntilWifi = secondsUntilNextWifi();
  ReadyPacket ready = {
    PACKET_TYPE_READY,
    nodeId,
    secondsUntilWifi
  };

  Serial.print("secondsUntilWifi: ");
  Serial.println(secondsUntilWifi);

  esp_err_t result = esp_now_send(macAddress, reinterpret_cast<const uint8_t*>(&ready), sizeof(ready));
  Serial.print("Ready send result: ");
  Serial.println(result == ESP_OK ? "queued" : "failed");
}

void printBatch(const PlantReadingBatchPacket& batch, uint32_t timestamp) {
  Serial.println("--- Reading batch received ---");
  Serial.print("timestamp: ");
  printTimestamp(timestamp);
  Serial.print("nodeId: ");
  Serial.println(batch.nodeId);
  Serial.print("readingCount: ");
  Serial.println(batch.readingCount);

  uint8_t count = batch.readingCount;
  if (count > READINGS_PER_BATCH) {
    count = READINGS_PER_BATCH;
  }

  for (uint8_t i = 0; i < count; i++) {
    Serial.print("readingIndex: ");
    Serial.println(i);
    Serial.print("moistureValue: ");
    Serial.println(batch.readings[i].moistureValue);
    Serial.print("luxValue: ");
    Serial.println(batch.readings[i].luxValue);
    Serial.print("batteryRawValue: ");
    Serial.println(batch.readings[i].batteryRawValue);
  }
}

void bufferBatch(const PlantReadingBatchPacket& batch, uint32_t timestamp) {
  if (bufferedBatchCount >= MAX_BUFFERED_BATCHES) {
    droppedBatchCount++;
    Serial.println("Batch buffer full, dropping batch");
    Serial.print("droppedBatchCount: ");
    Serial.println(droppedBatchCount);
    return;
  }

  bufferedBatches[bufferedBatchCount] = {timestamp, batch};
  bufferedBatchCount++;

  Serial.print("Buffered batches: ");
  Serial.println(bufferedBatchCount);
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
    printTimestamp(currentTimestamp());
    Serial.print("From MAC: ");
    printMacAddress(macAddress);
    Serial.println();
    Serial.print("nodeId: ");
    Serial.println(hello.nodeId);

    sendReady(macAddress, hello.nodeId);
    return;
  }

  if (packetType == PACKET_TYPE_READING_BATCH) {
    if (length != sizeof(PlantReadingBatchPacket)) {
      Serial.print("Ignored batch with unexpected size: ");
      Serial.println(length);
      return;
    }

    PlantReadingBatchPacket batch;
    memcpy(&batch, incomingData, sizeof(batch));
    uint32_t timestamp = currentTimestamp();

    Serial.print("From MAC: ");
    printMacAddress(macAddress);
    Serial.println();
    printBatch(batch, timestamp);
    bufferBatch(batch, timestamp);
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

String batchToJsonObject(const BufferedBatch& item) {
  const PlantReadingBatchPacket& batch = item.batch;
  uint8_t count = batch.readingCount;
  if (count > READINGS_PER_BATCH) {
    count = READINGS_PER_BATCH;
  }

  String json = "{\"nodeId\":" + String(batch.nodeId) +
                ",\"timestamp\":" + String(item.timestamp) +
                ",\"readings\":[";

  for (uint8_t i = 0; i < count; i++) {
    if (i > 0) {
      json += ",";
    }

    json += "{\"moisture\":" + String(batch.readings[i].moistureValue) +
            ",\"lux\":" + String(batch.readings[i].luxValue, 2) +
            ",\"batteryRaw\":" + String(batch.readings[i].batteryRawValue) + "}";
  }

  json += "]}";
  return json;
}

String bufferedBatchesToJson() {
  String json = "{\"data\":[";

  for (uint8_t i = 0; i < bufferedBatchCount; i++) {
    if (i > 0) {
      json += ",";
    }

    json += batchToJsonObject(bufferedBatches[i]);
  }

  json += "]}";
  return json;
}

bool sendPayload(const String& payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping HTTP send");
    return false;
  }

  HTTPClient http;

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
    http.end();
    return false;
  }

  String responseBody = http.getString();
  Serial.print("HTTP response body: ");
  Serial.println(responseBody);
  http.end();
  return responseCode >= 200 && responseCode < 300;
}

bool sendBufferedBatches() {
  return sendPayload(bufferedBatchesToJson());
}

bool sendEmptyHeartbeat() {
  return sendPayload("{\"data\":[]}");
}

void uploadBufferedBatches() {
  Serial.print("Buffered batches to upload: ");
  Serial.println(bufferedBatchCount);

  if (bufferedBatchCount == 0) {
    Serial.println("No buffered batches, sending empty heartbeat");
    sendEmptyHeartbeat();
    return;
  }

  if (sendBufferedBatches()) {
    bufferedBatchCount = 0;
    Serial.println("Buffered batches uploaded and cleared");
    return;
  }

  Serial.print("Upload failed, keeping buffered batches: ");
  Serial.println(bufferedBatchCount);
}

void runUploadCycle() {
  Serial.println("=== Upload cycle start ===");
  stopEspNow();
  connectToWifi();
  syncClock();
  uploadBufferedBatches();
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

  connectToWifi();
  syncClock();
  disconnectWifi();

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
