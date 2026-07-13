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
#include <Preferences.h>

// ─── Pins ───────────────────────────────────────────────────────────────────
const int SEG_PINS[7]   = { 13, 14, 27, 26, 25, 33, 32 };  // a, b, c, d, e, f, g
const int DIGIT_PINS[4] = { 16, 17, 18, 19 };              // left → right
const int COLON_PIN     = 21;
const int BTN_MODE_PIN  = 22;
const int BTN_UP_PIN    = 23;
const int BTN_DOWN_PIN  = 5;

// ─── Segment bit patterns (bit 0 = a, … bit 6 = g) ──────────────────────────
const uint8_t DIGIT_PATTERNS[10] = {
    0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,   // 0..4
    0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111    // 5..9
};
const uint8_t PATTERN_BLANK = 0;
const uint8_t PATTERN_MINUS = 0b01000000;                      // segment g only

// ─── Mode & user prefs ──────────────────────────────────────────────────────
enum ClockMode { RUN, SETUP_HOUR, SETUP_MINUTE, SETUP_TZ };
const char* modeName[] = { "RUN", "SETUP_HOUR", "SETUP_MINUTE", "SETUP_TZ" };
ClockMode mode = RUN;
bool is24Hour = true;
int  tzOffsetHours = 0;                                        // clamped -12..+14

// ─── Time — tracked as UTC seconds since a "set" anchor ─────────────────────
int32_t utcSecondsAtSet = 12L * 3600;                          // default 12:00 UTC
unsigned long millisAtSet = 0;

// ─── NVS ────────────────────────────────────────────────────────────────────
Preferences prefs;

// ─── Multiplex state ────────────────────────────────────────────────────────
int  currentDigit          = 0;
unsigned long lastDigitMs  = 0;
const int DIGIT_MS         = 4;                                // 4 ms × 4 = ~60 Hz refresh

// ─── Button with short/long-press ──────────────────────────────────────────
struct Button {
    int pin;
    int lastRaw               = HIGH;
    int state                 = HIGH;
    unsigned long lastChangeMs = 0;
    unsigned long pressStartMs = 0;
    bool longFired            = false;
    Button(int p) : pin(p) {}
};

const unsigned long DEBOUNCE_MS   = 30;
const unsigned long LONG_PRESS_MS = 1000;

// Returns 0=none, 1=short (on release), 2=long (fires once while held).
int checkButton(Button &b, unsigned long now) {
    int raw = digitalRead(b.pin);
    if (raw != b.lastRaw) {                                    // raw change → restart debounce
        b.lastChangeMs = now;
        b.lastRaw = raw;
    }
    if ((now - b.lastChangeMs) > DEBOUNCE_MS && raw != b.state) {
        b.state = raw;
        if (b.state == LOW) {                                  // just pressed
            b.pressStartMs = now;
            b.longFired = false;
        } else if (!b.longFired) {                             // released before long fired
            return 1;
        }
    }
    if (b.state == LOW && !b.longFired &&                      // held long enough
        (now - b.pressStartMs) > LONG_PRESS_MS) {
        b.longFired = true;
        return 2;
    }
    return 0;
}

Button btnMode(BTN_MODE_PIN);
Button btnUp  (BTN_UP_PIN);
Button btnDown(BTN_DOWN_PIN);

// ─── Time helpers ──────────────────────────────────────────────────────────
int32_t currentUtcSeconds(unsigned long now) {
    return utcSecondsAtSet + (int32_t)((now - millisAtSet) / 1000);
}

void setUtcTime(int h, int m, unsigned long now) {             // h, m in [0..23], [0..59]
    utcSecondsAtSet = (int32_t)h * 3600 + (int32_t)m * 60;
    millisAtSet = now;
}

// Local (display) time = UTC + tz, wrapped into 0..1439 minutes.
void getLocalHM(unsigned long now, int &outH, int &outM) {
    int32_t utcMin   = currentUtcSeconds(now) / 60;
    int32_t localMin = utcMin + (int32_t)tzOffsetHours * 60;
    localMin = ((localMin % 1440) + 1440) % 1440;              // safe mod for negatives
    outH = localMin / 60;
    outM = localMin % 60;
}

// ─── Setup mode value adjustment ───────────────────────────────────────────
void adjustField(int delta, unsigned long now) {
    if (mode == SETUP_TZ) {
        tzOffsetHours += delta;
        if (tzOffsetHours > 14)  tzOffsetHours = 14;
        if (tzOffsetHours < -12) tzOffsetHours = -12;
        prefs.putInt("tz", tzOffsetHours);
        Serial.printf("TZ %+d\n", tzOffsetHours);
        return;
    }
    int32_t utc = currentUtcSeconds(now);
    int h = (utc / 3600) % 24;
    int m = (utc / 60)   % 60;
    if (mode == SETUP_HOUR)   h = (h + delta + 24) % 24;
    if (mode == SETUP_MINUTE) m = (m + delta + 60) % 60;
    setUtcTime(h, m, now);
    Serial.printf("UTC %02d:%02d\n", h, m);
}

