// Exercise 08 — Three-Phase AC Power Simulation
//
// Three LEDs represent the three phases (L1, L2, L3) of a three-phase AC
// power system. Each phase is a sine wave, offset from the next by exactly
// 120° (2π/3 rad) — the same mathematical relationship that lets a real
// three-phase system deliver constant power to a balanced load.
//
// Wiring:
//    L1 (red)    → GPIO 2   → LEDC channel 0
//    L2 (yellow) → GPIO 4   → LEDC channel 1
//    L3 (blue)   → GPIO 5   → LEDC channel 2
//
// The demo runs slowly (see SIM_FREQ_HZ) so you can watch the peak visibly
// "rotate" from L1 → L2 → L3 → L1. Real grid power runs at 50 Hz
// (Europe/Asia/Africa) or 60 Hz (Americas) — invisible to the eye due to
// persistence of vision, which is precisely why AC lights don't flicker.
//
// ─── APPROACH ───────────────────────────────────────────────────────────────
//
// 1) DATA-DRIVEN PHASES.
//    Each phase is described by { pin, channel, offset, name }. Adding a
//    five-phase (72° apart) or dropping to a two-phase Tesla-style system is
//    one array edit — loop() doesn't change.
//
// 2) SHIFTED SINE FOR NON-NEGATIVE BRIGHTNESS.
//    An LED can't emit negative light. Real AC voltage swings ±V_peak; we
//    map it to 0..255 via  brightness = (sin(ωt + φ) + 1) / 2 · 255.
//    Trough = 0, peak = 255, phase relationship preserved. Alternative
//    mappings (rectified, squared for power) are noted in the loop body.
//
// 3) NON-BLOCKING BY CONSTRUCTION.
//    No delay(). Time comes from millis(); brightness is computed from time.
//    loop() runs freely, so serial prints / a button / a fault-detection
//    routine could all coexist without hiccuping the fades.

#include <Arduino.h>
#include <math.h>

// ─── Data model ─────────────────────────────────────────────────────────────
// One instance = one AC phase. Every calculation below iterates "for each
// Phase in this array" — nothing about the *number* of phases is hard-coded.
//
// TRY: add `float amplitude;` here and multiply the sine by it inside loop().
//      That simulates an UNBALANCED load — one phase weaker than the others,
//      which is exactly what you see on a real distribution board when a big
//      single-phase appliance runs on one leg.
struct Phase {
    int   pin;         // which GPIO drives this phase's LED
    int   channel;     // which LEDC hardware PWM channel (0..15 available)
    float offset;      // phase shift in radians (0 = reference)
    const char* name;  // label used only for logs — cheap to add, easy to grep
};

// ─── The system under simulation ────────────────────────────────────────────
// Three phases, 120° apart. That IS the definition of a 3-phase system.
// Offsets:
//    L1:  0 rad         reference phase
//    L2: -2π/3          120° BEHIND L1
//    L3: -4π/3          240° BEHIND L1 (equivalently, 120° AHEAD)
// The negative sign matches how power engineers draw the rotating phasor
// diagram counter-clockwise; flip the signs and you reverse rotation
// direction — which, on a real 3-phase motor, reverses spin.
//
// TRY: set all three offsets to 0 → single-phase system, all LEDs rise and
//      fall together. That's what powers your house wall socket.
// TRY: five-phase pentagon {0, -2π/5, -4π/5, -6π/5, -8π/5} → 5-phase
//      (rare in industry, but shows the pattern generalises to N phases).
const Phase phases[] = {
    { 2, 0,  0.0f,                    "L1 (R)" },
    { 4, 1, -2.0f * (float)PI / 3.0f, "L2 (Y)" },
    { 5, 2, -4.0f * (float)PI / 3.0f, "L3 (B)" }
};
const int NUM_PHASES = sizeof(phases) / sizeof(phases[0]);

// ─── PWM (LEDC) tuning ──────────────────────────────────────────────────────
// The LEDC peripheral switches each pin HIGH/LOW at LEDC_FREQ_HZ. What the
// LED (and the eye) sees is the *average*, which the duty cycle controls.
//
// LEDC_FREQ_HZ has to be well above the eye's flicker threshold (~200 Hz).
// 5 kHz is a comfortable pick — invisible flicker, plenty of headroom.
const int LEDC_FREQ_HZ    = 5000;

// LEDC_RESOLUTION picks the duty-value bit depth. 8 bits → 0..255 (256
// levels). Hardware constraint: freq × 2^resolution must stay under the
// LEDC clock (~80 MHz). 5 kHz × 256 = 1.28 MHz — well inside limits.
//
// TRY: 12-bit resolution (0..4095) with a slower freq for silky-smooth fades.
const int LEDC_RESOLUTION = 8;

// ─── The simulation frequency (NOT the PWM carrier) ─────────────────────────
// This is the frequency of the SIMULATED AC waveform. Real grid: 50/60 Hz.
// At those speeds the LEDs would look continuously lit — which is the whole
// point of real AC power.
//
// We run this slow so you can watch phase rotation with the naked eye.
// Slow it further to think about the math; speed it up until it blurs to
// feel what real AC "looks like" through persistence of vision.
//
// TRY: 50.0f → three steadily-lit LEDs, indistinguishable from steady DC.
//      Congratulations — you've just recreated the illusion of "smooth"
//      AC lighting that your ceiling lamp uses every day.
const float SIM_FREQ_HZ = 0.5f;

