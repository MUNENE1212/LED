// Exercise 09 — RGB Color Mixer
//
// One RGB LED wired as:
//    R  → GPIO 4   → LEDC channel 0
//    G  → GPIO 5   → LEDC channel 1
//    B  → GPIO 18  → LEDC channel 2
//    COM → GND (common-cathode: driving a channel HIGH lights it)
//
// Cycles through the palette every 2 s:  Red → Green → Blue → Yellow (R+G)
// → Cyan (G+B) → White (R+G+B), then loops.
//
// ─── APPROACH ───────────────────────────────────────────────────────────────
//
// Each color needs its OWN PWM channel — you can't drive three colors from
// one channel because they'd all fade together. So the ESP32's LEDC lets us
// allocate three independent channels, one per pin. Then `ledcWrite(ch,
// duty)` sets each color's brightness independently — pure additive color
// mixing.
//
// Palette is a plain array of {r, g, b, name}. Adding a color = one line.
// Cycling uses `millis()`-based non-blocking timing.

#include <Arduino.h>

struct RgbChannel { int pin; int channel; };
const RgbChannel R = { 4,  0 };
const RgbChannel G = { 5,  1 };
const RgbChannel B = { 18, 2 };

const int LEDC_FREQ_HZ    = 5000;
const int LEDC_RESOLUTION = 8;

struct Color { int r, g, b; const char* name; };

const Color palette[] = {
    { 255,   0,   0, "Red" },
    {   0, 255,   0, "Green" },
    {   0,   0, 255, "Blue" },
    { 255, 255,   0, "Yellow  (R+G)" },
    {   0, 255, 255, "Cyan    (G+B)" },
    { 255, 255, 255, "White   (R+G+B)" }
};
const int NUM_COLORS = sizeof(palette) / sizeof(palette[0]);

const unsigned long COLOR_MS = 2000;

void setupChannel(const RgbChannel &ch) {
    ledcSetup(ch.channel, LEDC_FREQ_HZ, LEDC_RESOLUTION);
    ledcAttachPin(ch.pin, ch.channel);
}

void showColor(const Color &c) {
    ledcWrite(R.channel, c.r);
    ledcWrite(G.channel, c.g);
    ledcWrite(B.channel, c.b);
    Serial.printf("→ %s\n", c.name);
}

void setup() {
    Serial.begin(115200);
    setupChannel(R);
    setupChannel(G);
    setupChannel(B);
    showColor(palette[0]);
}

void loop() {
    static unsigned long lastChange = millis();
    static int idx = 0;

    if (millis() - lastChange >= COLOR_MS) {
        idx = (idx + 1) % NUM_COLORS;
        showColor(palette[idx]);
        lastChange = millis();
    }
}
