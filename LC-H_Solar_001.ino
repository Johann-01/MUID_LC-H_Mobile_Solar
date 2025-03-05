#include <Adafruit_NeoPixel.h>  // Bibliothek für die Steuerung der NeoPixel-LEDs
#include <avr/sleep.h>          // Bibliothek für den Schlafmodus des Mikrocontrollers

// Pin-Definitionen für LEDs, Sensoren und Batterieüberwachung
#define LED_PIN 9          // Pin für den LED-Ring
#define NUM_LEDS 16        // Anzahl der LEDs im Ring
#define TOUCH_PIN 2        // Pin für den kapazitiven Touch-Sensor
#define CHARGE_PIN 3       // Pin für die Ladeerkennung
#define FULL_BATT_PIN 4    // Pin für die Anzeige, dass der Akku voll geladen ist
#define LDR1 A0           // Fotowiderstand 1 (Lichtsensor)
#define LDR2 A1           // Fotowiderstand 2 (Lichtsensor)
#define LDR3 A2           // Fotowiderstand 3 (Lichtsensor)
#define BATT_LEVEL A3     // Analoger Eingang für den Batterie-Ladezustand

// Zeitkonstanten für verschiedene Funktionen
#define SLEEP_TIMEOUT 10000 // Zeit in ms, bevor das System in den Schlafmodus geht
#define LONG_PRESS_TIME 1000 // Mindestzeit für langen Tastendruck (1 Sekunde)
#define SHORT_PRESS_TIME 200 // Mindestzeit für kurzen Tastendruck (200 Millisekunden)
#define BLINK_INTERVAL 500   // Intervall für das Blinken der LED (500 ms)
#define CHARGE_INTERVAL 100  // Zeitabstand für die Ladeanimations-Aktualisierung (100 ms)
#define BATTERY_ANIM_INTERVAL 50 // Intervall für die Batterieanzeige-Animation (50 ms)

// Initialisierung des LED-Rings
Adafruit_NeoPixel ring = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Variablen zur Speicherung von Zeitwerten und Zuständen
unsigned long lastActivityTime = 0; // Letzte Aktivität, um den Schlafmodus zu steuern
unsigned long lastBlinkTime = 0;    // Zeitpunkt der letzten Blinkaktion
unsigned long lastChargeTime = 0;   // Zeitpunkt der letzten Ladeanimations-Aktualisierung
unsigned long lastBatteryAnimTime = 0; // Zeitpunkt der letzten Batterie-Animation

// Statusvariablen
bool sleepMode = true;             // Gibt an, ob sich das System im Schlafmodus befindet
bool chargeMode = false;           // Gibt an, ob sich das Gerät im Lademodus befindet
bool compareMode = false;          // Vergleichsmodus für die Lichtsensoren
bool blinkState = false;           // Status für das Blinken der LED
bool isCharging = false;           // Status, ob das Gerät gerade lädt
bool canEnterCompareMode = true;   // Erlaubt das Betreten des Vergleichsmodus
bool overrideChargeDisplay = false;// Überschreibt die Ladeanzeige
bool batteryDisplayActive = false; // Gibt an, ob die Batterieanzeige aktiv ist
bool batteryAnimRunning = false;   // Gibt an, ob die Batterieanimationssequenz läuft

// Variablen für die Animation der Ladelichter
int chargePosition = 0;    // Aktuelle Position des Lade-Animationspunkts
int batteryAnimStep = 0;   // Fortschritt der Batterie-Animation
int targetBatteryLeds = 0; // Anzahl der LEDs, die den Batteriestand anzeigen

// Setup-Funktion: Initialisiert die Pins und den LED-Ring
void setup() {
    pinMode(TOUCH_PIN, INPUT);   // Setzt den Touch-Sensor-Pin als Eingang
    pinMode(CHARGE_PIN, INPUT);  // Setzt den Lade-Pin als Eingang
    pinMode(FULL_BATT_PIN, INPUT); // Setzt den Pin für die volle Batterie als Eingang
    ring.begin();  // Initialisiert den LED-Ring
    ring.show();   // Löscht alle LEDs (setzt sie auf AUS)
}