void setup() {
    Serial.begin(115200);

    // Configure one LEDC channel per phase, then bind each channel to its
    // GPIO. This loop scales cleanly: 3 phases, 5 phases, 15 phases — same
    // code, only the `phases[]` array changes.
    for (int i = 0; i < NUM_PHASES; i++) {

        // ledcSetup(channel, freq, resolution) — allocate & configure the
        // hardware PWM channel. Setup MUST come before attaching a pin;
        // otherwise the pin would be driven by an unconfigured channel and
        // behaviour is undefined.
        ledcSetup(phases[i].channel, LEDC_FREQ_HZ, LEDC_RESOLUTION);

        // ledcAttachPin(pin, channel) — route the channel's output to the
        // GPIO. From this line on, digitalWrite() on this pin does NOTHING;
        // only ledcWrite() moves it. Call ledcDetachPin() to get it back
        // as a plain GPIO.
        ledcAttachPin(phases[i].pin, phases[i].channel);
    }

    Serial.printf("3-phase demo: %d phases at %.2f Hz\n",
                  NUM_PHASES, SIM_FREQ_HZ);
}

void loop() {

    // ── Step 1 — what time is it? ──────────────────────────────────────────
    // millis() returns milliseconds since boot as unsigned long. Dividing
    // by 1000.0 (note the ".0") converts to seconds as a FLOAT — sub-ms
    // precision preserved. Without the ".0", integer division would truncate
    // to whole seconds and the fade would step in one-second jumps.
    float t = millis() / 1000.0f;

    // ── Step 2 — convert time to angle ─────────────────────────────────────
    // A sine wave is defined over radians, not seconds. To convert seconds
    // → radians we multiply by ω = 2π · f, the angular frequency (radians
    // per second the phasor rotates at).
    //
    // At SIM_FREQ_HZ = 0.5, ω = π rad/s. After 2 seconds we've swept 2π rad
    // = one full cycle. That's exactly what "0.5 Hz" means.
    //
    // TRY: replace with `wt = fmodf(2*PI*f*t, 2*PI)` if you're worried about
    //      float precision drift after millions of iterations (not for a
    //      short demo, but essential for anything running for weeks).
    float wt = 2.0f * (float)PI * SIM_FREQ_HZ * t;

    // ── Step 3 — for each phase, compute AND write its brightness ─────────
    for (int i = 0; i < NUM_PHASES; i++) {

        // sin(wt + φ) sits in the closed interval [-1, +1].
        // LEDs need [0, 255]. Shift and scale:
        //   • + 1.0 → range becomes [0, 2]
        //   • × 0.5 → range becomes [0, 1]         (this line)
        //   • × 255 → range becomes [0, 255]       (next line)
        //
        // The AC's zero-crossings (sin = 0) map to LED half-brightness, not
        // to LED off. That preserves the phase RELATIONSHIP visually — you
        // can see all three phases doing something at every instant.
        //
        // TRY (rectified): v = fabsf(sinf(wt + phases[i].offset));
        //      Full-wave rectified — pulses twice per cycle. Looks more like
        //      real power consumption (which is always positive).
        //
        // TRY (instantaneous power on a resistive load): compute
        //      float s = sinf(wt + phases[i].offset);
        //      v = s * s;
        //      That's P = V²/R, always positive, zero at zero-crossings.
        //      The three phases sum to a constant — the mathematical reason
        //      3-phase motors run smoother than single-phase.
        float v = (sinf(wt + phases[i].offset) + 1.0f) * 0.5f;

        // Map float [0, 1] → int [0, 255]. Cast to int truncates; for 256
        // levels of LED brightness the ~0.5 % rounding loss is invisible.
        //
        // TRY (gamma correction): human perception of brightness isn't
        //      linear. `duty = (int)(powf(v, 2.2f) * 255.0f)` makes the
        //      fade look more visually linear — especially near the dim
        //      end where linear-duty LEDs seem to change fastest.
        int duty = (int)(v * 255.0f);

        // Push the value into the PWM hardware. This is where the LED
        // actually changes. It's cheap — a few register writes, no waiting.
        //
        // TRY (data logging): every ~50 ms, print `t`, `v` for each phase
        //      via Serial. Paste into a spreadsheet to plot the waveform.
        //      Use the modulo trick to rate-limit:
        //         if (millis() % 50 < 5) Serial.printf(...);
        //      Full-speed printing would flood the serial buffer.
        ledcWrite(phases[i].channel, duty);
    }

    // No delay() anywhere. loop() runs at whatever speed the ESP32 can
    // manage — hundreds of thousands of iterations per second — but that's
    // fine: `duty` only visibly changes when it crosses a new integer.
    //
    // Because brightness always comes from `millis()`, you can add ANY other
    // work here — button reads, sensor sampling, MQTT publish — and none of
    // it will perturb the fade timing. That's the payoff of writing this
    // non-blocking from the start.
}
