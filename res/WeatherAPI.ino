/* -------------------- WeatherAPI.com fetch -------------------- */
void fetchWeather() {
    if (!client.connect(host, 80)) {
        Serial.println("HTTP connect failed");
        return;
    }

    String url = "/v1/current.json?key=" + apiKey + "&q=" + city + "&aqi=no";
    client.println("GET " + url + " HTTP/1.1");
    client.println("Host: " + String(host));
    client.println("Connection: close");
    client.println();

    String payload;
    while (client.connected() || client.available()) {
        if (client.available()) {
            payload += client.readStringUntil('\n');
        }
    }
    client.stop();

    // BREAKPOINT: inspect payload
    // Watch: payload

    int jsonStart = payload.indexOf('{');
    if (jsonStart < 0) {
        Serial.println("No JSON found.");
        return;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, payload.substring(jsonStart));
    if (err) {
        Serial.print("JSON parse error: ");
        Serial.println(err.c_str());
        return;
    }

    tempC = doc["current"]["temp_c"].as<float>();
    conditionText = doc["current"]["condition"]["text"].as<String>();
    weatherText = "T:" + String(tempC, 1) + "C " + conditionText + "  ";

    // TRACEPOINT: Weather updated: {tempC}C
    // Watch: tempC, conditionText, weatherText

    Serial.println(weatherText.c_str());

    
}