// ─── Build the 4-digit frame for the current mode/state ────────────────────
void buildFrame(unsigned long now, uint8_t patterns[4], bool blink[4]) {
    for (int i = 0; i < 4; i++) { patterns[i] = PATTERN_BLANK; blink[i] = false; }

    if (mode == SETUP_TZ) {                                    // show " +HH" / " -HH" (right-aligned)
        int a = tzOffsetHours < 0 ? -tzOffsetHours : tzOffsetHours;
        int tensPos = 2, onesPos = 3;
        if (tzOffsetHours < 0) patterns[a >= 10 ? 1 : 2] = PATTERN_MINUS;  // slide minus in front of number
        if (a >= 10) patterns[tensPos] = DIGIT_PATTERNS[a / 10];
        patterns[onesPos] = DIGIT_PATTERNS[a % 10];
        blink[2] = blink[3] = true;
        return;
    }

    int h, m;
    getLocalHM(now, h, m);
    if (!is24Hour) {                                           // 12h: convert 0..23 → 1..12
        h = h % 12;
        if (h == 0) h = 12;
    }
    // Hours (leading zero in 24h; suppressed in 12h)
    if (is24Hour || h >= 10) patterns[0] = DIGIT_PATTERNS[h / 10];
    patterns[1] = DIGIT_PATTERNS[h % 10];
    patterns[2] = DIGIT_PATTERNS[m / 10];
    patterns[3] = DIGIT_PATTERNS[m % 10];

    if (mode == SETUP_HOUR)   blink[0] = blink[1] = true;
    if (mode == SETUP_MINUTE) blink[2] = blink[3] = true;
}

// ─── One multiplex step (called every ~DIGIT_MS) ───────────────────────────
void multiplexStep(unsigned long now) {
    if (now - lastDigitMs < DIGIT_MS) return;
    lastDigitMs = now;

    uint8_t patterns[4]; bool blink[4];
    buildFrame(now, patterns, blink);
    bool fastOn = (now % 300) < 150;                           // ~3.3 Hz blink for setup fields

    currentDigit = (currentDigit + 1) % 4;

    for (int i = 0; i < 4; i++) digitalWrite(DIGIT_PINS[i], HIGH);   // disable all (cathode HIGH)

    uint8_t pat = patterns[currentDigit];
    if (blink[currentDigit] && !fastOn) pat = PATTERN_BLANK;
    for (int s = 0; s < 7; s++) digitalWrite(SEG_PINS[s], (pat >> s) & 1);

    digitalWrite(DIGIT_PINS[currentDigit], LOW);               // enable this digit
}

// ─── Colon: 1 Hz blink in run, solid in setup ──────────────────────────────
void updateColon(unsigned long now) {
    bool on = (mode == RUN) ? ((now % 1000) < 500) : true;
    digitalWrite(COLON_PIN, on ? HIGH : LOW);
}

// ─── Button dispatch ───────────────────────────────────────────────────────
void handleButtons(unsigned long now) {
    int mE = checkButton(btnMode, now);
    int uE = checkButton(btnUp,   now);
    int dE = checkButton(btnDown, now);

    if (mE == 1) {                                             // MODE short press
        if (mode == RUN) {
            is24Hour = !is24Hour;
            prefs.putBool("h24", is24Hour);
            Serial.printf("→ %s\n", is24Hour ? "24-hour" : "12-hour");
        } else {                                               // cycle through setup fields
            mode = (ClockMode)((int)mode + 1);
            if (mode > SETUP_TZ) mode = RUN;
            Serial.printf("→ %s\n", modeName[mode]);
        }
    } else if (mE == 2) {                                      // MODE long press
        mode = (mode == RUN) ? SETUP_HOUR : RUN;
        Serial.printf("→ %s (long)\n", modeName[mode]);
    }

    if (mode != RUN) {                                         // UP/DOWN only in setup
        if (uE == 1) adjustField(+1, now);
        if (dE == 1) adjustField(-1, now);
    }
}

// ─── Arduino entry points ──────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    for (int i = 0; i < 7; i++) { pinMode(SEG_PINS[i],   OUTPUT); digitalWrite(SEG_PINS[i],   LOW);  }
    for (int i = 0; i < 4; i++) { pinMode(DIGIT_PINS[i], OUTPUT); digitalWrite(DIGIT_PINS[i], HIGH); }
    pinMode(COLON_PIN,     OUTPUT);
    pinMode(BTN_MODE_PIN,  INPUT_PULLUP);
    pinMode(BTN_UP_PIN,    INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN,  INPUT_PULLUP);

    prefs.begin("watch", false);
    is24Hour      = prefs.getBool("h24", true);
    tzOffsetHours = prefs.getInt ("tz",  0);
    millisAtSet   = millis();

    Serial.printf("Watch boot: %s, TZ %+d, start 12:00 UTC\n",
                  is24Hour ? "24h" : "12h", tzOffsetHours);
    Serial.println("MODE short=12/24 · MODE long=setup · UP/DOWN adjust in setup");
}

void loop() {
    unsigned long now = millis();
    handleButtons(now);
    multiplexStep(now);
    updateColon(now);
}
