#pragma once

#include <Arduino.h>

constexpr uint8_t ESPNOW_CHANNEL = 1;
constexpr uint32_t DEEP_SLEEP_SECONDS = 30;
constexpr uint32_t TEST_NODE_ID = 1;
constexpr uint8_t MOISTURE_PIN = 0;
constexpr uint8_t BATTERY_PIN = 1;
constexpr uint8_t SDA_PIN = 8;
constexpr uint8_t SCL_PIN = 9;
constexpr uint8_t LUX_SENSOR_ADDRESS = 0x23;
constexpr uint32_t HANDSHAKE_WAIT_MILLISECONDS = 750;

enum PacketType : uint8_t {
  PACKET_TYPE_HELLO = 1,
  PACKET_TYPE_READY = 2,
  PACKET_TYPE_READING = 3,
};

struct HelloPacket {
  uint8_t type;
  uint32_t nodeId;
};

struct ReadyPacket {
  uint8_t type;
  uint32_t nodeId;
};

struct PlantReadingPacket {
  uint8_t type;
  uint32_t nodeId;
  uint32_t readingCount;
  uint32_t uptimeMilliseconds;
  uint16_t moistureValue;
  float luxValue;
  uint16_t batteryRawValue;
};
