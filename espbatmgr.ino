#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <math.h>
#include <time.h>
#include "Config.h"
#include "Prices.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

WiFiServer infoServer(10022);

hour_t lastHandledHour = 0;
bool inCheapHour = false;
bool inExpensiveHour = false;

hour_t getCurrentHour() {
  return timeClient.getEpochTime() / 3600;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Set the ESP8266 to be a Wi-Fi station (client)
  WiFi.begin(ssid, password);

  // Wait for the connection to establish
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    digitalWrite(LED_PIN, LED_ON);
    delay(100);
    digitalWrite(LED_PIN, LED_OFF);
    Serial.print(".");
  }

  Serial.println("WiFi connected successfully!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  infoServer.begin();
  infoServer.setNoDelay(true);

  timeClient.begin();
  timeClient.update();

  MDNS.begin("espbatmgr");

  while (!timeClient.isTimeSet()) {
    delay(50);
    digitalWrite(LED_PIN, LED_ON);
    delay(50);
    digitalWrite(LED_PIN, LED_OFF);
    Serial.print(".");
  }
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
  // digitalWrite(RELAY_PIN, RELAY_ON);
  // float voltage = measureVoltage();
}

void cheapHourEnded() {
  // digitalWrite(RELAY_PIN, RELAY_OFF);
}

void expensiveHourStarted() {
  // discharge batteries
}

void expensiveHourEnded() {
  // stop discharging batteries
}

void onNewHour(hour_t currentHour) {
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
}

void loop() {
  timeClient.update();

  if (infoServer.hasClient()) {
    WiFiClient infoClient = infoServer.accept();
    handleInfoConnection(infoClient);
    infoClient.stop();
  }

  // Sanity check for correct time
  if (timeClient.getEpochTime() < 1768000000) {
    return;
  }

  hour_t currentHour = getCurrentHour();
  if (currentHour != lastHandledHour) {
    lastHandledHour = currentHour;
    onNewHour(currentHour);
  }

  delay(1000);
  digitalWrite(LED_PIN, LED_ON);
  delay(1000);
  digitalWrite(LED_PIN, LED_OFF);
}
