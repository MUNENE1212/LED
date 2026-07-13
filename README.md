# ESP32 Learning Projects

A progression of ESP32 exercises that grow from a single blinking LED into
small, focused projects — each one solving something you'd plausibly build
for real. Written in C++ with the Arduino framework, developed with
PlatformIO, and simulated with [Wokwi](https://wokwi.com) so anything here
runs in a browser without needing hardware on the desk.

## Toolchain

- **Board:** classic ESP32 DevKit-C v4 (`board = esp32dev`)
- **Framework:** Arduino-ESP32 (v2.x, the stock PlatformIO release)
- **Build:** [PlatformIO](https://platformio.org) — one `[env:]` per exercise
- **Simulator:** [Wokwi for VS Code](https://docs.wokwi.com/vscode/getting-started)

## Repo layout

Every exercise lives in `src/NN_name/main.cpp` with its own PlatformIO env
block in `platformio.ini`. The root `diagram.json` and `wokwi.toml` are the
"currently active" simulator config — switching to another exercise is a
build command plus a two-line edit in `wokwi.toml`. History of past
diagrams and code is preserved via git.

```
Blink/
├── platformio.ini         # one [env:NN_name] per exercise
├── diagram.json           # active Wokwi wiring
├── wokwi.toml             # active firmware/elf paths for the simulator
└── src/
    ├── 01_basic_blink/
    ├── 02_heartbeat_blink/
    └── ...
```

## Build & simulate any exercise

```bash
# Build:
pio run -e 08_three_phase

# In wokwi.toml, point at that env's build output:
firmware = '.pio/build/08_three_phase/firmware.bin'
elf      = '.pio/build/08_three_phase/firmware.elf'

# Then run Wokwi from VS Code (⌘/Ctrl-Shift-P → "Wokwi: Start Simulator").
```

## Exercises

| # | Name | What it does | Concepts |
|---|---|---|---|
| 01 | [basic_blink](src/01_basic_blink) | Steady 1 Hz blink on GPIO 2. | `pinMode`, `digitalWrite`, `delay`. |
| 02 | [heartbeat_blink](src/02_heartbeat_blink) | Three rapid blinks + 1 s pause, with serial output. | `for` loop, `Serial`. |
| 03 | [sequential_leds](src/03_sequential_leds) | 3 LEDs cascade on then off. | Array of pins, `sizeof` for element count. |
| 04 | [traffic_light](src/04_traffic_light) | UK/EU traffic light with Red+Yellow transition. | Enum + `switch` finite-state machine. |
| 05 | [binary_counter](src/05_binary_counter) | 3-bit counter with UP/DOWN buttons, clamped 0–7. | `INPUT_PULLUP`, bitwise output, tick sampling. |
| 06 | [random_flasher](src/06_random_flasher) | Random LED flashes 3× then moves on. | `random`, `randomSeed`, helper functions. |
| 07 | [pwm_dimmer](src/07_pwm_dimmer) | Two-button PWM dimmer for a red LED. | ESP32 LEDC, encapsulated `DebouncedButton` struct, non-blocking debounce. |
| 08 | [three_phase](src/08_three_phase) | **Three-phase AC power simulator** — 3 LEDs 120° apart, driven by shifted sines. | Data-driven design, `millis()`-based time, `sinf`, LEDC on 3 channels. |
| 09 | [rgb_mixer](src/09_rgb_mixer) | RGB LED cycles through the primary and secondary colors. | 3 LEDC channels, color as data. |
| 10 | [temp_indicator](src/10_temp_indicator) | RGB LED shows a "temperature" band; flashes red when hot. | Enum bands, non-blocking flash, classify-then-render split. |
| 11 | [smart_bulb](src/11_smart_bulb) | **Smart LED bulb** — 6 LEDs (2R+2G+2B) that cycle modes on every power-cycle. Same trick real no-app smart bulbs use. | `Preferences` / NVS persistence, commit-delay pattern, 6 PWM channels, hex-layout diagram. |

## Design principles

Every project after Ex 07 follows the same "real-hardware-ready" rules:

- **No blocking `delay()` in the main path.** Everything runs from `millis()`
  so multiple concerns can coexist without hiccuping each other.
- **Encapsulate patterns.** When a piece of logic will exist for more than
  one instance (buttons, PWM channels, phases), it becomes a struct + a
  helper function.
- **Data over code.** Palettes, phase tables, and pin lists live in arrays
  so adding a case is one line, not a copy-paste of a block.
- **Explain the *why* in comments, not the *what*.** Names carry the what.

## What's next

Going forward, each exercise gets:
- Its own project framing (what it *does*, not just what concept it exercises).
- A commit with a clear, standalone message.
- A snapshot in this README's table.

The goal isn't just to learn Arduino — it's to build a portfolio of small,
real things.
