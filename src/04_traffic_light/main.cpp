// Exercise 04 — Traffic Light (UK / Europe style, with Red+Yellow transition)
// LEDs:
//   RED    → GPIO 2
//   YELLOW → GPIO 4
//   GREEN  → GPIO 5
//
// Cycle:  RED (5 s) → RED+YELLOW (3 s, "get ready") → GREEN (5 s)
//                                                   → YELLOW (3 s, "slow down") → RED …
//
// ─── APPROACH — why enum + switch instead of if/else chains ─────────────────
//
// A traffic light is a textbook finite state machine: at any moment it's in
// exactly ONE named state, and each state has a specific next state. That
// shape fits an `enum` + `switch` perfectly:
//
//   • enum gives each state a NAME the compiler understands (RED_ONLY, not
//     the magic number 0). Typos become compile errors, not silent bugs.
//   • switch handles one case per state, with the transition written
//     explicitly ("after RED_ONLY, next is RED_YELLOW"). Reads like the
//     spec, not like a puzzle.
//   • Adding a new state later means adding ONE enum value + ONE case, no
//     tangled if/else surgery.

#include <Arduino.h>

const int RED_PIN    = 2;
const int YELLOW_PIN = 4;
const int GREEN_PIN  = 5;

// The four states. Order in the enum doesn't matter — each case explicitly
// picks the next state, we never do `state++`.
enum TrafficState {
    RED_ONLY,
    RED_YELLOW,
    GREEN,
    YELLOW_ONLY
};

enum TrafficState state = RED_ONLY;

// Helper: set all three lights in one call. Cleaner than three digitalWrites
// scattered through each case block.
void setLights(bool r, bool y, bool g) {
    digitalWrite(RED_PIN,    r ? HIGH : LOW);
    digitalWrite(YELLOW_PIN, y ? HIGH : LOW);
    digitalWrite(GREEN_PIN,  g ? HIGH : LOW);
}

void setup() {
    Serial.begin(115200);
    pinMode(RED_PIN,    OUTPUT);
    pinMode(YELLOW_PIN, OUTPUT);
    pinMode(GREEN_PIN,  OUTPUT);
    setLights(false, false, false);  // known start state
}

void loop() {
    switch (state) {
        case RED_ONLY:
            Serial.println("RED 5 Seconds");
            setLights(true, false, false);
            delay(5000);
            state = RED_YELLOW;
            break;

        case RED_YELLOW:
            Serial.println("RED + YELLOW 3 seconds  (get ready)");
            setLights(true, true, false);
            delay(3000);
            state = GREEN;
            break;

        case GREEN:
            Serial.println("GREEN 5 Seconds");
            setLights(false, false, true);
            delay(5000);
            state = YELLOW_ONLY;
            break;

        case YELLOW_ONLY:
            Serial.println("YELLOW 3 Seconds (slow down)");
            setLights(false, true, false);
            delay(3000);
            state = RED_ONLY;
            break;
    }
}
