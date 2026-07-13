// Exercise 12 — 4-Digit 7-Segment Watch
//
// HH:MM on four multiplexed common-cathode digits + colon LED + 3 buttons.
// Buttons: MODE (short=toggle 12/24, long=enter/exit setup), UP/DOWN adjust.
// Setup fields cycle HOUR → MINUTE → TZ via MODE short-press.
// Settings persisted in NVS (24h flag + tz offset). Time starts 12:00 UTC.
//
// Pins: segs a–g = 13,14,27,26,25,33,32; digit cathodes 16..19;
//       colon 21; buttons MODE/UP/DOWN 22/23/5.

#include <Arduino.h>
#include <Preferences.h>              // NVS wrapper — persists user settings

// ─── Pins ───────────────────────────────────────────────────────────────────
const int SEG_PINS[7]   = { 13, 14, 27, 26, 25, 33, 32 };  // a, b, c, d, e, f, g
const int DIGIT_PINS[4] = { 16, 17, 18, 19 };              // left → right cathodes
const int COLON_PIN     = 21;                              // single LED between MM and HH
const int BTN_MODE_PIN  = 22;                              // active LOW (INPUT_PULLUP)
const int BTN_UP_PIN    = 23;
const int BTN_DOWN_PIN  = 5;                               // strapping pin — fine as input

// ─── Segment bit patterns (bit 0 = a, … bit 6 = g) ──────────────────────────
const uint8_t DIGIT_PATTERNS[10] = {                       // canonical 7-seg encodings
    0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,   // 0..4
    0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111    // 5..9
};
const uint8_t PATTERN_BLANK = 0;                           // no segments lit
const uint8_t PATTERN_MINUS = 0b01000000;                  // segment g only

// ─── Mode & user prefs ──────────────────────────────────────────────────────
enum ClockMode { RUN, SETUP_HOUR, SETUP_MINUTE, SETUP_TZ };
const char* modeName[] = { "RUN", "SETUP_HOUR", "SETUP_MINUTE", "SETUP_TZ" };
ClockMode mode = RUN;                                      // current state
bool is24Hour = true;                                      // false → 12h with no leading 0
int  tzOffsetHours = 0;                                    // clamped -12..+14

// ─── Time — tracked as UTC seconds since a "set" anchor ─────────────────────
int32_t utcSecondsAtSet = 12L * 3600;                      // default 12:00 UTC
unsigned long millisAtSet = 0;                             // millis() at last time-set

// ─── NVS ────────────────────────────────────────────────────────────────────
Preferences prefs;                                          // begin() in setup()

// ─── Multiplex state ────────────────────────────────────────────────────────
int  currentDigit          = 0;                            // 0..3, incremented each step
unsigned long lastDigitMs  = 0;                            // last multiplex tick
const int DIGIT_MS         = 4;                            // 4 ms × 4 digits ≈ 60 Hz refresh

// ─── Button with short/long-press ──────────────────────────────────────────
struct Button {
    int pin;
    int lastRaw               = HIGH;                      // raw read, previous iteration
    int state                 = HIGH;                      // committed stable state
    unsigned long lastChangeMs = 0;                        // when raw signal last changed
    unsigned long pressStartMs = 0;                        // when current press began
    bool longFired            = false;                     // long-press already emitted?
    Button(int p) : pin(p) {}                              // C++11 needs explicit ctor
};

const unsigned long DEBOUNCE_MS   = 30;                    // stable time before commit
const unsigned long LONG_PRESS_MS = 1000;                  // hold ≥ 1 s = long press

// Returns 0=none, 1=short (on release), 2=long (fires once while held).
int checkButton(Button &b, unsigned long now) {
    int raw = digitalRead(b.pin);
    if (raw != b.lastRaw) {                                // raw change (edge or bounce)
        b.lastChangeMs = now;                              // reset stability timer
        b.lastRaw = raw;
    }
    if ((now - b.lastChangeMs) > DEBOUNCE_MS               // stable long enough
        && raw != b.state) {                               // AND state actually changed
        b.state = raw;
        if (b.state == LOW) {                              // press edge
            b.pressStartMs = now;                          // start timing the hold
            b.longFired = false;
        } else if (!b.longFired) {                         // release AND long hadn't fired
            return 1;                                      // → short press
        }
    }
    if (b.state == LOW && !b.longFired &&                  // still held, long not yet fired,
        (now - b.pressStartMs) > LONG_PRESS_MS) {          // AND held long enough
        b.longFired = true;                                // fire once, don't repeat
        return 2;                                          // → long press
    }
    return 0;
}

