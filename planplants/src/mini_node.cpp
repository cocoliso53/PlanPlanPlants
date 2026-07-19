#include <Arduino.h>
#include <BH1750.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_wifi.h>

#include "espnow_common.h"

namespace {

constexpr uint8_t BROADCAST_ADDRESS[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr uint32_t NODE_ID = 1;
RTC_DATA_ATTR uint32_t readingCount = 0;
BH1750 luxSensor;
bool luxSensorReady = false;
volatile bool readyReceived = false;
ReadyPacket lastReady = {};

void setWifiChannel(uint8_t channel) {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}

void printWakeReason() {
  esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();

  Serial.print("Wake reason: ");

  if (wakeCause == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("timer");
  } else if (wakeCause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.println("power on reset");
  } else {
    Serial.println(static_cast<int>(wakeCause));
  }
}

void onDataSent(const uint8_t* macAddress, esp_now_send_status_t status) {
  Serial.print("Send status to ");

  for (int i = 0; i < 6; i++) {
    if (macAddress[i] < 16) {
      Serial.print("0");
    }

    Serial.print(macAddress[i], HEX);

    if (i < 5) {
      Serial.print(":");
    }
  }

  Serial.print(" -> ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "success" : "failed");
}

void onDataReceived(const uint8_t* macAddress, const uint8_t* incomingData, int length) {
  if (length != sizeof(ReadyPacket)) {
    Serial.print("Ignored response with unexpected size: ");
    Serial.println(length);
    return;
  }

  ReadyPacket ready;
  memcpy(&ready, incomingData, sizeof(ready));

  if (ready.type != PACKET_TYPE_READY) {
    Serial.print("Ignored response with unexpected type: ");
    Serial.println(ready.type);
    return;
  }

  lastReady = ready;
  readyReceived = true;

  Serial.print("Ready received from ");
  for (int i = 0; i < 6; i++) {
    if (macAddress[i] < 16) {
      Serial.print("0");
    }
    Serial.print(macAddress[i], HEX);
    if (i < 5) {
      Serial.print(":");
    }
  }
  Serial.println();
  Serial.print("Ready nodeId: ");
  Serial.println(ready.nodeId);
  Serial.print("secondsUntilWifi: ");
  Serial.println(ready.secondsUntilWifi);
}

bool addBroadcastPeer() {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_ADDRESS, sizeof(BROADCAST_ADDRESS));
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    return true;
  }

  Serial.println("Failed to add broadcast peer");
  return false;
}

void initializeSensors() {
  analogSetPinAttenuation(MOISTURE_PIN, ADC_11db);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
  Wire.begin(SDA_PIN, SCL_PIN);
  luxSensorReady = luxSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, LUX_SENSOR_ADDRESS, &Wire);

  Serial.print("Moisture pin: ");
  Serial.println(MOISTURE_PIN);
  Serial.print("Battery pin: ");
  Serial.println(BATTERY_PIN);
  Serial.print("I2C SDA pin: ");
  Serial.println(SDA_PIN);
  Serial.print("I2C SCL pin: ");
  Serial.println(SCL_PIN);
  Serial.print("BH1750 address: 0x");
  Serial.println(LUX_SENSOR_ADDRESS, HEX);
  Serial.println(luxSensorReady ? "BH1750 ready" : "BH1750 init failed");
}

SensorReading takeSensorReading(uint8_t readingIndex) {
  uint16_t moistureValue = analogRead(MOISTURE_PIN);
  float luxValue = luxSensorReady ? luxSensor.readLightLevel() : -1.0f;
  uint16_t batteryRawValue = analogRead(BATTERY_PIN);

  Serial.println("--- Reading sensors ---");
  Serial.print("readingIndex: ");
  Serial.println(readingIndex);
  Serial.print("moistureValue: ");
  Serial.println(moistureValue);
  Serial.print("luxValue: ");
  Serial.println(luxValue);
  Serial.print("batteryRawValue: ");
  Serial.println(batteryRawValue);

  return {
    moistureValue,
    luxValue,
    batteryRawValue
  };
}

PlantReadingBatchPacket takeReadingBatch() {
  readingCount++;

  PlantReadingBatchPacket batch = {
    PACKET_TYPE_READING_BATCH,
    NODE_ID,
    READINGS_PER_BATCH,
    {}
  };

  Serial.println("--- Reading batch ---");
  Serial.print("nodeId: ");
  Serial.println(NODE_ID);

  for (uint8_t i = 0; i < READINGS_PER_BATCH; i++) {
    batch.readings[i] = takeSensorReading(i);

    if (i + 1 < READINGS_PER_BATCH) {
      delay(BATCH_READING_INTERVAL_MILLISECONDS);
    }
  }

  return batch;
}

bool waitForReady() {
  unsigned long startedAt = millis();

  while (millis() - startedAt < HANDSHAKE_WAIT_MILLISECONDS) {
    if (readyReceived) {
      readyReceived = false;

      if (lastReady.nodeId != NODE_ID) {
        Serial.println("Ready response nodeId did not match");
        return false;
      }

      if (lastReady.secondsUntilWifi < MIN_SECONDS_BEFORE_WIFI_SEND) {
        Serial.println("Skipping readings because WiFi window is too close");
        return false;
      }

      return true;
    }

    delay(10);
  }

  return false;
}

bool pingMainNode() {
  HelloPacket hello = {
    PACKET_TYPE_HELLO,
    NODE_ID
  };

  readyReceived = false;
  esp_err_t result = esp_now_send(BROADCAST_ADDRESS, reinterpret_cast<const uint8_t*>(&hello), sizeof(hello));

  Serial.print("Hello send result: ");
  Serial.println(result == ESP_OK ? "queued" : "failed");

  if (result != ESP_OK) {
    return false;
  }

  if (waitForReady()) {
    Serial.println("Main node is listening");
    return true;
  }

  Serial.println("Main node did not respond");
  return false;
}

void sendReadingBatch(const PlantReadingBatchPacket& batch) {
  esp_err_t result = esp_now_send(BROADCAST_ADDRESS, reinterpret_cast<const uint8_t*>(&batch), sizeof(batch));

  Serial.print("Reading batch send result: ");
  Serial.println(result == ESP_OK ? "queued" : "failed");
}

void enterDeepSleep() {
  Serial.print("Going to deep sleep for ");
  Serial.print(DEEP_SLEEP_SECONDS);
  Serial.println(" seconds");
  Serial.flush();

  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(DEEP_SLEEP_SECONDS) * 1000000ULL);
  esp_deep_sleep_start();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  printWakeReason();
  initializeSensors();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  setWifiChannel(ESPNOW_CHANNEL);

  Serial.println("ESP-NOW mini node sender");
  Serial.print("Sender MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("WiFi channel: ");
  Serial.println(ESPNOW_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    enterDeepSleep();
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceived);

  if (!addBroadcastPeer()) {
    enterDeepSleep();
  }

  Serial.println("Sender ready");

  PlantReadingBatchPacket batch = takeReadingBatch();

  if (pingMainNode()) {
    sendReadingBatch(batch);
    delay(200);
  } else {
    Serial.println("Discarding unsent reading batch");
  }

  enterDeepSleep();
}

void loop() {
}
