#include <Adafruit_NeoPixel.h>
#include <avr/sleep.h>

// Pin-Definitionen
#define LED_PIN 9
#define NUM_LEDS 16
#define TOUCH_PIN 2
#define CHARGE_PIN 3
#define FULL_BATT_PIN 4
#define LDR1 A0
#define LDR2 A1
#define LDR3 A2
#define BATT_LEVEL A3

// Zeitkonstanten
#define SLEEP_TIMEOUT 10000 // 10 Sekunden Timeout für Inaktivität
#define LONG_PRESS_TIME 1000 // 1 Sekunde für langen Tastendruck
#define SHORT_PRESS_TIME 200 // 200 Millisekunden für kurzen Tastendruck
#define BLINK_INTERVAL 500   // Blink-Intervall für volle Batterie
#define CHARGE_INTERVAL 100  // Ladeanimations-Intervall
#define BATTERY_ANIM_INTERVAL 50 // Intervall für Batterieanzeige-Animation

Adafruit_NeoPixel ring = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastActivityTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastChargeTime = 0;
unsigned long lastBatteryAnimTime = 0;
bool sleepMode = true;
bool chargeMode = false;
bool compareMode = false;
bool blinkState = false;
bool isCharging = false;
bool canEnterCompareMode = true;
bool overrideChargeDisplay = false;
bool batteryDisplayActive = false;
bool batteryAnimRunning = false;
int chargePosition = 0;
int batteryAnimStep = 0;
int targetBatteryLeds = 0;

// Setup-Funktion: Initialisiert die Pins und den LED-Ring
void setup() {
    pinMode(TOUCH_PIN, INPUT);
    pinMode(CHARGE_PIN, INPUT);
    pinMode(FULL_BATT_PIN, INPUT);
    ring.begin();
    ring.show();
}

// Hauptprogrammschleife: Steuert den Betriebsmodus und reagiert auf Eingaben
void loop() {
    unsigned long currentTime = millis();
    int touchState = digitalRead(TOUCH_PIN);
    int charging = digitalRead(CHARGE_PIN);
    int fullBattery = digitalRead(FULL_BATT_PIN);

    if (sleepMode) {
        if (touchState == HIGH || charging == HIGH) {
            wakeUp();
        } else if (currentTime - lastActivityTime > SLEEP_TIMEOUT) {
            enterSleepMode();
        }
    } else if (compareMode) {
        handleCompareMode();
        if (longPressDetected()) {
            compareMode = false;
            ring.clear();
            ring.show();
            overrideChargeDisplay = false; // Ladeanimation nach Verlassen des Vergleichsmodus wieder aktivieren
        }
    } else {
        if (longPressDetected()) {
            compareMode = !compareMode;
            overrideChargeDisplay = true;
        } else if (touchState == HIGH) {
            batteryDisplayActive = !batteryDisplayActive;
            if (batteryDisplayActive) {
                startBatteryAnimation();
            } else {
                ring.clear();
                ring.show();
            }
            delay(300); // Entprellung des Touch-Sensors
        }
    }

    if (batteryAnimRunning) {
        updateBatteryAnimation(currentTime);
    }

    if (charging) {
        if (!overrideChargeDisplay && !batteryDisplayActive) {
            isCharging = true;
            if (fullBattery) {
                blinkNeonYellow(currentTime);
            } else {
                chargeAnimation(currentTime);
            }
        }
    } else {
        isCharging = false;
    }
}

void wakeUp() {
    sleepMode = false;
    lastActivityTime = millis();
    ring.clear();
    ring.show();
}

void enterSleepMode() {
    sleepMode = true;
    ring.clear();
    ring.show();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_mode();
}

bool longPressDetected() {
    unsigned long pressStart = millis();
    while (digitalRead(TOUCH_PIN) == HIGH) {
        if (millis() - pressStart > LONG_PRESS_TIME) {
            return true;
        }
    }
    return false;
}

void startBatteryAnimation() {
    int battLevel = analogRead(BATT_LEVEL);
    targetBatteryLeds = map(battLevel, 0, 1023, 0, NUM_LEDS);
    batteryAnimStep = 0;
    batteryAnimRunning = true;
    lastBatteryAnimTime = millis();
}

void updateBatteryAnimation(unsigned long currentTime) {
    if (currentTime - lastBatteryAnimTime >= BATTERY_ANIM_INTERVAL) {
        lastBatteryAnimTime = currentTime;
        ring.clear();
        
        if (batteryAnimStep < NUM_LEDS) {
            for (int i = 0; i < 3; i++) {
                int index = (batteryAnimStep - i + NUM_LEDS) % NUM_LEDS;
                int brightness = 255 - (i * 85);
                ring.setPixelColor(index, ring.Color(brightness, brightness, 0));
            }
        } else {
            for (int i = 0; i < min(batteryAnimStep - NUM_LEDS, targetBatteryLeds); i++) {
                int brightness = 255 - ((targetBatteryLeds - i) * 20);
                ring.setPixelColor(i, ring.Color(brightness, brightness, 0));
            }
        }

        ring.show();
        batteryAnimStep++;
        
        if (batteryAnimStep >= NUM_LEDS + targetBatteryLeds) {
            batteryAnimRunning = false;
        }
    }
}

void handleCompareMode() {
    int ldr1 = analogRead(LDR1);
    int ldr2 = analogRead(LDR2);
    int ldr3 = analogRead(LDR3);
    int diff1 = abs(ldr1 - ldr2);
    int diff2 = abs(ldr2 - ldr3);
    int diff3 = abs(ldr1 - ldr3);
    int maxDiff = max(diff1, max(diff2, diff3));

    ring.clear();
    uint32_t color;
    if (maxDiff < 50) {
        color = ring.Color(0, 0, 255);
    } else if (maxDiff < 200) {
        color = ring.Color(0, 255, 0);
    } else {
        color = ring.Color(255, 0, 0);
    }

    for (int i = 0; i < NUM_LEDS; i++) {
        ring.setPixelColor(i, color);
    }
    ring.show();
}


void blinkNeonYellow(unsigned long currentTime) {
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
        lastBlinkTime = currentTime;
        blinkState = !blinkState;
        if (blinkState) {
            for (int i = 0; i < NUM_LEDS; i++) {
                ring.setPixelColor(i, ring.Color(255, 255, 0));
            }
        } else {
            ring.clear();
        }
        ring.show();
    }
}

void chargeAnimation(unsigned long currentTime) {
    if (currentTime - lastChargeTime >= CHARGE_INTERVAL) {
        lastChargeTime = currentTime;
        ring.clear();
        for (int i = 0; i < 4; i++) {
            int index = (chargePosition + i) % NUM_LEDS;
            ring.setPixelColor(index, ring.Color(255, 255, 0));
        }
        ring.show();
        chargePosition = (chargePosition + 1) % NUM_LEDS;
    }
    if (isCharging = false) {
  ring.clear();
}
ring.show();
}

