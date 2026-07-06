#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_wifi.h>

#include "espnow_common.h"

namespace {

constexpr uint8_t BROADCAST_ADDRESS[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
RTC_DATA_ATTR uint32_t readingCount = 0;

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

void sendTestPacket() {
  readingCount++;

  PlantReadingPacket packet = {
    TEST_NODE_ID,
    readingCount,
    static_cast<uint32_t>(millis())
  };

  esp_err_t result = esp_now_send(BROADCAST_ADDRESS, reinterpret_cast<uint8_t*>(&packet), sizeof(packet));

  Serial.println("--- Sending packet ---");
  Serial.print("nodeId: ");
  Serial.println(packet.nodeId);
  Serial.print("readingCount: ");
  Serial.println(packet.readingCount);
  Serial.print("uptimeMilliseconds: ");
  Serial.println(packet.uptimeMilliseconds);
  Serial.print("esp_now_send result: ");
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

  if (!addBroadcastPeer()) {
    enterDeepSleep();
  }

  Serial.println("Sender ready");
  sendTestPacket();
  delay(200);
  enterDeepSleep();
}

void loop() {
}