Button btnMode(BTN_MODE_PIN);                              // instances share the same struct
Button btnUp  (BTN_UP_PIN);
Button btnDown(BTN_DOWN_PIN);

// ─── Time helpers ──────────────────────────────────────────────────────────
int32_t currentUtcSeconds(unsigned long now) {
    return utcSecondsAtSet                                 // last "set" anchor
         + (int32_t)((now - millisAtSet) / 1000);          // + seconds since then
}

void setUtcTime(int h, int m, unsigned long now) {         // h, m in [0..23], [0..59]
    utcSecondsAtSet = (int32_t)h * 3600 + (int32_t)m * 60; // new anchor value
    millisAtSet = now;                                     // and the millis() it corresponds to
}

// Local (display) time = UTC + tz, wrapped into 0..1439 minutes.
void getLocalHM(unsigned long now, int &outH, int &outM) {
    int32_t utcMin   = currentUtcSeconds(now) / 60;        // whole UTC minutes
    int32_t localMin = utcMin + (int32_t)tzOffsetHours * 60;
    localMin = ((localMin % 1440) + 1440) % 1440;          // safe mod (handles negatives)
    outH = localMin / 60;
    outM = localMin % 60;
}

// ─── Setup mode value adjustment ───────────────────────────────────────────
void adjustField(int delta, unsigned long now) {
    if (mode == SETUP_TZ) {
        tzOffsetHours += delta;
        if (tzOffsetHours > 14)  tzOffsetHours = 14;       // clamp — most tz range on Earth
        if (tzOffsetHours < -12) tzOffsetHours = -12;
        prefs.putInt("tz", tzOffsetHours);                 // persist immediately
        Serial.printf("TZ %+d\n", tzOffsetHours);
        return;
    }
    int32_t utc = currentUtcSeconds(now);                  // read current UTC h/m
    int h = (utc / 3600) % 24;
    int m = (utc / 60)   % 60;
    if (mode == SETUP_HOUR)   h = (h + delta + 24) % 24;   // wrap 0..23
    if (mode == SETUP_MINUTE) m = (m + delta + 60) % 60;   // wrap 0..59
    setUtcTime(h, m, now);                                 // write back through the anchor
    Serial.printf("UTC %02d:%02d\n", h, m);
}

// ─── Build the 4-digit frame for the current mode/state ────────────────────
void buildFrame(unsigned long now, uint8_t patterns[4], bool blink[4]) {
    for (int i = 0; i < 4; i++) {                          // start with blank frame
        patterns[i] = PATTERN_BLANK;
        blink[i] = false;
    }

    if (mode == SETUP_TZ) {                                // TZ view: sign + right-aligned value
        int a = tzOffsetHours < 0 ? -tzOffsetHours : tzOffsetHours;
        if (tzOffsetHours < 0) {
            patterns[a >= 10 ? 1 : 2] = PATTERN_MINUS;     // '-' slides left when 2 digits wide
        }
        if (a >= 10) patterns[2] = DIGIT_PATTERNS[a / 10]; // tens (only if needed)
        patterns[3] = DIGIT_PATTERNS[a % 10];              // ones — always shown
        blink[2] = blink[3] = true;                        // whole value blinks
        return;
    }

    int h, m;
    getLocalHM(now, h, m);
    if (!is24Hour) {                                       // 12-hour conversion
        h = h % 12;
        if (h == 0) h = 12;                                // 0h/12h collapse to '12'
    }
    if (is24Hour || h >= 10) patterns[0] = DIGIT_PATTERNS[h / 10];   // leading zero in 24h only
    patterns[1] = DIGIT_PATTERNS[h % 10];
    patterns[2] = DIGIT_PATTERNS[m / 10];
    patterns[3] = DIGIT_PATTERNS[m % 10];

    if (mode == SETUP_HOUR)   blink[0] = blink[1] = true;  // blink the field being edited
    if (mode == SETUP_MINUTE) blink[2] = blink[3] = true;
}

