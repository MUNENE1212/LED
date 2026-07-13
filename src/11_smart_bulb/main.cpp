// Exercise 11 — Smart LED Bulb
//
// A multi-mode LED bulb with TWO ways to change mode:
//   1) POWER-CYCLE the ESP32 — the "no-app-needed" smart-bulb trick.
//      Works even with the button disconnected.
//   2) PRESS the MODE button on GPIO 22 — instant advance from wherever
//      you are, without cutting power.
//
// Both mechanisms share the same NVS-backed state so they feel identical:
// press or cycle → next mode; leave it stable for 3 s → mode locks in.
//
// HOW THE POWER-CYCLE TRICK WORKS
//   NVS (Non-Volatile Storage) is a small flash region that survives power
//   loss. We stash a "next mode" index there. Every boot:
//     1. Read `next_mode` from NVS → run THAT mode now.
//     2. Immediately write `next_mode + 1` back to NVS.
//     3. After COMMIT_MS (3 s) of stable operation, overwrite NVS with the
//        CURRENT mode → mode is "locked", future power-ons stay put.
//   Quick off-on-off-on rotates modes. Leave it on and it settles.
//   The button hooks into the SAME persist-then-lock cycle, so a press
//   followed by a quick power-cycle advances from the button-selected mode.
//
// HARDWARE (see diagram.json)
//   Six LEDs in a hex, interleaved R,G,B,R,G,B so patterns walking around
//   the ring cycle color naturally.
//     pos 0 : R → GPIO 2   ch 0
//     pos 1 : G → GPIO 5   ch 1
//     pos 2 : B → GPIO 19  ch 2
//     pos 3 : R → GPIO 4   ch 3
//     pos 4 : G → GPIO 18  ch 4
//     pos 5 : B → GPIO 21  ch 5
//   MODE button → GPIO 22, other terminal to GND (uses internal pull-up).
//
// TESTING IN WOKWI
//   Click MODE = instant mode advance.
//   Stop the sim = "power off", Start = "power on". Do it quickly (<3 s) to
//   advance modes; wait longer to lock the current mode in NVS.

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
const int NUM_LEDS = sizeof(leds) / sizeof(leds[0]);

const int LEDC_FREQ_HZ    = 5000;    // PWM carrier — invisible to the eye
const int LEDC_RESOLUTION = 8;       // 8 bits → duty 0..255

// ─── Modes ──────────────────────────────────────────────────────────────────
enum Mode {
    MODE_WARM_WHITE = 0,
    MODE_COOL_WHITE,
    MODE_RED,
    MODE_BLUE,
    MODE_BREATHING,
    MODE_RAINBOW_CHASE,
    NUM_MODES                        // sentinel — always last, gives us count
};

const char* modeName[] = {
    "Warm White", "Cool White", "Red", "Blue", "Breathing", "Rainbow Chase"
};

// ─── Button (non-blocking debounce, pattern from Ex 07) ─────────────────────
const int MODE_BUTTON_PIN         = 22;
const unsigned long DEBOUNCE_MS   = 30;

struct DebouncedButton {
    int pin;
    int lastReading            = HIGH;
    int state                  = HIGH;
    unsigned long lastChangeMs = 0;
    DebouncedButton(int p) : pin(p) {}
};

bool pressed(DebouncedButton &b, unsigned long now) {
    int reading = digitalRead(b.pin);
    if (reading != b.lastReading) {
        b.lastChangeMs = now;
        b.lastReading  = reading;
    }
    if ((now - b.lastChangeMs) > DEBOUNCE_MS && reading != b.state) {
        b.state = reading;
        return (b.state == LOW);     // fire only on the press edge
    }
    return false;
}

DebouncedButton modeButton(MODE_BUTTON_PIN);

// ─── NVS persistence ────────────────────────────────────────────────────────
Preferences prefs;
const char* PREF_NAMESPACE      = "bulb";
const char* PREF_KEY            = "next";
const unsigned long COMMIT_MS   = 3000;   // stable time before mode locks

int  currentMode       = 0;
bool modeCommitted     = false;
unsigned long commitStartMs = 0;          // when the current lock countdown began

