#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// 1. WiFi & API Credentials
// ==========================================
#include "wificreds.h" // Ensure ssid and password are defined here
const char* ANWB_API_URL = "https://api.anwb.nl/energy/energy-services/v2/tarieven/electricity?interval=HOUR";

// ==========================================
// 2. Pin Definitions (ESP32 Classic / S3)
// ==========================================

// --- Charging Logic ---
const int RELAY_PIN       = 12; // Master relay for the charger
const int MOSFET_PINS[4]  = {13, 14, 27, 26}; // Individual battery charging gates

// --- Discharging Logic (PWM) ---
// Using pins compatible with LEDC (Hardware PWM)
const int PWM_PINS[4]     = {32, 33, 25, 18}; 

// --- Sensors & Indicators ---
const int VOLTAGE_SENSE_PIN = 34; // ADC1_CH6 (Analog Input)
const int LED_PIN    = 2;  // Built-in Blue LED on most DevKits

// ==========================================
// 3. Logic Levels
// ==========================================
const int RELAY_ON  = LOW;  // Most relay modules are active-low
const int RELAY_OFF = HIGH;

const int MOSFET_ON  = HIGH; // MOSFETs usually turn on with 3.3V
const int MOSFET_OFF = LOW;

const int LED_ON  = LOW;
const int LED_OFF = HIGH;

// NOTE: Because of the optocoupler, PWM logic is INVERTED.
// 255 (or 100%) duty cycle = Opto LED ON = DIM Pin pulled to Ground = PT4115 OFF.
// 0 (or 0%) duty cycle = Opto LED OFF = DIM Pin floating High = PT4115 ON.

// ==========================================
// 4. Energy & Voltage Constants
// ==========================================
const int MAX_PRICES = 72;
const float INITIAL_AVG_PRICE = 25.0;

// Voltage divider: 100k (R1) and 15k (R2)
// Ratio = (R1 + R2) / R2 = 115 / 15 = 7.666
const float VOLTAGE_DIVIDER_RATIO = 7.666; 
const float ADC_RESOLUTION        = 4095.0;
const float ESP32_VCC             = 3.3;

// Battery Thresholds (21V Battery System)
const float BATTERY_MAX_V   = 21.0; // Stop charging here
const float BATTERY_MIN_V   = 17.5; // Stop discharging here
const float CHARGING_RESUME = 19.5; // If below this during cheap hour, start charge

// ==========================================
// 5. PWM (LEDC) Settings
// ==========================================
const int PWM_FREQ       = 5000; // 5kHz is good for PT4115
const int PWM_RESOLUTION = 8;    // 0-255 steps

#endif