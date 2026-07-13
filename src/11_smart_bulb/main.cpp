// Exercise 11 — Smart LED Bulb
//
// A multi-mode LED bulb that cycles to the NEXT mode every time you power
// it off and on again. Same trick real "no-app-needed" smart bulbs use —
// no controller, no wifi, no phone. Just the wall switch.
//
// HOW THE POWER-CYCLE TRICK WORKS
//   The ESP32 has NVS (Non-Volatile Storage) — a small flash region that
//   survives power loss. We stash a "next mode" index there. On every boot:
//     1. Read `next_mode` from NVS  → that's the mode we run NOW.
//     2. Immediately write `next_mode + 1` back to NVS.
//     3. If power stays on for COMMIT_MS (3 s), overwrite NVS with the
//        CURRENT mode → mode is "locked" and future power-ons stay put.
//   Quick off-on-off-on rotates through modes. Leave it on and it settles.
//
// HARDWARE (see diagram.json)
//   Six LEDs arranged in a hex around the "bulb" — interleaved R,G,B,R,G,B
//   so patterns that walk around the ring cycle color naturally.
//     pos 0 : R → GPIO 2   ch 0
//     pos 1 : G → GPIO 5   ch 1
//     pos 2 : B → GPIO 19  ch 2
//     pos 3 : R → GPIO 4   ch 3
//     pos 4 : G → GPIO 18  ch 4
//     pos 5 : B → GPIO 21  ch 5
//
// TESTING IN WOKWI
//   Stop the simulator = "power off". Start it = "power on". Do it quickly
//   (< 3 s) to advance modes. Do it after a longer run to stay put.

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

// ─── NVS persistence ────────────────────────────────────────────────────────
Preferences prefs;
const char* PREF_NAMESPACE = "bulb";
const char* PREF_KEY       = "next";
const unsigned long COMMIT_MS = 3000;   // stable-on time before mode is locked

int  currentMode   = 0;
bool modeCommitted = false;

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
                float b = fmaxf(0.0f, 1.0f - dist);        // triangular blob width = 1 LED
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

    // 2) Read the mode NVS says to run this boot.
    prefs.begin(PREF_NAMESPACE, false);                    // false = read/write
    currentMode = prefs.getInt(PREF_KEY, 0);               // default 0 on first boot
    if (currentMode < 0 || currentMode >= NUM_MODES) currentMode = 0;

    // 3) Immediately write "next mode" back. If power gets cut RIGHT NOW,
    //    the next boot runs the mode after this one.
    int nextMode = (currentMode + 1) % NUM_MODES;
    prefs.putInt(PREF_KEY, nextMode);

    Serial.printf("Boot: mode %d (%s). Quick power-cycle → mode %d (%s).\n",
                  currentMode, modeName[currentMode],
                  nextMode,    modeName[nextMode]);
    Serial.printf("Stay powered for %lu ms to lock this mode.\n", COMMIT_MS);
}

void loop() {
    unsigned long now = millis();

    // After COMMIT_MS of stable power, overwrite NVS with the CURRENT mode
    // so power-cycles from here don't advance. This is what makes "leave
    // the bulb on" behave sensibly.
    if (!modeCommitted && now >= COMMIT_MS) {
        prefs.putInt(PREF_KEY, currentMode);
        modeCommitted = true;
        Serial.printf("Mode %d (%s) locked.\n", currentMode, modeName[currentMode]);
    }

    runMode((Mode)currentMode, now);
}
