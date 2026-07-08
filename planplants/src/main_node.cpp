#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "espnow_common.h"

namespace {

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

void onDataReceived(const uint8_t* macAddress, const uint8_t* incomingData, int length) {
  if (length != sizeof(PlantReadingPacket)) {
    Serial.print("Ignored packet with unexpected size: ");
    Serial.println(length);
    return;
  }

  PlantReadingPacket packet;
  memcpy(&packet, incomingData, sizeof(packet));

  Serial.println("--- Packet received ---");
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
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  setWifiChannel(ESPNOW_CHANNEL);

  Serial.println("ESP-NOW main node receiver");
  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("WiFi channel: ");
  Serial.println(ESPNOW_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onDataReceived);
  Serial.println("Receiver ready");
}

void loop() {
  delay(1000);
}
