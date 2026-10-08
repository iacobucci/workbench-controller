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

// --- Debounce state ---
struct ButtonDebounce {
	int pin;
	bool lastReading;
	bool isPressed;
	unsigned long lastDebounceTime;
};

ButtonDebounce btnMinus  = {PIN_MINUS, HIGH, false, 0};
ButtonDebounce btnPower  = {PIN_POWER, HIGH, false, 0};
ButtonDebounce btnPlus   = {PIN_PLUS,  HIGH, false, 0};
ButtonDebounce btnUpdate = {PIN_UPDATE, HIGH, false, 0};

const unsigned long DEBOUNCE_DELAY = 40; // ms

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
	// Power symbol: circle with open top and center line
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

// Check button debounce and detect press event
bool checkButtonPressed(ButtonDebounce &btn) {
	bool reading = digitalRead(btn.pin);

	if (reading != btn.lastReading) {
		btn.lastDebounceTime = millis();
		btn.lastReading = reading;
	}

	if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY) {
		if (reading == LOW && !btn.isPressed) {
			btn.isPressed = true;
			return true;
		} else if (reading == HIGH && btn.isPressed) {
			btn.isPressed = false;
		}
	}
	return false;
}

// Parse incoming status or signal from Serial (Rock board or PlatformIO monitor)
void parseIncomingSerial(String line) {
	line.trim();
	if (line.length() == 0) return;

	Serial.print("[RECV] ");
	Serial.println(line);

	String lineUpper = line;
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

	// Commands typed via PlatformIO monitor
	if (lineUpper == "POWER" || lineUpper == "TOGGLE" || lineUpper == "P") {
		Serial.println("POWER");
		drawPowerIcon();
	} else if (lineUpper == "BRIGHTNESS_UP" || lineUpper == "+" || lineUpper == "UP") {
		Serial.println("BRIGHTNESS_UP");
		drawPlusIcon();
	} else if (lineUpper == "BRIGHTNESS_DOWN" || lineUpper == "-" || lineUpper == "DOWN") {
		Serial.println("BRIGHTNESS_DOWN");
		drawMinusIcon();
	} else if (lineUpper == "STATUS" || lineUpper == "UPDATE" || lineUpper == "?") {
		Serial.println("STATUS");
		drawSyncIcon();
	} else if (lineUpper.startsWith("OK")) {
		// Acknowledgment received
		if (lineUpper.indexOf("POWER") >= 0) {
			drawPowerIcon();
		} else if (lineUpper.indexOf("BRIGHTNESS") >= 0) {
			drawPlusIcon();
		}
	}
}

void setup() {
	// USB Serial at 115200 baud
	Serial.begin(115200);

	// Wait up to 2 seconds for USB CDC Serial to connect (if monitoring)
	unsigned long serialWaitStart = millis();
	while (!Serial && (millis() - serialWaitStart < 2000)) {
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
	Serial.println("READY: Arduino Workbench Controller (USB Mode)");
	Serial.println("Commands available: POWER, BRIGHTNESS_UP, BRIGHTNESS_DOWN, STATUS");

	// Initial status request to IoT server
	Serial.println("STATUS");
}

void loop() {
	// Check physical buttons
	if (checkButtonPressed(btnPower)) {
		Serial.println("POWER");
		drawPowerIcon();
	}

	if (checkButtonPressed(btnPlus)) {
		Serial.println("BRIGHTNESS_UP");
		drawPlusIcon();
	}

	if (checkButtonPressed(btnMinus)) {
		Serial.println("BRIGHTNESS_DOWN");
		drawMinusIcon();
	}

	if (checkButtonPressed(btnUpdate)) {
		Serial.println("STATUS");
		drawSyncIcon();
	}

	// Check incoming USB Serial messages (from Rock 3C IoT server or PlatformIO monitor)
	while (Serial.available()) {
		String line = Serial.readStringUntil('\n');
		parseIncomingSerial(line);
	}

	// Restore default display when feedback animation ends
	if (feedbackUntil > 0 && millis() >= feedbackUntil) {
		feedbackUntil = 0;
		showDefaultDisplay();
	}
}
