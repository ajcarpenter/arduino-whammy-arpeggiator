# Arduino Whammy Arpeggiator (Rebuilt)

A full modernization of the original sketch into a small, testable system that still fits classic Arduino constraints.

## What this version improves

- **Clear architecture:** timing and musical logic live in a reusable engine, while `main.ino` handles board wiring and peripherals.
- **Safer persistence:** EEPROM settings are versioned and written with `EEPROM.update` to reduce unnecessary flash wear.
- **More musical defaults:** multiple factory presets are included so the device is useful immediately.
- **Hardware-free validation:** a desktop simulation harness exercises core behavior with repeatable tests.

## Project layout

- `main/main.ino` — Arduino runtime integration (pins, ISR wiring, EEPROM, MIDI adapter).
- `main/WhammyArpEngine.h` / `main/WhammyArpEngine.cpp` — platform-neutral engine.
- `sim/WhammyArpEngineTests.cpp` — host simulation + assertions.

## Engine behavior

### Sequence model

Each sequence contains:

- up to 16 interval steps,
- step count,
- initial tempo,
- beat subdivisions,
- name.

Invalid sequence values are sanitized when loaded (minimum one step, default tempo, default subdivisions), making EEPROM corruption less likely to break runtime behavior.

### MIDI output

On each subdivision tick the engine emits one composite Whammy command:

1. Control Change #11 for parameter position.
2. Program Change for Whammy slot selection.

The original interval table (0 to +12 semitones) is preserved.

### Timing and jitter handling

- Subdivision and beat timers run independently.
- If `loop()` is delayed, the engine catches up a bounded number of ticks in one update to keep musical phase coherent.
- Beat LED pulses for ~100ms each beat.

### Tap tempo

- Tap history is averaged across recent valid taps.
- Tempo is clamped to a practical range (30–300 BPM).
- New tap tempo immediately re-aligns beat/subdivision timers and sequence position.

## Arduino runtime behavior

`main.ino` uses interrupt-safe button handling:

- ISR only sets a flag.
- Debounce and startup grace logic run in the main loop.
- Button defaults to `INPUT_PULLUP` + `FALLING` trigger for robust wiring with a simple momentary switch to ground.

## Running simulation tests

From repo root:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -o sim/whammy_tests sim/WhammyArpEngineTests.cpp main/WhammyArpEngine.cpp
./sim/whammy_tests
```

## Suggested hardware hookup

- Tempo button: between `kTempoButtonPin` and GND.
- Tempo LED: `kTempoLedPin` -> resistor -> LED -> GND.
- MIDI out: standard UART MIDI interface matching your board design.

## Tuning points

Adjust in `main/main.ino`:

- `kTempoButtonPin`
- `kTempoLedPin`
- `kWhammyMidiChannel`
- `kFactorySettings` sequences and `sequenceCount`
