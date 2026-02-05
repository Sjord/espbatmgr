#ifndef PRICES_H
#define PRICES_H

#include <Arduino.h>

const int INVALID_PRICE = INT_MIN;

typedef uint hour_t;
typedef int price_t;

typedef struct hourprice {
  price_t price;
  hour_t hour;
} hourprice_t;

bool priceIsLowest(hour_t currentHour);
bool priceIsHighest(hour_t currentHour);
int countFutureHours(hour_t currentHour);
void fetchEnergyPrices(hour_t currentHour);
void printPricesDebugInfo(Print &client);

#endif