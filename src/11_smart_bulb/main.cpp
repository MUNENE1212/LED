// Exercise 11 — Smart LED Bulb
//
// 6 LEDs (2R+2G+2B) in an interleaved R,G,B,R,G,B hex. Two controls:
//   • MODE button (GPIO 22): press ON→OFF (mode stays) or OFF→ON (advance)
//   • Power-cycle: also advances mode; stable 3 s locks the current one
// Mode index persists in NVS (Preferences).

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

// ─── LED layout ─────────────────────────────────────────────────────────────
enum Color { R, G, B };
struct Led { int pin; int channel; Color color; };

const Led leds[] = {
    { 2,  0, R },   // pos 0
    { 5,  1, G },   // pos 1
    { 19, 2, B },   // pos 2
    { 4,  3, R },   // pos 3
    { 18, 4, G },   // pos 4
    { 21, 5, B }    // pos 5
};
const int NUM_LEDS = sizeof(leds) / sizeof(leds[0]);   // count derived from array

const int LEDC_FREQ_HZ    = 5000;    // PWM carrier — above eye flicker
const int LEDC_RESOLUTION = 8;       // 8 bits → duty 0..255

// ─── Modes ──────────────────────────────────────────────────────────────────
enum Mode {
    MODE_WARM_WHITE = 0,
    MODE_COOL_WHITE,
    MODE_RED,
    MODE_BLUE,
    MODE_BREATHING,
    MODE_RAINBOW_CHASE,
    NUM_MODES                        // sentinel — gives us mode count
};

const char* modeName[] = {
    "Warm White", "Cool White", "Red", "Blue", "Breathing", "Rainbow Chase"
};

// ─── Button (non-blocking debounce, from Ex 07) ─────────────────────────────
const int MODE_BUTTON_PIN       = 22;
const unsigned long DEBOUNCE_MS = 30;

struct DebouncedButton {
    int pin;
    int lastReading            = HIGH;      // raw read, previous iteration
    int state                  = HIGH;      // committed stable state
    unsigned long lastChangeMs = 0;         // when raw signal last changed
    DebouncedButton(int p) : pin(p) {}      // C++11 needs explicit ctor here
};

bool pressed(DebouncedButton &b, unsigned long now) {
    int reading = digitalRead(b.pin);
    if (reading != b.lastReading) {                       // raw edge (real or bounce)
        b.lastChangeMs = now;                             // reset stability timer
        b.lastReading  = reading;
    }
    if ((now - b.lastChangeMs) > DEBOUNCE_MS              // stable long enough
        && reading != b.state) {                          // AND state changed
        b.state = reading;
        return (b.state == LOW);                          // fire only on press edge
    }
    return false;
}

DebouncedButton modeButton(MODE_BUTTON_PIN);

// ─── NVS-backed mode state ──────────────────────────────────────────────────
Preferences prefs;
const char* PREF_NAMESPACE      = "bulb";
const char* PREF_KEY            = "next";
const unsigned long COMMIT_MS   = 3000;    // stable time before mode locks

int  currentMode       = 0;
bool modeCommitted     = false;
bool bulbOn            = true;             // button toggles this; boot = ON
unsigned long commitStartMs = 0;           // start of current lock countdown

// Advance mode + write "next mode" to NVS + restart lock timer.
// Used by boot logic AND button (OFF→ON transition).
void advanceModeAndPersist() {
    currentMode = (currentMode + 1) % NUM_MODES;
    prefs.putInt(PREF_KEY, (currentMode + 1) % NUM_MODES);  // "next next" for quick cycle
    modeCommitted = false;
    commitStartMs = millis();
    Serial.printf("→ mode %d (%s)\n", currentMode, modeName[currentMode]);
}

// ─── LED helpers ────────────────────────────────────────────────────────────
void writeRGB(int r, int g, int b) {                      // set every LED by color role
    for (int i = 0; i < NUM_LEDS; i++) {
        int val = (leds[i].color == R) ? r
                : (leds[i].color == G) ? g
                                       : b;
        ledcWrite(leds[i].channel, val);
    }
}

void writePosition(int pos, int val) {                    // one LED by index
    ledcWrite(leds[pos].channel, val);
}

// ─── Mode implementations ───────────────────────────────────────────────────
void runMode(Mode m, unsigned long now) {
    switch (m) {
        case MODE_WARM_WHITE:  writeRGB(255, 150,  40); break;   // ~2700 K
        case MODE_COOL_WHITE:  writeRGB(150, 200, 255); break;   // ~6500 K
        case MODE_RED:         writeRGB(255,   0,   0); break;
        case MODE_BLUE:        writeRGB(  0,   0, 255); break;

        case MODE_BREATHING: {                            // white pulse, 4 s cycle
            float phase = 2.0f * (float)PI * (now % 4000) / 4000.0f;
            int   level = (int)((sinf(phase) + 1.0f) * 0.5f * 255.0f);
            writeRGB(level, level, level);
            break;
        }

        case MODE_RAINBOW_CHASE: {                        // blob circles ring in 1.2 s
            float pos = (now % 1200) / 1200.0f * NUM_LEDS;
            for (int i = 0; i < NUM_LEDS; i++) {
                float dist = fabsf(i - pos);
                if (dist > NUM_LEDS / 2.0f) dist = NUM_LEDS - dist;  // ring wrap
                float b = fmaxf(0.0f, 1.0f - dist);       // triangular blob
                writePosition(i, (int)(b * 255.0f));
            }
            break;
        }

        default: writeRGB(0, 0, 0);
    }
}

// ─── Arduino entry points ───────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    for (int i = 0; i < NUM_LEDS; i++) {                          // configure 6 PWM channels
        ledcSetup(leds[i].channel, LEDC_FREQ_HZ, LEDC_RESOLUTION);
        ledcAttachPin(leds[i].pin, leds[i].channel);
    }

    pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);                       // button = active LOW

    prefs.begin(PREF_NAMESPACE, false);                           // false = read/write
    currentMode = prefs.getInt(PREF_KEY, 0);                      // mode to run NOW
    if (currentMode < 0 || currentMode >= NUM_MODES) currentMode = 0;

    int nextMode = (currentMode + 1) % NUM_MODES;
    prefs.putInt(PREF_KEY, nextMode);                             // quick power-cycle → advance
    commitStartMs = millis();                                     // start lock countdown

    Serial.printf("Boot: mode %d (%s). Quick cycle → %d (%s).\n",
                  currentMode, modeName[currentMode],
                  nextMode,    modeName[nextMode]);
    Serial.printf("Press MODE (GPIO %d): ON↔OFF; OFF→ON advances mode.\n",
                  MODE_BUTTON_PIN);
    Serial.printf("Stay stable %lu ms to lock the current mode.\n", COMMIT_MS);
}

void loop() {
    unsigned long now = millis();

    if (pressed(modeButton, now)) {                               // button edge?
        if (bulbOn) {
            bulbOn = false;                                       // ON → OFF (mode unchanged)
            Serial.println("→ OFF");
        } else {
            bulbOn = true;                                        // OFF → ON + advance mode
            advanceModeAndPersist();
        }
    }

    if (!modeCommitted && (now - commitStartMs) >= COMMIT_MS) {   // stable → lock in NVS
        prefs.putInt(PREF_KEY, currentMode);
        modeCommitted = true;
        Serial.printf("Mode %d (%s) locked.\n", currentMode, modeName[currentMode]);
    }

    if (bulbOn) runMode((Mode)currentMode, now);                  // render current mode
    else        writeRGB(0, 0, 0);                                // OFF = all dark
}
