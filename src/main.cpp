#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>

// --- Pin definitions ---
const int PIN_MINUS = 2;
const int PIN_POWER = 3;
const int PIN_PLUS = 4;
const int PIN_RESTART = 5;
const int PIN_UPDATE = PIN_RESTART; // Backward compatibility alias

// --- State variables ---
bool currentPower = false;
int currentBrightness = 128;
bool hasState = false;

// --- LED Matrix ---
ArduinoLEDMatrix matrix;

// Blank frame: all 96 LEDs completely OFF (no light emission at night)
const uint32_t FRAME_BLANK[] = {0, 0, 0};

unsigned long feedbackUntil = 0;
const unsigned long FEEDBACK_DURATION = 1200; // ms

// --- Button tracking (instant response with debounce lockout) ---
bool lastMinus = HIGH;
bool lastPower = HIGH;
bool lastPlus  = HIGH;

unsigned long lastMinusTime = 0;
unsigned long lastPowerTime = 0;
unsigned long lastPlusTime  = 0;

// Long-press reboot tracking for Reset button (Pin 5, active HIGH with external pull-down)
const bool RESTART_PRESSED        = HIGH;
bool restartActive                = false;
unsigned long restartPressTime    = 0;
unsigned long restartReleaseStart = 0;
bool restartLongTriggered         = false;
int restartCountdownStage         = 0;
const unsigned long REBOOT_HOLD_DURATION = 3000; // 3 seconds to reboot rock-3c
const unsigned long SHORT_PRESS_MIN_MS   = 50;   // Minimum press duration (debounce)
const unsigned long RELEASE_DEBOUNCE_MS  = 30;   // Stable release time before ending press

const unsigned long DEBOUNCE_LOCKOUT = 200; // ms

// --- Visual feedback helpers ---
void turnOffDisplay() {
	matrix.loadFrame(FRAME_BLANK);
	feedbackUntil = 0;
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

void drawRestartIcon() {
	matrix.beginDraw();
	matrix.clear();
	matrix.stroke(0xFFFFFF);
	matrix.textFont(Font_4x6);
	matrix.beginText(0, 1, 0xFFFFFF);
	matrix.print("RST");
	matrix.endText();
	matrix.endDraw();
	feedbackUntil = millis() + FEEDBACK_DURATION;
}

void drawRebootIcon() {
	matrix.beginDraw();
	matrix.clear();
	matrix.stroke(0xFFFFFF);
	matrix.textFont(Font_4x6);
	matrix.beginText(0, 1, 0xFFFFFF);
	matrix.print("RBT");
	matrix.endText();
	matrix.endDraw();
	feedbackUntil = millis() + 3000;
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
	} else if (lineUpper == "REBOOT" || lineUpper == "SYSTEM_REBOOT" || lineUpper == "SYS_REBOOT") {
		sendSignal("REBOOT");
		drawRebootIcon();
	} else if (lineUpper == "RESTART" || lineUpper == "RESET" || lineUpper == "R") {
		sendSignal("RESTART");
		drawRestartIcon();
	} else if (lineUpper == "STATUS" || lineUpper == "UPDATE" || lineUpper == "?") {
		sendSignal("STATUS");
		drawSyncIcon();
	} else if (lineUpper.startsWith("OK")) {
		if (lineUpper.indexOf("REBOOT") >= 0) {
			drawRebootIcon();
		} else if (lineUpper.indexOf("POWER") >= 0) {
			drawPowerIcon();
		} else if (lineUpper.indexOf("BRIGHTNESS_DECREASED") >= 0 || lineUpper.indexOf("BRIGHTNESS_DOWN") >= 0) {
			drawMinusIcon();
		} else if (lineUpper.indexOf("BRIGHTNESS") >= 0) {
			drawPlusIcon();
		} else if (lineUpper.indexOf("RESTART") >= 0 || lineUpper.indexOf("RESET") >= 0) {
			drawRestartIcon();
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

	// Initialize LED matrix and ensure it is completely OFF
	matrix.begin();
	turnOffDisplay();

	// Initialize button pins
	pinMode(PIN_MINUS, INPUT_PULLUP);
	pinMode(PIN_POWER, INPUT_PULLUP);
	pinMode(PIN_PLUS, INPUT_PULLUP);
	pinMode(PIN_RESTART, INPUT);

	// Announce readiness over USB Serial
	sendSignal("READY: Arduino Workbench Controller (USB Mode)");
	sendSignal("STATUS");
}

// Non-blocking serial RX buffer
String serialRxBuffer = "";

void loop() {
	unsigned long now = millis();

	// Read physical buttons (falling edge = pressed for -, power, +; active HIGH for Pin 5)
	bool minus   = digitalRead(PIN_MINUS);
	bool power   = digitalRead(PIN_POWER);
	bool plus    = digitalRead(PIN_PLUS);
	bool restart = digitalRead(PIN_RESTART);

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

	// Reset button (Pin 5): short press (<1s) restarts service, 3s hold reboots rock-3c
	if (!restartActive) {
		if (restart == RESTART_PRESSED) {
			restartActive = true;
			restartPressTime = now;
			restartReleaseStart = 0;
			restartLongTriggered = false;
			restartCountdownStage = 0;
		}
	} else if (restart == RESTART_PRESSED) {
		restartReleaseStart = 0;
		if (!restartLongTriggered) {
			unsigned long elapsed = now - restartPressTime;
			if (elapsed >= REBOOT_HOLD_DURATION) {
				restartLongTriggered = true;
				sendSignal("REBOOT");
				drawRebootIcon();
			} else if (elapsed >= 2000 && restartCountdownStage < 2) {
				restartCountdownStage = 2;
				displayText("1");
			} else if (elapsed >= 1000 && restartCountdownStage < 1) {
				restartCountdownStage = 1;
				displayText("2");
			}
		}
	} else {
		if (restartReleaseStart == 0) {
			restartReleaseStart = now;
		} else if (now - restartReleaseStart >= RELEASE_DEBOUNCE_MS) {
			unsigned long heldDuration = restartReleaseStart - restartPressTime;
			if (!restartLongTriggered) {
				if (heldDuration >= SHORT_PRESS_MIN_MS && heldDuration < 1000) {
					sendSignal("RESTART");
					drawRestartIcon();
				} else if (restartCountdownStage > 0) {
					// Aborted long press before reaching 3s
					turnOffDisplay();
				}
			}
			restartActive = false;
			restartReleaseStart = 0;
			restartLongTriggered = false;
			restartCountdownStage = 0;
		}
	}

	lastMinus = minus;
	lastPower = power;
	lastPlus  = plus;

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

	// Turn off display completely when feedback expires (idle = completely dark)
	if (feedbackUntil > 0 && millis() >= feedbackUntil) {
		feedbackUntil = 0;
		turnOffDisplay();
	}
}
