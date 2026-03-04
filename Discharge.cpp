#include "Config.h"

bool currentlyDischarging = false;

void stopDischarge() {
  digitalWrite(DISCHARGE_PIN, DISCHARGE_OFF);
  currentlyDischarging = false;
}

void setupDischarge() {
  // ledcAttach(DISCHARGE_PIN, PWM_FREQ, PWM_RESOLUTION);
  pinMode(DISCHARGE_PIN, OUTPUT);
  stopDischarge();
}

void startDischarge() {
  digitalWrite(DISCHARGE_PIN, DISCHARGE_ON);
  currentlyDischarging = true;
}

void updateDischarge() {
  // nothing to do
}

void printDischargeDebugInfo(Print &client) {
  client.print("currentlyDischarging: ");
  client.println(currentlyDischarging);
}

void testDischarge() {
  startDischarge();
  delay(1000);
  stopDischarge();
}