// ─── One multiplex step (called every ~DIGIT_MS) ───────────────────────────
void multiplexStep(unsigned long now) {
    if (now - lastDigitMs < DIGIT_MS) return;              // rate limit — not every loop
    lastDigitMs = now;

    uint8_t patterns[4]; bool blink[4];
    buildFrame(now, patterns, blink);                      // compute frame once per step
    bool fastOn = (now % 300) < 150;                       // ~3.3 Hz blink for setup fields

    currentDigit = (currentDigit + 1) % 4;                 // advance to next digit slot

    for (int i = 0; i < 4; i++)
        digitalWrite(DIGIT_PINS[i], HIGH);                 // disable all cathodes (off)

    uint8_t pat = patterns[currentDigit];
    if (blink[currentDigit] && !fastOn) pat = PATTERN_BLANK;   // blank during blink-off half

    for (int s = 0; s < 7; s++)
        digitalWrite(SEG_PINS[s], (pat >> s) & 1);         // drive shared segment bus

    digitalWrite(DIGIT_PINS[currentDigit], LOW);           // enable ONLY this digit (LOW = on)
}

// ─── Colon: short 100 ms tick each second in run, solid in setup ───────────
void updateColon(unsigned long now) {
    const unsigned long TICK_ON_MS = 100;                  // pulse width — feel of a mechanical tick
    bool on = (mode == RUN) ? ((now % 1000) < TICK_ON_MS)  // 10 % duty at 1 Hz
                            : true;                        // solid in setup
    digitalWrite(COLON_PIN, on ? HIGH : LOW);
}

// ─── Button dispatch ───────────────────────────────────────────────────────
void handleButtons(unsigned long now) {
    int mE = checkButton(btnMode, now);
    int uE = checkButton(btnUp,   now);
    int dE = checkButton(btnDown, now);

    if (mE == 1) {                                         // MODE short press
        if (mode == RUN) {
            is24Hour = !is24Hour;                          // toggle format
            prefs.putBool("h24", is24Hour);                // persist
            Serial.printf("→ %s\n", is24Hour ? "24-hour" : "12-hour");
        } else {                                           // in setup: cycle through fields
            mode = (ClockMode)((int)mode + 1);
            if (mode > SETUP_TZ) mode = RUN;               // wrap back after last field
            Serial.printf("→ %s\n", modeName[mode]);
        }
    } else if (mE == 2) {                                  // MODE long press
        mode = (mode == RUN) ? SETUP_HOUR : RUN;           // toggle in/out of setup
        Serial.printf("→ %s (long)\n", modeName[mode]);
    }

    if (mode != RUN) {                                     // UP/DOWN only affect setup fields
        if (uE == 1) adjustField(+1, now);
        if (dE == 1) adjustField(-1, now);
    }
}

// ─── Arduino entry points ──────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    for (int i = 0; i < 7; i++) {                          // segment bus: OUTPUT, start LOW
        pinMode(SEG_PINS[i], OUTPUT);
        digitalWrite(SEG_PINS[i], LOW);
    }
    for (int i = 0; i < 4; i++) {                          // digit cathodes: OUTPUT, start HIGH (off)
        pinMode(DIGIT_PINS[i], OUTPUT);
        digitalWrite(DIGIT_PINS[i], HIGH);
    }
    pinMode(COLON_PIN,     OUTPUT);
    pinMode(BTN_MODE_PIN,  INPUT_PULLUP);                  // idle HIGH, pressed = LOW
    pinMode(BTN_UP_PIN,    INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN,  INPUT_PULLUP);

    prefs.begin("watch", false);                           // false = read/write
    is24Hour      = prefs.getBool("h24", true);            // default 24-hour
    tzOffsetHours = prefs.getInt ("tz",  0);               // default UTC
    millisAtSet   = millis();                              // anchor "now" for time calc

    Serial.printf("Watch boot: %s, TZ %+d, start 12:00 UTC\n",
                  is24Hour ? "24h" : "12h", tzOffsetHours);
    Serial.println("MODE short=12/24 · MODE long=setup · UP/DOWN adjust in setup");
}

void loop() {
    unsigned long now = millis();
    handleButtons(now);                                    // 1) accept input
    multiplexStep(now);                                    // 2) refresh one digit
    updateColon(now);                                      // 3) blink/solid the colon
}
