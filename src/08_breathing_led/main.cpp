// Exercise 08 — Breathing LED
//
// Smooth non-blocking fade on GPIO 2 (red LED):
//    0 → 255 over 2 s (fade in)
//    255 → 0 over 2 s (fade out)
//    repeat forever.
//
// ─── APPROACH ───────────────────────────────────────────────────────────────
//
// No delay(). No blocking loops. Instead we COMPUTE the current brightness
// from the wall-clock: `millis() % CYCLE_MS` tells us where we are in the
// current 4-second cycle. First half → fading in, second half → fading out.
//
// Why this pattern beats a naive `for (int i=0..255) { delay(8); }` loop:
//   • loop() stays free to do OTHER work during the fade — a button read, a
//     serial print, a status LED. None of that would hiccup even mid-fade.
//   • Timing comes from `millis()`, so the fade takes exactly 2 s regardless
//     of what else the CPU is doing.
//   • Trivial to change: swap `PHASE_MS` and the whole curve retimes.

#include <Arduino.h>

const int LED_PIN         = 2;
const int LEDC_CHANNEL    = 0;
const int LEDC_FREQ_HZ    = 5000;
const int LEDC_RESOLUTION = 8;                    // duty 0..255

const unsigned long PHASE_MS = 2000;              // 2 s per direction
const unsigned long CYCLE_MS = 2 * PHASE_MS;      // 4 s full cycle

void setup() {
    Serial.begin(115200);
    ledcSetup(LEDC_CHANNEL, LEDC_FREQ_HZ, LEDC_RESOLUTION);
    ledcAttachPin(LED_PIN, LEDC_CHANNEL);
}

void loop() {
    unsigned long t = millis() % CYCLE_MS;

    int level = (t < PHASE_MS)
              ? map(t, 0, PHASE_MS - 1, 0, 255)          // fade in
              : map(t, PHASE_MS, CYCLE_MS - 1, 255, 0);  // fade out

    ledcWrite(LEDC_CHANNEL, level);
}
