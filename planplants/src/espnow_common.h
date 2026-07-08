#pragma once

#include <Arduino.h>

constexpr uint8_t ESPNOW_CHANNEL = 1;
constexpr uint32_t DEEP_SLEEP_SECONDS = 30;
constexpr uint32_t TEST_NODE_ID = 1;
constexpr uint8_t MOISTURE_PIN = 0;
constexpr uint8_t SDA_PIN = 8;
constexpr uint8_t SCL_PIN = 9;
constexpr uint8_t LUX_SENSOR_ADDRESS = 0x23;

struct PlantReadingPacket {
  uint32_t nodeId;
  uint32_t readingCount;
  uint32_t uptimeMilliseconds;
  uint16_t moistureValue;
  float luxValue;
};
