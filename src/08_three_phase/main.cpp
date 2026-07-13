// Exercise 08 — Three-Phase AC Power Simulation
//
// Three LEDs represent the phases (L1, L2, L3) of an AC three-phase system.
// Each is a sine wave 120° (2π/3) apart. Watch the peak rotate L1→L2→L3→L1.
//
// Wiring:  L1 red → GPIO 2   L2 yellow → GPIO 4   L3 blue → GPIO 5
// Real grid runs at 50/60 Hz — invisible to the eye. We run slow on purpose.

#include <Arduino.h>
#include <math.h>

struct Phase {
    int   pin;         // GPIO driving this phase's LED
    int   channel;     // LEDC hardware PWM channel (0..15)
    float offset;      // phase shift, radians (0 = reference)
    const char* name;  // label used only for logs
};

const Phase phases[] = {
    { 2, 0,  0.0f,                    "L1 (R)" },   // reference phase
    { 4, 1, -2.0f * (float)PI / 3.0f, "L2 (Y)" },   // 120° behind L1
    { 5, 2, -4.0f * (float)PI / 3.0f, "L3 (B)" }    // 240° behind L1
};
const int NUM_PHASES = sizeof(phases) / sizeof(phases[0]);  // count from array, never desync

const int   LEDC_FREQ_HZ    = 5000;    // PWM carrier — well above eye flicker
const int   LEDC_RESOLUTION = 8;       // 8 bits → duty range 0..255
const float SIM_FREQ_HZ     = 0.5f;    // simulated AC frequency — slow so it's visible

void setup() {
    Serial.begin(115200);

    for (int i = 0; i < NUM_PHASES; i++) {                            // one PWM channel per phase
        ledcSetup(phases[i].channel, LEDC_FREQ_HZ, LEDC_RESOLUTION);  // configure BEFORE attach
        ledcAttachPin(phases[i].pin, phases[i].channel);              // route channel to GPIO
    }

    Serial.printf("3-phase demo: %d phases at %.2f Hz\n", NUM_PHASES, SIM_FREQ_HZ);
}

void loop() {
    float t  = millis() / 1000.0f;                    // ms → seconds (float keeps sub-ms precision)
    float wt = 2.0f * (float)PI * SIM_FREQ_HZ * t;    // seconds → radians:  ω·t = 2π·f·t

    for (int i = 0; i < NUM_PHASES; i++) {                             // walk every phase
        float v    = (sinf(wt + phases[i].offset) + 1.0f) * 0.5f;      // sin [-1,+1] → [0,1]
        int   duty = (int)(v * 255.0f);                                // [0,1] → duty [0,255]
        ledcWrite(phases[i].channel, duty);                            // push new brightness to hardware
    }
    // no delay() — brightness comes from millis(), so other work here won't disturb the fade
}
