#pragma once

#include <Arduino.h>

constexpr uint8_t ESPNOW_CHANNEL = 1;
constexpr uint32_t DEEP_SLEEP_SECONDS = 10UL * 60UL;
constexpr uint8_t MOISTURE_PIN = 0;
constexpr uint8_t BATTERY_PIN = 1;
constexpr uint8_t SDA_PIN = 6;
constexpr uint8_t SCL_PIN = 7;
constexpr uint8_t LUX_SENSOR_ADDRESS = 0x23;
constexpr uint32_t HANDSHAKE_WAIT_MILLISECONDS = 750;
constexpr uint32_t MIN_SECONDS_BEFORE_WIFI_SEND = 10;
constexpr uint8_t READINGS_PER_BATCH = 5;
constexpr uint32_t BATCH_READING_INTERVAL_MILLISECONDS = 1000;

enum PacketType : uint8_t {
  PACKET_TYPE_HELLO = 1,
  PACKET_TYPE_READY = 2,
  PACKET_TYPE_READING = 3,
  PACKET_TYPE_READING_BATCH = 4,
};

struct HelloPacket {
  uint8_t type;
  uint32_t nodeId;
};

struct ReadyPacket {
  uint8_t type;
  uint32_t nodeId;
  uint32_t secondsUntilWifi;
};

struct SensorReading {
  uint16_t moistureValue;
  float luxValue;
  uint16_t batteryRawValue;
};

struct PlantReadingPacket {
  uint8_t type;
  uint32_t nodeId;
  uint32_t readingCount;
  uint16_t moistureValue;
  float luxValue;
  uint16_t batteryRawValue;
};

struct PlantReadingBatchPacket {
  uint8_t type;
  uint32_t nodeId;
  uint8_t readingCount;
  SensorReading readings[READINGS_PER_BATCH];
};
