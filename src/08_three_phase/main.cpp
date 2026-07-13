// Exercise 08 — Three-Phase AC Power Simulation
//
// Three LEDs represent the three phases (L1, L2, L3) of a three-phase AC
// power system. Each phase is a sine wave, offset from the next by exactly
// 120° (2π/3 rad) — the same mathematical relationship that lets a real
// three-phase system deliver constant power to a balanced load.
//
// Wiring:
//    L1 (red)    → GPIO 2   → LEDC channel 0
//    L2 (yellow) → GPIO 4   → LEDC channel 1
//    L3 (blue)   → GPIO 5   → LEDC channel 2
//
// The demo runs at 0.5 Hz (2 s per full cycle) so you can watch the peak
// visibly "rotate" from L1 → L2 → L3 → L1. Real grid power runs at 50 Hz
// (Europe/Asia/Africa) or 60 Hz (Americas) — invisible to the eye due to
// persistence of vision, which is precisely why AC lights don't flicker.
// Change SIM_FREQ_HZ to speed the demo up until it blurs.
//
// ─── APPROACH ───────────────────────────────────────────────────────────────
//
// 1) DATA-DRIVEN PHASES.
//    Each phase is described by { pin, channel, offset, name }. Adding a
//    five-phase (72° apart) or dropping to a two-phase Tesla-style system is
//    one array edit — loop() doesn't change.
//
// 2) SHIFTED SINE FOR NON-NEGATIVE BRIGHTNESS.
//    An LED can't emit negative light. Real AC voltage swings ±V_peak; we
//    map it to 0..255 via  brightness = (sin(ωt + φ) + 1) / 2 · 255.
//    Trough = 0, peak = 255, phase relationship preserved. Absolute value
//    would give you a rectified-power visualisation instead — both
//    interesting; this one shows the phase rotation more clearly.
//
// 3) NON-BLOCKING BY CONSTRUCTION.
//    No delay(). Time comes from millis(), we compute brightness from
//    time. loop() runs freely, so serial prints / a button / a future
//    fault-detection routine could all coexist without hiccuping the fades.

#include <Arduino.h>
#include <math.h>

struct Phase {
    int   pin;
    int   channel;
    float offset;     // radians, relative to L1
    const char* name;
};

const Phase phases[] = {
    { 2, 0,  0.0f,                    "L1 (R)" },
    { 4, 1, -2.0f * (float)PI / 3.0f, "L2 (Y)" },
    { 5, 2, -4.0f * (float)PI / 3.0f, "L3 (B)" }
};
const int NUM_PHASES = sizeof(phases) / sizeof(phases[0]);

const int LEDC_FREQ_HZ    = 5000;
const int LEDC_RESOLUTION = 8;

// Simulation frequency — slow so the human eye can see phase rotation.
// Bump toward 50–60 Hz for realism (LEDs will then look steady).
const float SIM_FREQ_HZ = 0.5f;

void setup() {
    Serial.begin(115200);
    for (int i = 0; i < NUM_PHASES; i++) {
        ledcSetup(phases[i].channel, LEDC_FREQ_HZ, LEDC_RESOLUTION);
        ledcAttachPin(phases[i].pin, phases[i].channel);
    }
    Serial.printf("3-phase demo: %d phases at %.2f Hz\n", NUM_PHASES, SIM_FREQ_HZ);
}

void loop() {
    float t  = millis() / 1000.0f;
    float wt = 2.0f * (float)PI * SIM_FREQ_HZ * t;

    for (int i = 0; i < NUM_PHASES; i++) {
        float v    = (sinf(wt + phases[i].offset) + 1.0f) * 0.5f;
        int   duty = (int)(v * 255.0f);
        ledcWrite(phases[i].channel, duty);
    }
}