// Hauptprogrammschleife: Steuert den Betriebsmodus und reagiert auf Eingaben
void loop() {
    unsigned long currentTime = millis(); // Speichert die aktuelle Zeit in Millisekunden
    int touchState = digitalRead(TOUCH_PIN); // Liest den Zustand des Touch-Sensors
    int charging = digitalRead(CHARGE_PIN);  // Liest den Ladezustand
    int fullBattery = digitalRead(FULL_BATT_PIN); // Liest den Zustand der vollen Batterie

    // Falls sich das Gerät im Schlafmodus befindet
    if (sleepMode) {
        if (touchState == HIGH || charging == HIGH) { // Falls der Touch-Sensor berührt oder das Gerät geladen wird
            wakeUp();  // Weckt das System auf
        } else if (currentTime - lastActivityTime > SLEEP_TIMEOUT) { // Falls die Inaktivitätszeit überschritten wurde
            enterSleepMode(); // Gerät in den Schlafmodus versetzen
        }
    } 
    else if (compareMode) { // Falls der Vergleichsmodus aktiv ist
        handleCompareMode(); // Führt die Vergleichsberechnung aus
        if (longPressDetected()) { // Falls eine lange Berührung erkannt wird
            compareMode = false; // Vergleichsmodus verlassen
            ring.clear(); // LEDs ausschalten
            ring.show();
            overrideChargeDisplay = false; // Ladeanimation nach Verlassen wieder aktivieren
        }
    } 
    else { // Normale Betriebsart
        if (longPressDetected()) { // Falls eine lange Berührung erkannt wird
            compareMode = !compareMode; // Vergleichsmodus umschalten
            overrideChargeDisplay = true; // Ladeanzeige unterdrücken
        } else if (touchState == HIGH) { // Falls der Touch-Sensor betätigt wird
            batteryDisplayActive = !batteryDisplayActive; // Batterieanzeige umschalten
            if (batteryDisplayActive) {
                startBatteryAnimation(); // Starte die Batterieanzeige
            } else {
                ring.clear(); // LEDs ausschalten
                ring.show();
            }
            delay(300); // Entprellung des Touch-Sensors
        }
    }

    // Falls die Batterieanimation läuft
    if (batteryAnimRunning) {
        updateBatteryAnimation(currentTime); // Aktualisiert die Batterieanimation
    }

    // Falls das Gerät geladen wird
    if (charging) {
        if (!overrideChargeDisplay && !batteryDisplayActive) { // Falls keine Animation aktiv ist
            isCharging = true;
            if (fullBattery) {
                blinkNeonYellow(currentTime); // Blinken bei voller Batterie
            } else {
                chargeAnimation(currentTime); // Ladeanimation ausführen
            }
        }
    } else {
        isCharging = false;
    }
}

// Weckt das System aus dem Schlafmodus auf
void wakeUp() {
    sleepMode = false; // Deaktiviert den Schlafmodus
    lastActivityTime = millis(); // Setzt den Inaktivitäts-Timer zurück
    ring.clear();
    ring.show();
}

// Versetzt das System in den Schlafmodus
void enterSleepMode() {
    sleepMode = true; // Aktiviert den Schlafmodus
    ring.clear();
    ring.show();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Energiesparmodus setzen
    sleep_enable();
    sleep_mode();
}

// Prüft, ob eine lange Berührung erkannt wurde
bool longPressDetected() {
    unsigned long pressStart = millis(); // Speichert den Startzeitpunkt der Berührung
    while (digitalRead(TOUCH_PIN) == HIGH) { // Solange die Berührung anhält
        if (millis() - pressStart > LONG_PRESS_TIME) { // Falls die Zeit überschritten wurde
            return true; // Langer Tastendruck erkannt
        }
    }
    return false; // Kein langer Tastendruck
}

// Startet die Animation zur Batterieanzeige
void startBatteryAnimation() {
    int battLevel = analogRead(BATT_LEVEL); // Liest den aktuellen Batteriestand aus
    targetBatteryLeds = map(battLevel, 0, 1023, 0, NUM_LEDS); // Skaliert den Wert auf die LED-Anzahl
    batteryAnimStep = 0; // Setzt die Animation auf den Anfang
    batteryAnimRunning = true; // Startet die Animation
    lastBatteryAnimTime = millis(); // Speichert die aktuelle Zeit
}

