/*
  UNO R4 WiFi - Weather Dashboard (WeatherAPI.com)
  Scrolls temperature + condition on the 12x8 LED matrix.

  Included in this Solution:-
      Board Package:
        Arduino Renesas UNO R4 Boards v 1.5.1

      External Libraries Included in this Solution:
        ArduinoGraphics v1.1.4
        ArduinoJson v7.4.2

Debug Options (No Additional Hardware Needed!):-
    Serial Debugger (+Plot Charts)
    Hardware Debugger
*/

#include <ArduinoJson.hpp>
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include <WiFiS3.h>
#include <ArduinoJson.h>
#include "Secrets.h"

ArduinoLEDMatrix matrix;

const char* ssid = WIFI_SSID;
const char* pass = WIFI_PASS;

// WeatherAPI endpoint (free tier)
const char* host = "api.weatherapi.com";
String apiKey = API_KEY;
String city = "Birmingham";

// Data fields
String weatherText = "Starting... ";
float  tempC = 0.0;
String conditionText;

unsigned long lastFetch = 0;
const unsigned long FETCH_INTERVAL_MS = 5UL * 60UL * 1000UL; // 5 min

WiFiClient client;

void setup() {
    Serial.begin(115200);
    matrix.begin();

#if VM_DEBUG_GDB
    delay(5000); // Allow GDB to attach if in Debug Mode
#endif

    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(ssid, pass);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nConnected!");
    showScrolling("Wi-Fi OK ");

    fetchWeather();  // initial
}

void loop() {
    if (millis() - lastFetch >= FETCH_INTERVAL_MS) {
        fetchWeather();
    }
    else {
        // Just keep showing the last information we received repeatedly
        showScrolling(weatherText);
        lastFetch = millis();
    }
    delay(1000);
}