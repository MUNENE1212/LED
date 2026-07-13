// Exercise 05 — Binary Counter 0..7 with UP / DOWN Buttons
//
// LEDs represent 3 bits of a binary number:
//   bit 0 (value 1) → GPIO 2   (red)      ← least significant
//   bit 1 (value 2) → GPIO 4   (yellow)
//   bit 2 (value 4) → GPIO 5   (green)    ← most significant
//
// Buttons (wired with internal pull-up, so PRESSED reads LOW):
//   UP   → GPIO 21
//   DOWN → GPIO 22
//
// BEHAVIOR:
//   Every 500 ms we look at the buttons.
//     • UP held only    → count + 1
//     • DOWN held only  → count - 1
//     • both / neither  → no change
//   Count is clamped to [0, 7] — no wrap-around.
//
// ─── APPROACH — a few decisions worth naming ────────────────────────────────
//
// 1) SAMPLING ON A TICK, not edge-detecting each press.
//    Edge detection ("only act when the button just changed from released to
//    pressed") is more code and needs debounce logic. Since the exercise
//    says "count every 500 ms," we just sample the button state on a 500 ms
//    beat. Holding UP => steady 2-per-second count-up. Simpler, and 500 ms
//    is already far longer than any physical bounce.
//
// 2) INPUT_PULLUP + active-LOW buttons.
//    We enable the ESP32's internal pull-up resistor on each button pin.
//    That means the pin idles HIGH (pulled to 3.3 V through the internal
//    resistor). When the button is pressed, it shorts the pin to GND, so we
//    read LOW. This is why the wiring only needs "pin → button → GND" — no
//    external resistor.
//
// 3) BITWISE DISPLAY: `(count >> b) & 1`.
//    To show bit `b` of `count`, we right-shift `count` by `b` positions so
//    bit `b` ends up in the ones place, then AND with 1 to isolate it. Result
//    is 0 or 1 — which happens to be exactly LOW / HIGH. One line replaces
//    an if-else per bit and generalises to any number of bits.
//
// 4) `MAX_COUNT = 7` DERIVED FROM 3 BITS.
//    Three bits can hold values 0..7 (2^3 - 1). We hard-code 7 for clarity;
//    if we added a 4th LED (bit) we'd change the array AND update MAX_COUNT
//    to 15.

#include <Arduino.h>

const int BIT_PINS[]  = {2, 4, 5};   // index 0 = bit 0 (LSB) → LSB LED
const int NUM_BITS    = sizeof(BIT_PINS) / sizeof(BIT_PINS[0]);
const int UP_PIN      = 21;
const int DOWN_PIN    = 22;
const int TICK_MS     = 500;
const int MIN_COUNT   = 0;
const int MAX_COUNT   = 7;           // 3 bits → 0..7

int count = 0;

// Show `value` on the LEDs, one bit per LED.
void showCount(int value) {
    for (int b = 0; b < NUM_BITS; b++) {
        int bit = (value >> b) & 1;              // 0 or 1
        digitalWrite(BIT_PINS[b], bit ? HIGH : LOW);
    }
}

void setup() {
    Serial.begin(115200);
    for (int i = 0; i < NUM_BITS; i++) {
        pinMode(BIT_PINS[i], OUTPUT);
    }
    pinMode(UP_PIN,   INPUT_PULLUP);   // idle HIGH, pressed = LOW
    pinMode(DOWN_PIN, INPUT_PULLUP);
    showCount(count);
}

void loop() {
    bool up_pressed   = (digitalRead(UP_PIN)   == LOW);
    bool down_pressed = (digitalRead(DOWN_PIN) == LOW);

    // Both-or-neither pressed → no change. That prevents fighting each other.
    if (up_pressed && !down_pressed && count < MAX_COUNT) {
        count++;
    } else if (down_pressed && !up_pressed && count > MIN_COUNT) {
        count--;
    }

    Serial.print("count = ");
    Serial.println(count);
    showCount(count);
    delay(TICK_MS);
}