// ─── Mode advance (shared by boot and button) ───────────────────────────────
// Advance the mode, write "next-mode" to NVS so a power-cycle from here
// would move to the mode AFTER this one, and restart the 3 s lock timer.
void advanceModeAndPersist() {
    currentMode = (currentMode + 1) % NUM_MODES;
    int nextMode = (currentMode + 1) % NUM_MODES;
    prefs.putInt(PREF_KEY, nextMode);
    modeCommitted = false;
    commitStartMs = millis();
    Serial.printf("→ mode %d (%s)\n", currentMode, modeName[currentMode]);
}

// ─── Low-level LED helpers ──────────────────────────────────────────────────
void writeRGB(int r, int g, int b) {                       // set all LEDs by color role
    for (int i = 0; i < NUM_LEDS; i++) {
        int val = (leds[i].color == R) ? r
                : (leds[i].color == G) ? g
                                       : b;
        ledcWrite(leds[i].channel, val);
    }
}

void writePosition(int pos, int val) {                     // one specific LED by index
    ledcWrite(leds[pos].channel, val);
}

// ─── Mode implementations ───────────────────────────────────────────────────
void runMode(Mode m, unsigned long now) {
    switch (m) {
        case MODE_WARM_WHITE:
            writeRGB(255, 150, 40);                        // ~2700 K incandescent
            break;

        case MODE_COOL_WHITE:
            writeRGB(150, 200, 255);                       // ~6500 K daylight
            break;

        case MODE_RED:
            writeRGB(255, 0, 0);
            break;

        case MODE_BLUE:
            writeRGB(0, 0, 255);
            break;

        case MODE_BREATHING: {                             // whole bulb pulses white, 4 s cycle
            float phase = 2.0f * (float)PI * (now % 4000) / 4000.0f;
            int   level = (int)((sinf(phase) + 1.0f) * 0.5f * 255.0f);
            writeRGB(level, level, level);
            break;
        }

        case MODE_RAINBOW_CHASE: {                         // blob of light circles the ring
            float pos = (now % 1200) / 1200.0f * NUM_LEDS; // 0..NUM_LEDS over 1.2 s
            for (int i = 0; i < NUM_LEDS; i++) {
                float dist = fabsf(i - pos);
                if (dist > NUM_LEDS / 2.0f) dist = NUM_LEDS - dist;   // wrap around ring
                float b = fmaxf(0.0f, 1.0f - dist);        // triangular blob, width ≈ 1 LED
                writePosition(i, (int)(b * 255.0f));
            }
            break;
        }

        default:                                           // safety net
            writeRGB(0, 0, 0);
    }
}

// ─── Arduino entry points ───────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // 1) Configure the 6 PWM channels.
    for (int i = 0; i < NUM_LEDS; i++) {
        ledcSetup(leds[i].channel, LEDC_FREQ_HZ, LEDC_RESOLUTION);
        ledcAttachPin(leds[i].pin, leds[i].channel);
    }

    // 2) Configure the MODE button — active LOW via internal pull-up.
    pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

    // 3) Read the mode NVS says to run this boot.
    prefs.begin(PREF_NAMESPACE, false);                    // false = read/write
    currentMode = prefs.getInt(PREF_KEY, 0);               // default 0 on first boot
    if (currentMode < 0 || currentMode >= NUM_MODES) currentMode = 0;

    // 4) Immediately write "next mode" back. If power gets cut RIGHT NOW,
    //    the next boot runs the mode after this one.
    int nextMode = (currentMode + 1) % NUM_MODES;
    prefs.putInt(PREF_KEY, nextMode);

    commitStartMs = millis();                              // start the lock countdown

    Serial.printf("Boot: mode %d (%s). Quick power-cycle → mode %d (%s).\n",
                  currentMode, modeName[currentMode],
                  nextMode,    modeName[nextMode]);
    Serial.printf("Press MODE (GPIO %d) or power-cycle to advance.\n", MODE_BUTTON_PIN);
    Serial.printf("Stay stable %lu ms to lock the current mode.\n", COMMIT_MS);
}

void loop() {
    unsigned long now = millis();

    // Button pressed → advance mode instantly + reset lock timer.
    if (pressed(modeButton, now)) {
        advanceModeAndPersist();
    }

    // Timer elapsed and mode not yet locked → commit it.
    if (!modeCommitted && (now - commitStartMs) >= COMMIT_MS) {
        prefs.putInt(PREF_KEY, currentMode);
        modeCommitted = true;
        Serial.printf("Mode %d (%s) locked.\n", currentMode, modeName[currentMode]);
    }

    runMode((Mode)currentMode, now);
}
