#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>

// --- Pin definitions ---
const int PIN_MINUS = 2;
const int PIN_POWER = 3;
const int PIN_PLUS = 4;
const int PIN_UPDATE = 5;

// --- State variables ---
bool currentPower = false;
int currentBrightness = 128;
bool hasState = false;

// --- LED Matrix ---
ArduinoLEDMatrix matrix;

// Default "happy" face frame
const uint32_t FRAME_HAPPY[] = {
	0x19819,
	0x80000001,
	0x81f8000
};

// Off frame (small dot)
const uint32_t FRAME_OFF[] = {
	0x0,
	0x00060000,
	0x0
};

unsigned long feedbackUntil = 0;
const unsigned long FEEDBACK_DURATION = 1500; // ms

// --- Button tracking (instant response with debounce lockout) ---
bool lastMinus  = HIGH;
bool lastPower  = HIGH;
bool lastPlus   = HIGH;
bool lastUpdate = HIGH;

unsigned long lastMinusTime  = 0;
unsigned long lastPowerTime  = 0;
unsigned long lastPlusTime   = 0;
unsigned long lastUpdateTime = 0;

const unsigned long DEBOUNCE_LOCKOUT = 200; // ms

// --- Visual feedback helpers ---
void showDefaultDisplay() {
	if (hasState) {
		if (currentPower) {
			matrix.loadFrame(FRAME_HAPPY);
		} else {
			matrix.loadFrame(FRAME_OFF);
		}
	} else {
		matrix.loadFrame(FRAME_HAPPY);
	}
}

void displayText(const String &text) {
	matrix.beginDraw();
	matrix.stroke(0xFFFFFF);
	matrix.textFont(Font_4x6);
	matrix.clear();

	int xStart = (12 - ((int)text.length() * 4)) / 2;
	if (xStart < 0) xStart = 0;

	matrix.beginText(xStart, 1, 0xFFFFFF);
	matrix.print(text);
	matrix.endText();
	matrix.endDraw();
	feedbackUntil = millis() + FEEDBACK_DURATION;
}

void drawPowerIcon() {
	matrix.beginDraw();
	matrix.clear();
	matrix.stroke(0xFFFFFF);
	matrix.rect(3, 1, 6, 6);
	matrix.stroke(0x000000);
	matrix.point(5, 1);
	matrix.point(6, 1);
	matrix.stroke(0xFFFFFF);
	matrix.line(5, 0, 5, 3);
	matrix.endDraw();
	feedbackUntil = millis() + FEEDBACK_DURATION;
}

void drawPlusIcon() {
	matrix.beginDraw();
	matrix.clear();
	matrix.stroke(0xFFFFFF);
	matrix.line(5, 1, 5, 6);
	matrix.line(2, 3, 8, 3);
	matrix.line(2, 4, 8, 4);
	matrix.endDraw();
	feedbackUntil = millis() + FEEDBACK_DURATION;
}

void drawMinusIcon() {
	matrix.beginDraw();
	matrix.clear();
	matrix.stroke(0xFFFFFF);
	matrix.line(3, 3, 8, 3);
	matrix.line(3, 4, 8, 4);
	matrix.endDraw();
	feedbackUntil = millis() + FEEDBACK_DURATION;
}

void drawSyncIcon() {
	matrix.beginDraw();
	matrix.clear();
	matrix.stroke(0xFFFFFF);
	matrix.textFont(Font_4x6);
	matrix.beginText(0, 1, 0xFFFFFF);
	matrix.print("SYNC");
	matrix.endText();
	matrix.endDraw();
	feedbackUntil = millis() + FEEDBACK_DURATION;
}

// Send signal over USB serial with immediate flush
void sendSignal(const char *cmd) {
	Serial.println(cmd);
	Serial.flush();
}

