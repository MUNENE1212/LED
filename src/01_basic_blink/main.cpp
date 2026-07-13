// Exercise 01 — Basic Blink
// Steady 1 Hz blink: 1 s on, 1 s off, forever. No serial output.

#include <Arduino.h>

#define LED_PIN 2        // ← Line 1

void setup() {           // ← Line 2: runs ONCE at startup
    pinMode(LED_PIN, OUTPUT);  // ← Line 3
}

void loop() {            // ← Line 4: runs FOREVER
    digitalWrite(LED_PIN, HIGH);  // ← Line 5: LED ON
    delay(1000);                 // ← Line 6: wait 1000ms = 1 second
    digitalWrite(LED_PIN, LOW);   // ← Line 7: LED OFF
    delay(1000);                 // ← Line 8: wait 1 second
}                               // ← loop() RESTARTS immediately
