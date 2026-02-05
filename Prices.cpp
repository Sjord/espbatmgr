#include <ArduinoJson.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "Config.h"
#include "Prices.h"

price_t averagePrice = 25;
hourprice_t hourlyPrices[MAX_PRICES];

const int months_lengths[2][12] = {
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

static price_t getPrice(hour_t currentHour) {
  for (int i = 0; i < MAX_PRICES; i++) {
    if (hourlyPrices[i].hour == currentHour) {
      return hourlyPrices[i].price;
    }
  }
  return INVALID_PRICE;
}

static price_t getLowestPrice() {
  price_t min = INT_MAX;
  for (int i = 0; i < MAX_PRICES; i++) {
    if (hourlyPrices[i].hour != 0 && hourlyPrices[i].price < min) {
      min = hourlyPrices[i].price;
    }
  }
  return min;
}

static price_t getHighestPrice() {
  price_t max = INT_MIN;
  for (int i = 0; i < MAX_PRICES; i++) {
    if (hourlyPrices[i].hour != 0 && hourlyPrices[i].price > max) {
      max = hourlyPrices[i].price;
    }
  }
  return max;
}

bool priceIsLowest(hour_t currentHour) {
    price_t price = getPrice(currentHour);
    return (price < averagePrice && price == getLowestPrice());
}

bool priceIsHighest(hour_t currentHour) {
    price_t price = getPrice(currentHour);
    return (price > averagePrice && price == getHighestPrice());
}

int countFutureHours(hour_t currentHour) {
  int hourCount = 0;
  for (int i = 0; i < MAX_PRICES; i++) {
    if (hourlyPrices[i].hour > currentHour) {
      hourCount++;
    }
  }
  return hourCount;
}

static hour_t parseIsoDateTime(String datetime) {
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

static String formatTimeForApi(time_t epochTime) {
  struct tm *ptm = gmtime(&epochTime);
  
  char buffer[25];
  strftime(buffer, 25, "%Y-%m-%dT%H:%M:00.000Z", ptm);
  return String(buffer);
}

static void updateAveragePrice() {
  price_t sum = 0;
  int count = 0;
  for (int i = 0; i < MAX_PRICES; i++) {
    if (hourlyPrices[i].hour != 0 && hourlyPrices[i].price != INVALID_PRICE) {
      sum += hourlyPrices[i].price;
      count += 1;
    }
  }
  averagePrice = (10 * averagePrice + sum) / (10 + count);
}

void fetchEnergyPrices(hour_t currentHour) {
  time_t startOfCurrentHourUtc = currentHour * 3600;

  // 2. Prepare API Request (Fetching 48 hours of data)
  String startDate = formatTimeForApi(startOfCurrentHourUtc);
  String endDate = formatTimeForApi(startOfCurrentHourUtc + (48 * 3600));
  String fullUrl = String(ANWB_API_URL) + "&startDate=" + startDate + "&endDate=" + endDate;
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

        Serial.print("€0.");
        Serial.print(hourlyPrices[index].price);
        Serial.print(" for hour ");
        Serial.println(hourlyPrices[index].hour);

        index += 1;
        if (index >= MAX_PRICES) break;
      }
      Serial.print("Price array updated for the next ");
      Serial.print(index);
      Serial.println(" hours.");

      updateAveragePrice();
    }
  }
  http.end();
}

void printPricesDebugInfo(Print &client) {
  client.print("getLowestPrice: ");
  client.println(getLowestPrice());

  client.print("getHighestPrice: ");
  client.println(getHighestPrice());

  client.print("MAX_PRICES: ");
  client.println(MAX_PRICES);

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
