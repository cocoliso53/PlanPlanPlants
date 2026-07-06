#pragma once

#include <Arduino.h>

constexpr uint8_t ESPNOW_CHANNEL = 1;
constexpr uint32_t DEEP_SLEEP_SECONDS = 30;
constexpr uint32_t TEST_NODE_ID = 1;

struct PlantReadingPacket {
  uint32_t nodeId;
  uint32_t readingCount;
  uint32_t uptimeMilliseconds;
};
