#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <math.h>
#include <time.h>
#include "wificreds.h"

const int RELAIS_PIN = D1; // green on D1, blue on GND, purple on VV
const int RELAIS_ON = LOW;
const int RELAIS_OFF = HIGH;

const int LED_PIN = D4; // built-in LED, on when LOW and off when HIGH
const int LED_ON = LOW;
const int LED_OFF = HIGH;

const int MEASUREMENT_PIN = A0; // voltage measurement

const char* anwbApiUrl = "https://api.anwb.nl/energy/energy-services/v2/tarieven/electricity?interval=HOUR";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

WiFiServer infoServer(10022);

typedef uint hour_t;
typedef int price_t;
#define INVALID_PRICE INT_MIN

typedef struct hourprice {
  price_t price;
  hour_t hour;
} hourprice_t;

hour_t lastHandledHour = 0;
price_t averagePrice = 25;

const int MAX_PRICES = 50;
hourprice_t hourlyPrices[MAX_PRICES];

int months_lengths[2][12] = {
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

hour_t parseIsoDateTime(String datetime) {
  // 2025-12-13T23:00:00.000Z
  int year = datetime.substring(0, 4).toInt();
  int month = datetime.substring(5, 7).toInt();
  int day = datetime.substring(8, 10).toInt();
  int hour = datetime.substring(11, 13).toInt();

  bool is_leap_year = year % 4 == 0;
  int days = 365 * (year - 1970) + (year - 1969) / 4;
  for (int m = 0; m < month - 1; m++) {
    days += months_lengths[is_leap_year][m];
  }
  days += day - 1;
  return days * 24 + hour;
}

hour_t getCurrentHour() {
  return timeClient.getEpochTime() / 3600;
}

price_t getCurrentPrice() {
  hour_t currentHour = getCurrentHour();
  for (int i = 0; i < MAX_PRICES; i++) {
    if (hourlyPrices[i].hour == currentHour) {
      return hourlyPrices[i].price;
    }
  }
  return INVALID_PRICE;
}

price_t getLowestPrice() {
  price_t min = INT_MAX;
  hour_t currentHour = getCurrentHour();
  for (int i = 0; i < MAX_PRICES; i++) {
    if (hourlyPrices[i].hour != 0 && hourlyPrices[i].price < min) {
      min = hourlyPrices[i].price;
    }
  }
  return min;
}

price_t getHighestPrice() {
  price_t max = INT_MIN;
  hour_t currentHour = getCurrentHour();
  for (int i = 0; i < MAX_PRICES; i++) {
    if (hourlyPrices[i].hour != 0 && hourlyPrices[i].price > max) {
      max = hourlyPrices[i].price;
    }
  }
  return max;
}

String formatTimeForApi(time_t epochTime) {
  struct tm *ptm = gmtime(&epochTime);
  
  char buffer[25];
  strftime(buffer, 25, "%Y-%m-%dT%H:%M:00.000Z", ptm);
  return String(buffer);
}

int countFutureHours() {
  int hourCount = 0;
  hour_t currentHour = getCurrentHour();
  for (int i = 0; i < MAX_PRICES; i++) {
    if (hourlyPrices[i].hour > currentHour) {
      hourCount++;
    }
  }
  return hourCount;
}

void fetchEnergyPrices() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!timeClient.isTimeSet()) return;

  // 1. Determine current UTC time to calculate offsets
  time_t nowUtc = timeClient.getEpochTime();
  
  // Clean the current hour (e.g., 14:25 becomes 14:00) to align with array index 0
  time_t startOfCurrentHourUtc = (nowUtc / 3600) * 3600;

  // 2. Prepare API Request (Fetching 48 hours of data)
  String startDate = formatTimeForApi(startOfCurrentHourUtc);
  String endDate = formatTimeForApi(startOfCurrentHourUtc + (48 * 3600));
  String fullUrl = String(anwbApiUrl) + "&startDate=" + startDate + "&endDate=" + endDate;
  Serial.println(fullUrl);

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  http.begin(client, fullUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getString());

    if (!error) {
      // Initialize array before filling
      for (int i = 0; i < MAX_PRICES; i++) {
        hourlyPrices[i].price = INVALID_PRICE;
        hourlyPrices[i].hour = 0;
      }

      int index = 0;
      JsonArray dataArray = doc["data"].as<JsonArray>();
      for (JsonObject dataPoint : dataArray) {
        hourlyPrices[index].hour = parseIsoDateTime(dataPoint["date"]);
        hourlyPrices[index].price = dataPoint["values"]["allInPrijs"].as<price_t>();

        Serial.print("EUR");
        Serial.print(hourlyPrices[index].price);
        Serial.print(" for hour ");
        Serial.println(hourlyPrices[index].hour);

        index += 1;
        if (index >= MAX_PRICES) break;
      }
      Serial.println("Price array updated for the next 48 hours.");
    }
  }
  http.end();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  pinMode(RELAIS_PIN, OUTPUT);
  digitalWrite(RELAIS_PIN, RELAIS_OFF);

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

  // fetchEnergyPrices();
}

float measureVoltage() {
  int rawValue = -120;
  for (int i = 0; i < 10; i++) {
    rawValue += analogRead(MEASUREMENT_PIN);
    delay(1);
  }
  return rawValue * 0.00233;
}

void cheapHourStarted() {
  // digitalWrite(RELAIS_PIN, RELAIS_ON);
  // float voltage = measureVoltage();
  // digitalWrite(RELAIS_PIN, RELAIS_OFF);
}

void expensiveHourStarted() {
  // discharge batteries
}

void onNewHour(hour_t currentHour) {
  // call fetchEnergyPrices if necessary
  if (timeClient.getHours() == 18 || countFutureHours() < 6) {
    fetchEnergyPrices();
  }

  price_t currentPrice = getCurrentPrice();
  
  // call cheapHourStarted on cheapest hour
  if (currentPrice < averagePrice && currentPrice == getLowestPrice()) {
    cheapHourStarted();
  }

  // call expensiveHourStarted on most expensive hour
  if (currentPrice > averagePrice && currentPrice == getHighestPrice()) {
    expensiveHourStarted();
  }
}

void handleInfoConnection(WiFiClient client) {
  client.print("timeClient.getFormattedTime(): ");
  client.println(timeClient.getFormattedTime());

  client.print("lastHandledHour: ");
  client.println(lastHandledHour);

  client.print("getCurrentHour: ");
  client.println(getCurrentHour());

  client.print("getCurrentPrice: ");
  client.println(getCurrentPrice());

  client.print("getLowestPrice: ");
  client.println(getLowestPrice());

  client.print("getHighestPrice: ");
  client.println(getHighestPrice());

  client.print("MAX_PRICES: ");
  client.println(MAX_PRICES);

  client.print("measureVoltage: ");
  client.println(measureVoltage());

  client.print("countFutureHours: ");
  client.println(countFutureHours());

  client.print("averagePrice: ");
  client.println(averagePrice);

  client.println("hourlyPrices: ");
  for (int i = 0; i < MAX_PRICES; i++) {
    client.print(i);
    client.print(" ");
    client.print(hourlyPrices[i].hour);
    client.print(" ");
    client.println(hourlyPrices[i].price);
  }
}

void loop() {
  timeClient.update();
  MDNS.update();

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
