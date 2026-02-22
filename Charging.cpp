#include "Config.h"

bool currentlyCharging = false;
int chargingBattery = -1;

static void everythingOff() {
  currentlyCharging = false;
  chargingBattery = -1;

  digitalWrite(RELAY_PIN, RELAY_OFF);
  for (int i = 0; i < NUM_CHANNELS; i++) {
    digitalWrite(MOSFET_PINS[i], MOSFET_OFF);
  }
}

void setupCharging() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  for (int i = 0; i < NUM_CHANNELS; i++) {
    pinMode(MOSFET_PINS[i], OUTPUT);
    digitalWrite(MOSFET_PINS[i], MOSFET_OFF);
  }
}

void startCharging() {
  everythingOff();

  digitalWrite(RELAY_PIN, RELAY_ON);
  currentlyCharging = true;
}

void stopCharging() {
  everythingOff();
}

void updateCharging(int minute) {
  if (!currentlyCharging) return;

  int batteryToCharge = minute % NUM_CHANNELS;
  if (batteryToCharge != chargingBattery) {
    if (chargingBattery >= 0) {
      digitalWrite(MOSFET_PINS[chargingBattery], MOSFET_OFF);
      delay(50);
    }
    digitalWrite(MOSFET_PINS[batteryToCharge], MOSFET_ON);
    chargingBattery = batteryToCharge;
  }
}

void printChargingDebugInfo(Print &client) {
  client.print("currentlyCharging: ");
  client.println(currentlyCharging);

  client.print("chargingBattery: ");
  client.println(chargingBattery);
}