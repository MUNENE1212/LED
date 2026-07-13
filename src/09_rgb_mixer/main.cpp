// Exercise 09 — RGB Color Mixer
//
// Cycles Red → Green → Blue → Yellow (R+G) → Cyan (G+B) → White, 2 s each.
// Each channel has its own LEDC — that's what lets us mix colors (drive
// several channels at once at any duty).
// Wiring: R → GPIO 4, G → GPIO 5, B → GPIO 18 (LEDC ch 0/1/2), COM → GND.

#include <Arduino.h>

struct RgbChannel { int pin; int channel; };
const RgbChannel R = { 4,  0 };
const RgbChannel G = { 5,  1 };
const RgbChannel B = { 18, 2 };

const int LEDC_FREQ_HZ    = 5000;    // PWM carrier — above eye flicker
const int LEDC_RESOLUTION = 8;       // 8 bits → duty 0..255

struct Color { int r, g, b; const char* name; };

const Color palette[] = {                                  // add a color = one line
    { 255,   0,   0, "Red" },
    {   0, 255,   0, "Green" },
    {   0,   0, 255, "Blue" },
    { 255, 255,   0, "Yellow  (R+G)" },
    {   0, 255, 255, "Cyan    (G+B)" },
    { 255, 255, 255, "White   (R+G+B)" }
};
const int NUM_COLORS = sizeof(palette) / sizeof(palette[0]);

const unsigned long COLOR_MS = 2000;                       // dwell per color

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
    showColor(palette[0]);                                 // start on first color, no wait
}

void loop() {
    static unsigned long lastChange = millis();            // set once, retained across loop() calls
    static int idx = 0;

    if (millis() - lastChange >= COLOR_MS) {               // dwell elapsed?
        idx = (idx + 1) % NUM_COLORS;
        showColor(palette[idx]);
        lastChange = millis();
    }
}
