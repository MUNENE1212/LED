// Exercise 10 — Temperature Indicator
//
// Same RGB LED wiring as Ex 09:  R → GPIO 4, G → GPIO 5, B → GPIO 18.
//
// Bands:
//    <  20 °C  →  Blue           (cold)
//    20–25 °C  →  Green          (comfortable)
//    25–30 °C  →  Yellow         (warm)
//    >  30 °C  →  Red + flash    (hot!)
//
// No sensor in this exercise — we cycle through a set of TEST_TEMPS every
// 3 s so every band gets exercised.
//
// ─── APPROACH ───────────────────────────────────────────────────────────────
//
// Two ideas worth naming:
//
// 1) CLASSIFY-THEN-RENDER.
//    `bandOf(temp)` turns a float into an enum (COLD / COMFORTABLE / WARM /
//    HOT). `showBand(band, now)` turns that enum into an RGB output. Keeping
//    the two responsibilities apart makes each easy to change: swap in a
//    real thermistor read for the test-value cycle, and `bandOf()` still
//    works untouched.
//
// 2) NON-BLOCKING FLASH FOR THE HOT STATE.
//    "Flash" is easy to get wrong: a naive `digitalWrite/delay/digitalWrite/
//    delay` loop freezes everything else. Instead we derive the flash state
//    from `millis() % 500 < 250` — a 2 Hz square wave, 50 % duty, that
//    responds instantly to the temperature dropping out of the hot band.

#include <Arduino.h>

struct RgbChannel { int pin; int channel; };
const RgbChannel R = { 4,  0 };
const RgbChannel G = { 5,  1 };
const RgbChannel B = { 18, 2 };

const int LEDC_FREQ_HZ    = 5000;
const int LEDC_RESOLUTION = 8;

enum Band { COLD, COMFORTABLE, WARM, HOT };
const char* bandName[] = { "COLD", "COMFORTABLE", "WARM", "HOT" };

// Rotate through these every TEST_MS so every band gets shown.
const float TEST_TEMPS[]     = { 15.0, 22.0, 27.0, 32.0 };
const int   NUM_TEMPS        = sizeof(TEST_TEMPS) / sizeof(TEST_TEMPS[0]);
const unsigned long TEST_MS  = 3000;

Band bandOf(float t) {
    if (t < 20) return COLD;
    if (t < 25) return COMFORTABLE;
    if (t < 30) return WARM;
    return HOT;
}

void setupChannel(const RgbChannel &ch) {
    ledcSetup(ch.channel, LEDC_FREQ_HZ, LEDC_RESOLUTION);
    ledcAttachPin(ch.pin, ch.channel);
}

void writeRGB(int r, int g, int b) {
    ledcWrite(R.channel, r);
    ledcWrite(G.channel, g);
    ledcWrite(B.channel, b);
}

void showBand(Band b, unsigned long now) {
    switch (b) {
        case COLD:        writeRGB(  0,   0, 255); break;
        case COMFORTABLE: writeRGB(  0, 255,   0); break;
        case WARM:        writeRGB(255, 255,   0); break;
        case HOT: {
            bool on = (now % 500) < 250;         // 2 Hz, 50 % duty flash
            writeRGB(on ? 255 : 0, 0, 0);
            break;
        }
    }
}

void setup() {
    Serial.begin(115200);
    setupChannel(R);
    setupChannel(G);
    setupChannel(B);
}

void loop() {
    unsigned long now = millis();

    // Pick a test temperature based on time.
    int   idx  = (now / TEST_MS) % NUM_TEMPS;
    float temp = TEST_TEMPS[idx];
    Band  band = bandOf(temp);

    showBand(band, now);

    // Log once per test-value change (only when the index actually flips).
    static int lastIdx = -1;
    if (idx != lastIdx) {
        Serial.printf("test temp = %.1f °C  →  %s\n", temp, bandName[band]);
        lastIdx = idx;
    }
}
