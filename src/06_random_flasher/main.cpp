// Exercise 06 — Random Flasher
//
// Pattern:
//   1. Pick a random LED from the array.
//   2. Flash it 3 times (100 ms ON, 100 ms OFF).
//   3. Pause 500 ms.
//   4. Repeat with a new random LED.
//
// LEDs used: pins 2 (red), 4 (yellow), 5 (green), 18 (blue).
//
// ─── APPROACH — two things worth knowing ────────────────────────────────────
//
// 1) SEEDING THE RANDOM GENERATOR.
//    Arduino's random() is a *pseudo* random number generator: it produces
//    a deterministic sequence based on its internal state. If you never
//    seed it, that sequence is the SAME every time the program starts —
//    the flash order would be identical after every reset, which defeats
//    the whole "random" idea.
//
//    Standard trick: seed it from analogRead() on an unconnected input pin.
//    A floating input picks up electrical noise that varies bit-to-bit
//    every boot, giving us a different seed → a different sequence each run.
//
//    We use GPIO 34 as the seed pin — on the ESP32 that's an input-only ADC
//    pin, perfect because it's unlikely to be wired to anything.
//
// 2) `random(min, max)` IS HALF-OPEN.
//    random(0, NUM_LEDS) returns a value in the range [0, NUM_LEDS) —
//    minimum inclusive, maximum EXCLUSIVE. So for a 4-element array it
//    returns 0, 1, 2, or 3 — exactly the valid indices. If we passed
//    NUM_LEDS - 1 as the max we'd never pick the last LED.
//
// ─── Design of the flash helper ─────────────────────────────────────────────
//    We factored the "flash a pin N times" pattern into its own function.
//    loop() reads at a higher level ("pick, flash, pause") without the
//    reader having to parse the flashing details every iteration. Small
//    function = one job, easy to reuse if a future exercise needs it.

#include <Arduino.h>

const int LED_PINS[]    = {2, 4, 5, 18};
const int NUM_LEDS      = sizeof(LED_PINS) / sizeof(LED_PINS[0]);
const int FLASH_ON_MS   = 100;
const int FLASH_OFF_MS  = 100;
const int FLASH_COUNT   = 3;
const int PAUSE_MS      = 500;
const int SEED_PIN      = 34;   // input-only ADC pin, left floating for noise

void flash(int pin, int times, int on_ms, int off_ms) {
    for (int i = 0; i < times; i++) {
        digitalWrite(pin, HIGH);
        delay(on_ms);
        digitalWrite(pin, LOW);
        delay(off_ms);
    }
}

void setup() {
    Serial.begin(115200);
    for (int i = 0; i < NUM_LEDS; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }
    randomSeed(analogRead(SEED_PIN));   // different sequence every boot
}

void loop() {
    int idx = random(0, NUM_LEDS);      // [0, NUM_LEDS) — max is exclusive
    int pin = LED_PINS[idx];

    Serial.print("Flashing GPIO ");
    Serial.println(pin);

    flash(pin, FLASH_COUNT, FLASH_ON_MS, FLASH_OFF_MS);
    delay(PAUSE_MS);
}
