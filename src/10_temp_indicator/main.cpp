// Exercise 10 — Temperature Indicator
//
// RGB LED shows a "temperature" band:
//   < 20 °C → Blue   20–25 → Green   25–30 → Yellow   > 30 → Red + flash
// No sensor — TEST_TEMPS cycle every 3 s so every band gets exercised.
// Wiring: R → GPIO 4, G → GPIO 5, B → GPIO 18 (LEDC ch 0/1/2), COM → GND.

#include <Arduino.h>

struct RgbChannel { int pin; int channel; };
const RgbChannel R = { 4,  0 };
const RgbChannel G = { 5,  1 };
const RgbChannel B = { 18, 2 };

const int LEDC_FREQ_HZ    = 5000;    // PWM carrier — above eye flicker
const int LEDC_RESOLUTION = 8;       // 8 bits → duty 0..255

enum Band { COLD, COMFORTABLE, WARM, HOT };
const char* bandName[] = { "COLD", "COMFORTABLE", "WARM", "HOT" };

const float TEST_TEMPS[]     = { 15.0, 22.0, 27.0, 32.0 };           // one per band
const int   NUM_TEMPS        = sizeof(TEST_TEMPS) / sizeof(TEST_TEMPS[0]);
const unsigned long TEST_MS  = 3000;                                 // dwell per test value

Band bandOf(float t) {                                               // classify — pure fn
    if (t < 20) return COLD;
    if (t < 25) return COMFORTABLE;
    if (t < 30) return WARM;
    return HOT;
}

void setupChannel(const RgbChannel &ch) {
    ledcSetup(ch.channel, LEDC_FREQ_HZ, LEDC_RESOLUTION);
    ledcAttachPin(ch.pin, ch.channel);
}

void writeRGB(int r, int g, int b) {                                 // set 3 channels at once
    ledcWrite(R.channel, r);
    ledcWrite(G.channel, g);
    ledcWrite(B.channel, b);
}

void showBand(Band b, unsigned long now) {                           // render — depends on band + time
    switch (b) {
        case COLD:        writeRGB(  0,   0, 255); break;
        case COMFORTABLE: writeRGB(  0, 255,   0); break;
        case WARM:        writeRGB(255, 255,   0); break;
        case HOT: {
            bool on = (now % 500) < 250;                             // 2 Hz square, 50% duty — non-blocking flash
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

    int   idx  = (now / TEST_MS) % NUM_TEMPS;                        // which test value now
    float temp = TEST_TEMPS[idx];
    Band  band = bandOf(temp);

    showBand(band, now);

    static int lastIdx = -1;                                         // log only when the test value flips
    if (idx != lastIdx) {
        Serial.printf("test temp = %.1f °C  →  %s\n", temp, bandName[band]);
        lastIdx = idx;
    }
}
