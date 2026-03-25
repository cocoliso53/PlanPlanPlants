#include <Arduino.h>


void setup() {
  Serial.begin(115200);
}

void loop() {
  int value = analogRead(34);
  Serial.println(value);
  delay(1000);
}