// Parse incoming status or signal from Serial
void parseIncomingSerial(const String &line) {
	String trimmed = line;
	trimmed.trim();
	if (trimmed.length() == 0) return;

	String lineUpper = trimmed;
	lineUpper.toUpperCase();

	// Check for STATUS message: e.g. "STATUS state=ON brightness=204"
	if (lineUpper.startsWith("STATUS")) {
		hasState = true;
		if (lineUpper.indexOf("STATE=ON") >= 0 || lineUpper.indexOf("STATE: ON") >= 0) {
			currentPower = true;
		} else if (lineUpper.indexOf("STATE=OFF") >= 0 || lineUpper.indexOf("STATE: OFF") >= 0) {
			currentPower = false;
		}

		int bIdx = lineUpper.indexOf("BRIGHTNESS=");
		if (bIdx < 0) bIdx = lineUpper.indexOf("BRIGHTNESS:");
		if (bIdx >= 0) {
			int valStart = lineUpper.indexOf("=", bIdx);
			if (valStart < 0) valStart = lineUpper.indexOf(":", bIdx);
			if (valStart >= 0) {
				String bStr = lineUpper.substring(valStart + 1);
				bStr.trim();
				int endSpace = bStr.indexOf(' ');
				if (endSpace > 0) bStr = bStr.substring(0, endSpace);
				currentBrightness = bStr.toInt();
			}
		}

		if (currentPower) {
			int pct = (currentBrightness * 100) / 254;
			displayText(String(pct) + "%");
		} else {
			displayText("OFF");
		}
		return;
	}

	// Commands typed via PlatformIO monitor or server acknowledgments
	if (lineUpper == "POWER" || lineUpper == "TOGGLE" || lineUpper == "P") {
		sendSignal("POWER");
		drawPowerIcon();
	} else if (lineUpper == "BRIGHTNESS_UP" || lineUpper == "+" || lineUpper == "UP") {
		sendSignal("BRIGHTNESS_UP");
		drawPlusIcon();
	} else if (lineUpper == "BRIGHTNESS_DOWN" || lineUpper == "-" || lineUpper == "DOWN") {
		sendSignal("BRIGHTNESS_DOWN");
		drawMinusIcon();
	} else if (lineUpper == "STATUS" || lineUpper == "UPDATE" || lineUpper == "?") {
		sendSignal("STATUS");
		drawSyncIcon();
	} else if (lineUpper.startsWith("OK")) {
		if (lineUpper.indexOf("POWER") >= 0) {
			drawPowerIcon();
		} else if (lineUpper.indexOf("BRIGHTNESS") >= 0) {
			drawPlusIcon();
		}
	}
}

void setup() {
	// USB CDC Serial at 115200 baud
	Serial.begin(115200);

	// Wait briefly for USB CDC Serial to connect (if monitoring)
	unsigned long start = millis();
	while (!Serial && (millis() - start < 1500)) {
		;
	}

	// Initialize LED matrix
	matrix.begin();
	matrix.loadFrame(FRAME_HAPPY);

	// Initialize button pins
	pinMode(PIN_MINUS, INPUT_PULLUP);
	pinMode(PIN_POWER, INPUT_PULLUP);
	pinMode(PIN_PLUS, INPUT_PULLUP);
	pinMode(PIN_UPDATE, INPUT_PULLUP);

	// Announce readiness over USB Serial
	sendSignal("READY: Arduino Workbench Controller (USB Mode)");
	sendSignal("STATUS");
}

// Non-blocking serial RX buffer
String serialRxBuffer = "";

void loop() {
	unsigned long now = millis();

	// Read physical buttons (falling edge = pressed)
	bool minus  = digitalRead(PIN_MINUS);
	bool power  = digitalRead(PIN_POWER);
	bool plus   = digitalRead(PIN_PLUS);
	bool update = digitalRead(PIN_UPDATE);

	if (power == LOW && lastPower == HIGH && (now - lastPowerTime > DEBOUNCE_LOCKOUT)) {
		lastPowerTime = now;
		sendSignal("POWER");
		drawPowerIcon();
	}

	if (plus == LOW && lastPlus == HIGH && (now - lastPlusTime > DEBOUNCE_LOCKOUT)) {
		lastPlusTime = now;
		sendSignal("BRIGHTNESS_UP");
		drawPlusIcon();
	}

	if (minus == LOW && lastMinus == HIGH && (now - lastMinusTime > DEBOUNCE_LOCKOUT)) {
		lastMinusTime = now;
		sendSignal("BRIGHTNESS_DOWN");
		drawMinusIcon();
	}

	if (update == LOW && lastUpdate == HIGH && (now - lastUpdateTime > DEBOUNCE_LOCKOUT)) {
		lastUpdateTime = now;
		sendSignal("STATUS");
		drawSyncIcon();
	}

	lastMinus  = minus;
	lastPower  = power;
	lastPlus   = plus;
	lastUpdate = update;

	// Non-blocking serial read (accumulate until newline)
	while (Serial.available()) {
		char c = (char)Serial.read();
		if (c == '\n' || c == '\r') {
			if (serialRxBuffer.length() > 0) {
				parseIncomingSerial(serialRxBuffer);
				serialRxBuffer = "";
			}
		} else {
			if (serialRxBuffer.length() < 128) {
				serialRxBuffer += c;
			}
		}
	}

	// Restore default display when feedback animation ends
	if (feedbackUntil > 0 && millis() >= feedbackUntil) {
		feedbackUntil = 0;
		showDefaultDisplay();
	}
}
