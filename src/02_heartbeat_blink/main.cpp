// Exercise 02 — Heartbeat Blink
// Pattern: 3 rapid blinks (100 ms on / 100 ms off), then a 1 s pause, repeat.
// New vs. Exercise 01: adds a for-loop for the burst, and Serial output at 115200 baud.

#include <Arduino.h>

#define LED_PIN 2        // ← Line 1

void setup() {           // ← Line 2: runs ONCE at startup
    pinMode(LED_PIN, OUTPUT);  // ← Line 3
    Serial.begin(115200);  // ← Line 3.1: initialize serial communication at 115200 baud
}

void loop() {
  for (int i = 0; i < 3; i++){
    digitalWrite(LED_PIN, HIGH);  // ← Line 5: LED ON
    Serial.println("LED ON 100mS");  // ← Line 5.1: print to serial monitor
    delay(100);                 // ← Line 6: wait 100ms
    digitalWrite(LED_PIN, LOW);   // ← Line 7: LED OFF
    Serial.println("LED OFF 100mS");  // ← Line 7.1: print to serial monitor
    delay(100);
  }
  digitalWrite(LED_PIN, LOW);   // ← Line 7: LED OFF (pause between bursts)
  Serial.println("LED OFF");  // ← Line 7.1: print to serial monitor
  delay(1000);                 // ← Line 8: wait 1 second
}                               // ← loop() RESTARTS immediately
