// Exercise 03 — Sequential LEDs
// Three LEDs on GPIO 4, 5, 18.
// Turn ON one at a time: 4 → 5 → 18, then OFF one at a time in the same order.
// 200 ms between each step.
//
// ─── APPROACH — why the code is shaped this way ──────────────────────────────
//
// 1) PINS LIVE IN AN ARRAY.
//    Ex 02 had one LED and used  #define LED_PIN 2 .  With three LEDs the
//    obvious next step is LED1_PIN / LED2_PIN / LED3_PIN + three copies of
//    the same digitalWrite block. That works, but every new LED means a copy-
//    paste. An array lets ONE loop drive all the pins — and adding a 4th LED
//    later becomes a one-character change (add "27," to the array), not a
//    copy-paste of a whole code block.
//
// 2) `const int`, NOT `#define`.
//    #define is a text substitution done by the preprocessor before the compiler
//    even sees the code — it has no type, no scope, and doesn't show up in the
//    debugger. `const int` is a real typed constant the compiler knows about.
//    Same performance, safer, and errors point at real code instead of a macro.
//
// 3) NUM_LEDS IS COMPUTED, NOT HARDCODED.
//    `sizeof(LED_PINS) / sizeof(LED_PINS[0])` asks the compiler at build time:
//    "how big is the whole array divided by the size of one element?" = the
//    element count. If we hardcoded  const int NUM_LEDS = 3;  and later added
//    a 4th pin, we might forget to bump the count and the loop would silently
//    skip the last LED. Computing it can never go out of sync with the array.
//
// 4) TWO SEPARATE LOOPS — one for ON, one for OFF.
//    If we put ON→delay→OFF→delay inside a single loop, each LED would flash
//    briefly on and then off before the next started — a *chase*, not a
//    cascade. The exercise asks for "all lights come on one by one, then all
//    lights go off one by one." Two passes over the same array give us that.
//
// 5) EXPLICIT LOW IN setup().
//    Right after pinMode(OUTPUT), the pin's actual level is not guaranteed on
//    every chip. Writing LOW immediately forces a known starting state before
//    loop() runs so the very first ON is a clean edge.
//
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>

// Add or remove pins here — NUM_LEDS updates itself, no other line has to change.
const int LED_PINS[]     = {2,4, 5, 18};
const int NUM_LEDS       = sizeof(LED_PINS) / sizeof(LED_PINS[0]);
const int STEP_DELAY_MS  = 200;   // gap between one LED changing and the next

void setup() {
    Serial.begin(115200);

    // Configure every pin as an output AND force it LOW so we start from a
    // known "all off" state before loop() begins.
    for (int i = 0; i < NUM_LEDS; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }
}

void loop() {
    // Pass 1 — cascade ON in order 4 → 5 → 18.
    // Notice we only write HIGH here; we never touch previous LEDs, so LEDs
    // that already turned on stay on while the next one lights up.
    for (int i = 0; i < NUM_LEDS; i++) {
        digitalWrite(LED_PINS[i], HIGH);
        Serial.print("ON  GPIO ");
        Serial.println(LED_PINS[i]);
        delay(STEP_DELAY_MS);
    }

    // Pass 2 — cascade OFF in the same order (4 first, then 5, then 18).
    // Exact same loop structure as Pass 1; the only thing that changes is the
    // value we write. That symmetry is the "one pattern, different value"
    // point mentioned in the approach note above.
    for (int i = 0; i < NUM_LEDS; i++) {
        digitalWrite(LED_PINS[i], LOW);
        Serial.print("OFF GPIO ");
        Serial.println(LED_PINS[i]);
        delay(STEP_DELAY_MS);
    }
}
