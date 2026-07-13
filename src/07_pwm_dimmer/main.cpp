// Exercise 07 — Two-Button PWM Dimmer
//
// PWM output:  GPIO 2   (red LED — dimmed via ESP32 LEDC peripheral)
// Button A:    GPIO 4   → each press adds 25 to brightness (max 255)
// Button B:    GPIO 5   → each press subtracts 25 from brightness (min 0)
//
// Both buttons use the internal pull-up, so PRESSED reads LOW.
//
// ─── APPROACH — the real-world version ──────────────────────────────────────
//
// This is written the way you'd write it for actual ESP32 hardware in a
// project that has to do more than blink one LED. No blocking `delay()`, no
// per-button copy-paste of debounce state, no shortcuts.
//
// 1) PWM VIA LEDC.
//    The ESP32 has no `analogWrite`. Its LEDC peripheral exposes 16 hardware
//    PWM channels. Setup is three calls:
//        ledcSetup(channel, freq, resolution)
//        ledcAttachPin(pin, channel)
//        ledcWrite(channel, duty)
//    (Arduino-ESP32 v3.x collapses these into `ledcAttach()` /
//     `ledcWrite(pin, duty)`. We use the v2.x API because that's what
//     PlatformIO's stock `platform = espressif32` ships. If you bump the
//     platform, expect a small migration.)
//
// 2) 8-BIT RESOLUTION AT 5 kHz.
//    Duty range 0..255 (matches the spec exactly). 5 kHz is well above the
//    eye's flicker threshold and well below the LEDC hardware ceiling at
//    8-bit resolution.
//
// 3) NON-BLOCKING DEBOUNCE — the real reason this rewrite exists.
//    A physical pushbutton "bounces": for a few ms after a press the
//    contacts chatter, and naive edge detection would see multiple presses.
//    The lazy fix is `delay(50)` after registering a press — but delay()
//    freezes the entire CPU. That means:
//      - a second button being pressed during that 50 ms is missed
//      - a status LED that's supposed to blink stutters
//      - a sensor that needs sampling misses samples
//    The moment your project has more than one moving part, blocking
//    debounce falls apart.
//
//    The right pattern uses `millis()` timestamps and three state variables
//    PER BUTTON:
//      • lastReading   — raw pin state on the previous loop iteration
//                        (used to detect that the signal CHANGED, real or
//                        bouncy)
//      • state         — the "committed" stable state (used to decide
//                        whether to fire — edge detection against `state`)
//      • lastChangeMs  — timestamp of the most recent raw change
//
//    Every loop we sample the pin. If it differs from `lastReading`, we
//    restart the timer (bounce chatter keeps restarting it, that's the
//    point). Only once the reading has been stable for DEBOUNCE_MS do we
//    commit it to `state`. A HIGH→LOW commit on `state` is a real press —
//    fire once, and the function returns. loop() never blocks.
//
// 4) ONE STRUCT + ONE FUNCTION PER BUTTON PATTERN.
//    Both buttons need the same debounce logic with different pins and
//    different actions. Encapsulating gives us a `DebouncedButton` struct
//    holding the three state variables, and a `pressed()` function that
//    returns true exactly once per real press. loop() reads:
//        if (pressed(btn_a, now)) setBrightness(brightness + STEP);
//        if (pressed(btn_b, now)) setBrightness(brightness - STEP);
//    Adding a third button is one struct instance and one line in loop().
//
// 5) `constrain()` FOR CLAMPING.
//    The Arduino idiom `constrain(value, lo, hi)` reads cleaner than
//    nested `min(max(...))`. Same result, less noise.

#include <Arduino.h>

// ─── Pins & tuning ──────────────────────────────────────────────────────────

const int LED_PIN     = 2;
const int BTN_A_PIN   = 4;      // brighter
const int BTN_B_PIN   = 5;      // dimmer

const int STEP        = 25;
const int MIN_LEVEL   = 0;
const int MAX_LEVEL   = 255;

const int LEDC_CHANNEL     = 0;      // one of 16 hardware PWM channels
const int LEDC_FREQ_HZ     = 5000;   // 5 kHz — invisible to the eye
const int LEDC_RESOLUTION  = 8;      // 8 bits → duty 0..255

const unsigned long DEBOUNCE_MS = 30;

// ─── Debounced button primitive ─────────────────────────────────────────────

struct DebouncedButton {
    int pin;
    int lastReading            = HIGH;   // raw read, previous iteration
    int state                  = HIGH;   // committed stable state
    unsigned long lastChangeMs = 0;      // when raw signal last changed

    // Explicit constructor is required because C++11 (the default C++ standard
    // for Arduino-ESP32 v2.x) forbids aggregate `{...}` initialization on any
    // struct that has default member initializers. C++14 relaxed that rule,
    // but we don't rely on it. The constructor sets `pin`; the other three
    // members take their inline defaults above.
    DebouncedButton(int p) : pin(p) {}
};

// Non-blocking edge-detected press. Returns true exactly once, on the
// SETTLED falling edge (release → press). Call every loop() with the same
// `now = millis()`.
bool pressed(DebouncedButton &b, unsigned long now) {
    int reading = digitalRead(b.pin);

    // Any raw change (real edge or bounce chatter) resets the timer.
    if (reading != b.lastReading) {
        b.lastChangeMs = now;
        b.lastReading  = reading;
    }

    // Reading has been stable long enough — commit it as the new state.
    if ((now - b.lastChangeMs) > DEBOUNCE_MS && reading != b.state) {
        b.state = reading;
        return (b.state == LOW);   // fire on the press edge only
    }
    return false;
}

// ─── State ──────────────────────────────────────────────────────────────────

int brightness = 0;
DebouncedButton btn_a(BTN_A_PIN);
DebouncedButton btn_b(BTN_B_PIN);

// ─── Helpers ────────────────────────────────────────────────────────────────

void setBrightness(int level) {
    brightness = constrain(level, MIN_LEVEL, MAX_LEVEL);
    ledcWrite(LEDC_CHANNEL, brightness);
    Serial.printf("brightness = %d\n", brightness);
}

// ─── Arduino entry points ───────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    pinMode(btn_a.pin, INPUT_PULLUP);
    pinMode(btn_b.pin, INPUT_PULLUP);

    ledcSetup(LEDC_CHANNEL, LEDC_FREQ_HZ, LEDC_RESOLUTION);
    ledcAttachPin(LED_PIN, LEDC_CHANNEL);

    setBrightness(0);
}

void loop() {
    unsigned long now = millis();

    if (pressed(btn_a, now)) setBrightness(brightness + STEP);
    if (pressed(btn_b, now)) setBrightness(brightness - STEP);
}
