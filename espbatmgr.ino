#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESPmDNS.h>
#include <math.h>
#include <time.h>
#include "Config.h"
#include "Prices.h"
#include "Charging.h"
#include "Discharge.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

WiFiServer infoServer(10022);

hour_t lastHandledHour = 0;
bool inCheapHour = false;
bool inExpensiveHour = false;

hour_t getCurrentHour() {
  return timeClient.getEpochTime() / 3600;
}

void waitForWiFi() {
  // Wait for the connection to establish
  Serial.println("Waiting for WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    digitalWrite(LED_PIN, LED_ON);
    delay(100);
    digitalWrite(LED_PIN, LED_OFF);
    Serial.print(".");
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void waitForNtp() {
  Serial.println("Waiting for NTP...");
  while (!timeClient.isTimeSet()) {
    timeClient.update();
    delay(50);
    digitalWrite(LED_PIN, LED_ON);
    delay(50);
    digitalWrite(LED_PIN, LED_OFF);
    Serial.print(".");
  }
}

void ensureWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost. Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    waitForWiFi();
    waitForNtp();
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);

  setupCharging();
  setupDischarge();

  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Set the ESP to be a Wi-Fi station (client)
  WiFi.begin(ssid, password);
  waitForWiFi();

  infoServer.begin();
  infoServer.setNoDelay(true);

  timeClient.begin();
  waitForNtp();

  MDNS.begin("espbatmgr");

  Serial.println("setup() finished.");
}

float measureVoltage() {
  int rawValue = -120;
  for (int i = 0; i < 10; i++) {
    rawValue += analogRead(VOLTAGE_SENSE_PIN);
    delay(1);
  }
  return rawValue * 0.00233;
}

void cheapHourStarted() {
  startCharging();
}

void cheapHourEnded() {
  stopCharging();
}

void expensiveHourStarted() {
  startDischarge();
}

void expensiveHourEnded() {
  stopDischarge();
}

void onNewHour(hour_t currentHour) {
  ensureWiFi();

  if (timeClient.getHours() == 18 || countFutureHours(currentHour) < 6) {
    fetchEnergyPrices(currentHour);
  }

  // call cheapHourStarted on cheapest hour
  if (priceIsLowest(currentHour)) {
    if (!inCheapHour) {
      inCheapHour = true;
      cheapHourStarted();
    }
  } else if (inCheapHour) {
    inCheapHour = false;
    cheapHourEnded();
  }

  // call expensiveHourStarted on most expensive hour
  if (priceIsHighest(currentHour)) {
    if (!inExpensiveHour) {
      inExpensiveHour = true;
      expensiveHourStarted();
    }
  } else if (inExpensiveHour) {
    inExpensiveHour = false;
    expensiveHourEnded();
  }
}

void handleInfoConnection(WiFiClient client) {
  client.print("timeClient.getFormattedTime(): ");
  client.println(timeClient.getFormattedTime());

  client.print("lastHandledHour: ");
  client.println(lastHandledHour);

  client.print("getCurrentHour: ");
  client.println(getCurrentHour());

  client.print("inCheapHour: ");
  client.println(inCheapHour);

  client.print("inExpensiveHour: ");
  client.println(inExpensiveHour);

  client.print("measureVoltage: ");
  client.println(measureVoltage());

  printPricesDebugInfo(client);
  printChargingDebugInfo(client);
  printDischargeDebugInfo(client);
}

void test() {
  digitalWrite(LED_PIN, LED_ON);
  testCharging();
  testDischarge();
  digitalWrite(LED_PIN, LED_OFF);
}

void loop() {
  timeClient.update();

  if (infoServer.hasClient()) {
    WiFiClient infoClient = infoServer.accept();
    handleInfoConnection(infoClient);
    infoClient.stop();
    test();
  }

  // Sanity check for correct time
  if (timeClient.getEpochTime() < 1768000000) {
    ensureWiFi();
    return;
  }

  updateCharging(timeClient.getMinutes());
  updateDischarge();

  hour_t currentHour = getCurrentHour();
  if (currentHour != lastHandledHour) {
    lastHandledHour = currentHour;
    onNewHour(currentHour);
  }

  digitalWrite(LED_PIN, 0 == ((millis() / 10) % 100));
}
