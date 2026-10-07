#include <ArduinoJson.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>
#include <NTPClient.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>

#include "secrets.h"

// --- Config WiFi ---
const char *ssid = MYSSID;
const char *password = MYPASSWORD;

String weathersite = "api.weatherapi.com";
String weatherapikey = WEATHER_API_KEY;
const char *city = "Bologna";

// Define NTP Client to get time
const long utcOffsetInSeconds = 3600;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 60000);

// --- Config server ---
const char *hostname = "rock-3c";
const char *fallback_ip = "192.168.0.65";
const int port = 8000;

// --- Pin pulsanti ---
const int PIN_MINUS = 2;
const int PIN_POWER = 3;
const int PIN_PLUS = 4;
const int PIN_UPDATE = 5;

// --- Variabili stato ---
bool powerState = false;
float brightness = 0.5;
float currentTemperature = 0.0;
String currentCondition = "";
bool showWeather = false;

WiFiClient client;

ArduinoLEDMatrix matrix;

// --- Variabile per monitorare la connessione ---
unsigned long lastConnectionCheck = 0;
const unsigned long CONNECTION_CHECK_INTERVAL =
	30000; // Controlla ogni 30 secondi

void checkWiFiConnection() {
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("⚠️ WiFi disconnesso! Riconnessione...");
		WiFi.disconnect();
		delay(1000);

		if (WiFi.begin(ssid, password) == WL_CONNECTED) {
			Serial.println("✅ WiFi riconnesso!");
			Serial.print("IP locale: ");
			Serial.println(WiFi.localIP());
		} else {
			Serial.println("❌ Riconnessione fallita!");
		}
	}
}

void httpPost(const char *path, const String &jsonBody) {
	// Verifica connessione WiFi prima di tentare la richiesta
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("⚠️ WiFi non connesso, tento riconnessione...");
		checkWiFiConnection();
		if (WiFi.status() != WL_CONNECTED) {
			Serial.println("❌ Impossibile procedere senza WiFi");
			return;
		}
	}

	Serial.print("Connessione a ");
	Serial.print(hostname);
	Serial.print("...");

	IPAddress serverIP;
	if (WiFi.hostByName(hostname, serverIP) != 1) {
		Serial.println("❌ DNS fallito, uso IP statico.");
		serverIP.fromString(fallback_ip);
	} else {
		Serial.print("✅ Risolto: ");
		Serial.println(serverIP);
	}

	// Chiudi eventuali connessioni precedenti rimaste aperte
	if (client.connected()) {
		client.stop();
		delay(100);
	}

	// Timeout per la connessione
	unsigned long startAttempt = millis();
	const unsigned long timeout = 5000; // 5 secondi timeout

	if (client.connect(serverIP, port)) {
		Serial.println("Connesso al server.");

		String request = "";
		request += String("POST ") + path + " HTTP/1.1\r\n";
		request += String("Host: ") + hostname + ":" + String(port) + "\r\n";
		request += "User-Agent: Arduino/1.0\r\n";
		request += "Accept: */*\r\n";
		request += "Content-Type: application/json\r\n";
		request += String("Content-Length: ") + jsonBody.length() + "\r\n";
		request += "Connection: close\r\n";
		request += "\r\n";
		request += jsonBody;

		client.print(request);
		Serial.print("→ POST ");
		Serial.print(path);
		Serial.print(" ");
		Serial.println(jsonBody);

		// Leggi risposta con timeout
		while (client.connected() && (millis() - startAttempt < timeout)) {
			while (client.available()) {
				char c = client.read();
				Serial.write(c);
			}
		}

		client.stop();
		Serial.println("\nConnessione chiusa.");
	} else {
		Serial.println("❌ Connessione fallita!");
		// Prova a riconnettersi al WiFi per il prossimo tentativo
		checkWiFiConnection();
	}
}