// Aktualisiert die Batterieanimation
void updateBatteryAnimation(unsigned long currentTime) {
    if (currentTime - lastBatteryAnimTime >= BATTERY_ANIM_INTERVAL) { // Falls das Intervall überschritten ist
        lastBatteryAnimTime = currentTime; // Aktualisiert den Timer
        ring.clear();
        ring.show();
        batteryAnimStep++; // Nächster Schritt in der Animation

        if (batteryAnimStep >= NUM_LEDS + targetBatteryLeds) { // Falls die Animation abgeschlossen ist
            batteryAnimRunning = false; // Beende die Animation
        }
    }
}

// Verarbeitet den Vergleichsmodus basierend auf den Lichtsensoren
void handleCompareMode() {
    int ldr1 = analogRead(LDR1); // Liest den Wert des ersten Lichtsensors
    int ldr2 = analogRead(LDR2); // Liest den Wert des zweiten Lichtsensors
    int ldr3 = analogRead(LDR3); // Liest den Wert des dritten Lichtsensors

    // Berechnet die absoluten Unterschiede zwischen den Sensorwerten
    int diff1 = abs(ldr1 - ldr2);
    int diff2 = abs(ldr2 - ldr3);
    int diff3 = abs(ldr1 - ldr3);

    // Bestimmt die größte Differenz zwischen den Sensorwerten
    int maxDiff = max(diff1, max(diff2, diff3));

    ring.clear(); // Löscht die aktuellen LED-Farben

    uint32_t color; // Variable zur Speicherung der Farbe

    // Setzt die LED-Farbe basierend auf der maximalen Differenz
    if (maxDiff < 50) {
        color = ring.Color(0, 0, 255); // Blau: Kaum Unterschiede im Licht
    } else if (maxDiff < 200) {
        color = ring.Color(0, 255, 0); // Grün: Mittlere Unterschiede
    } else {
        color = ring.Color(255, 0, 0); // Rot: Große Unterschiede
    }

    // Setzt alle LEDs auf die berechnete Farbe
    for (int i = 0; i < NUM_LEDS; i++) {
        ring.setPixelColor(i, color);
    }
    ring.show(); // Zeigt die Farben an
}

// Blinkt die LEDs in Gelb, wenn die Batterie voll geladen ist
void blinkNeonYellow(unsigned long currentTime) {
    // Prüft, ob das Blinkintervall überschritten wurde
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
        lastBlinkTime = currentTime; // Aktualisiert den letzten Blinkzeitpunkt
        blinkState = !blinkState; // Wechselt zwischen An und Aus

        if (blinkState) {
            // Setzt alle LEDs auf Gelb (255, 255, 0)
            for (int i = 0; i < NUM_LEDS; i++) {
                ring.setPixelColor(i, ring.Color(255, 255, 0));
            }
        } else {
            ring.clear(); // Löscht alle LEDs
        }

        ring.show(); // Zeigt die Änderung an
    }
}

// Erstellt eine Ladeanimation für die LEDs
void chargeAnimation(unsigned long currentTime) {
    // Prüft, ob das Animationsintervall überschritten wurde
    if (currentTime - lastChargeTime >= CHARGE_INTERVAL) {
        lastChargeTime = currentTime; // Aktualisiert den letzten Animationszeitpunkt
        ring.clear(); // Löscht den aktuellen LED-Zustand

        // Erstellt eine gelbe Wanderanimation durch den LED-Ring
        for (int i = 0; i < 4; i++) { // Vier LEDs werden nacheinander beleuchtet
            int index = (chargePosition + i) % NUM_LEDS; // Berechnet die LED-Position
            ring.setPixelColor(index, ring.Color(255, 255, 0)); // Setzt die Farbe auf Gelb
        }

        ring.show(); // Zeigt die Änderungen an
        chargePosition = (chargePosition + 1) % NUM_LEDS; // Bewegt die Animation weiter
    }

    // Falls das Gerät nicht mehr lädt, werden die LEDs ausgeschaltet
    if (isCharging == false) { // Hier war vorher ein Fehler: "isCharging = false" wurde als Zuweisung statt Vergleich verwendet
        ring.clear(); // Schaltet alle LEDs aus
    }

    ring.show(); // Aktualisiert den LED-Ring
}