String httpGet(const char *path) {
	String body = "";

	// Verifica WiFi
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("⚠️ WiFi non connesso, tento riconnessione...");
		checkWiFiConnection();
		if (WiFi.status() != WL_CONNECTED) {
			Serial.println("❌ Impossibile procedere senza WiFi");
			return body;
		}
	}

	// Risoluzione DNS
	IPAddress serverIP;
	if (WiFi.hostByName(hostname, serverIP) != 1) {
		Serial.println("❌ DNS fallito, uso IP statico");
		serverIP.fromString(fallback_ip);
	}

	// Chiudi connessioni precedenti
	if (client.connected()) {
		client.stop();
		delay(50);
	}

	const unsigned long timeout = 5000;
	unsigned long startTime = millis();

	if (!client.connect(serverIP, port)) {
		Serial.println("❌ Connessione fallita");
		return body;
	}

	// Invia richiesta GET
	client.print("GET ");
	client.print(path);
	client.println(" HTTP/1.1");
	client.print("Host: ");
	client.println(hostname);
	client.println("Connection: close");
	client.println();

	// ---- Lettura risposta ----
	bool headersEnded = false;

	while (client.connected() && millis() - startTime < timeout) {
		while (client.available()) {
			String line = client.readStringUntil('\n');

			if (!headersEnded) {
				// Fine header HTTP
				if (line == "\r") {
					headersEnded = true;
				}
			} else {
				body += line + "\n";
			}
		}
	}

	client.stop();
	return body;
}

static const int matrix_width = 12;

static const int matrix_height = 8;

static unsigned long lastClockUpdate = 0;

const unsigned long CLOCK_UPDATE_INTERVAL = 60000; // 1 minuto

void setup() {
	Serial.begin(115200);
	while (!Serial && millis() < 5000)
		; // Timeout di 5 secondi per Serial
	matrix.begin();
	const uint32_t happy[] = {0x19819, 0x80000001, 0x81f8000};
	matrix.loadFrame(happy);
	pinMode(PIN_MINUS, INPUT_PULLUP);
	pinMode(PIN_POWER, INPUT_PULLUP);
	pinMode(PIN_PLUS, INPUT_PULLUP);
	pinMode(PIN_UPDATE, INPUT_PULLUP);
	Serial.println("Connessione alla rete WiFi...");
	WiFi.disconnect(); // Assicurati di partire pulito
	delay(100);
	if (WiFi.begin(ssid, password) != WL_CONNECTED) {
		Serial.println("❌ Connessione WiFi fallita!");
		while (true) {
			delay(1000);
		}
	}
	// Disabilita il power saving del WiFi (importante!)
	WiFi.setHostname("arduino-lamp-controller");
	Serial.println("✅ Connesso al WiFi!");
	Serial.print("IP locale: ");
	Serial.println(WiFi.localIP());
	Serial.println("Pronto a ricevere input dai pulsanti.");
	timeClient.begin();
}
float getServerBrightness() {
	String response = httpGet("/status");
	if (response.length() == 0) {
		Serial.println("❌ Stato vuoto");
		return -1.0;
	}
	int idx = response.indexOf("\"brightness\":");
	if (idx < 0) {
		Serial.println("❌ Brightness non trovato");
		return -1.0;
	}
	idx += strlen("\"brightness\":");
	return response.substring(idx).toFloat();
}
void setServerBrightness(float value) {
	if (value < 0.0)
		value = 0.0;
	if (value > 1.0)
		value = 1.0;
	String body = String("{\"brightness\":") + String(value, 1) + "}";
	httpPost("/brightness", body);
}

void displayStaticTemp(int temp) {
	matrix.beginDraw();
	matrix.stroke(0xFFFFFF);
	matrix.textFont(Font_4x6);
	matrix.clear();
	
	String text = String(temp) + "C";
	
	// Centra il testo orizzontalmente
	// Font_4x6: ogni carattere è largo circa 4 pixel
	int xStart = (12 - (text.length() * 4)) / 2;
	if (xStart < 0) xStart = 0;

	matrix.beginText(xStart, 1, 0xFFFFFF);
	matrix.print(text);
	matrix.endText();
	matrix.endDraw();
}

void printWeather() {
	Serial.println("Connecting to WeatherAPI...");
	if (!client.connect(weathersite.c_str(), 80)) {
		Serial.println("Connection failed");
		return;
	}
	// HTTP request
	client.print("GET /v1/current.json?key=" + weatherapikey + "&q=" + city +
				 "&aqi=no HTTP/1.1\r\n" + "Host: " + weathersite + "\r\n" +
				 "Connection: close\r\n\r\n");
	// Skip HTTP headers
	while (client.connected()) {
		String line = client.readStringUntil('\n');
		if (line == "\r")
			break;
	}
	// Read JSON body
	String payload;
	unsigned long readTimeout = millis();
	while (client.connected() || client.available()) {
		if (client.available()) {
			payload += (char)client.read();
			readTimeout = millis();
		}
		if (millis() - readTimeout > 2000) break;
	}
	client.stop();

	payload.trim();
	
	// Troubleshooting: Cerchiamo l'inizio del JSON
	int jsonStart = payload.indexOf('{');
	if (jsonStart == -1) {
		Serial.println("Errore: Nessun JSON trovato nella risposta");
		Serial.println("Risposta ricevuta:");
		Serial.println(payload);
		return;
	}
	
	// Riduciamo il payload solo alla parte JSON (rimuove eventuali residui di header o chunked encoding iniziale)
	String jsonBody = payload.substring(jsonStart);

	// Parse JSON con Filtro (consuma meno memoria e ignora il superfluo)
	JsonDocument filter;
	filter["current"]["temp_c"] = true;
	filter["current"]["condition"]["text"] = true;

	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, jsonBody, DeserializationOption::Filter(filter));

	if (error) {
		Serial.print("JSON error: ");
		Serial.println(error.c_str());
		Serial.println("Corpo JSON tentato:");
		Serial.println(jsonBody);
		return;
	}

	// Extract data con valori di default
	currentCondition = doc["current"]["condition"]["text"] | "N/A";
	currentTemperature = doc["current"]["temp_c"] | 0.0;

	// Print
	Serial.println("---- Weather ----");
	Serial.print("Condition: ");
	Serial.println(currentCondition);
	Serial.print("Temperature: ");
	Serial.print(currentTemperature);
	Serial.println(" °C");
	Serial.println("-----------------");
	
	displayStaticTemp((int)currentTemperature);
}



void loop() {
	timeClient.update();
	static bool initialized = false;
	static bool lastMinus = HIGH;
	static bool lastPower = HIGH;
	static bool lastPlus = HIGH;
	static bool lastUpdate = HIGH;
	// Controllo periodico della connessione WiFi
	if (millis() - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
		checkWiFiConnection();
		lastConnectionCheck = millis();
	}
	if (!showWeather && millis() - lastClockUpdate > CLOCK_UPDATE_INTERVAL) {
		lastClockUpdate = millis();
		matrix.beginText(0, 1, 0xFFFFFF);
		matrix.print(timeClient.getFormattedTime());
		matrix.endText(SCROLL_LEFT);
		delay(100);
	}
	bool minus = digitalRead(PIN_MINUS);
	bool power = digitalRead(PIN_POWER);
	bool plus = digitalRead(PIN_PLUS);
	bool update = digitalRead(PIN_UPDATE);
	// --- Fase di inizializzazione ---
	if (!initialized) {
		lastMinus = minus;
		lastPower = power;
		lastPlus = plus;
		lastUpdate = update;
		initialized = true;
		return;
	}
	// --- Gestione power toggle ---
	if (power == LOW && lastPower == HIGH) {
		httpGet("/power?form_toggle=1");
	}
	if (plus == LOW && lastPlus == HIGH) {
		httpGet("/increase_brightness");
	}
	if (minus == LOW && lastMinus == HIGH) {
		httpGet("/decrease_brightness");
	}
	if (update == LOW && lastUpdate == HIGH) {
		Serial.println("UPDATE!!!");
		showWeather = !showWeather;
		if (showWeather) {
			printWeather();
		} else {
			lastClockUpdate = 0; // Trigger immediate clock update
		}
	}
	// --- Aggiornamento stato ---
	lastMinus = minus;
	lastPower = power;
	lastPlus = plus;
	lastUpdate = update;
	delay(200);
}
