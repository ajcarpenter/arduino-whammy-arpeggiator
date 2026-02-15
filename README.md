# Arduino Whammy Arpeggiator (Rebuilt)

A complete rewrite of the original sketch into a maintainable project with:

- a reusable arpeggiator engine,
- a cleaner Arduino runtime layer,
- persisted sequence presets in EEPROM,
- and a host-side simulation harness with automated tests.

## Why this rewrite

The original project worked, but mixed timing, tap-tempo, MIDI mapping, storage, and board-specific code in one sketch. This rewrite separates those concerns so it is easier to:

- understand,
- extend,
- verify without hardware,
- and keep stable over time.

## Project layout

- `main/main.ino` — Arduino-specific glue (pins, EEPROM load/save, MIDI send, interrupt wiring).
- `main/WhammyArpEngine.h` / `main/WhammyArpEngine.cpp` — platform-agnostic arpeggiator engine.
- `sim/WhammyArpEngineTests.cpp` — desktop simulation harness + tests.

## Core behavior

### Sequence model

Each sequence has:

- up to 16 semitone intervals (`0..12` supported by the Whammy mapping),
- step count,
- initial tempo,
- beat subdivisions,
- name.

At runtime the engine iterates sequence steps on each subdivision tick and emits the corresponding MIDI command pair:

- Control Change 11 (for fine position), then
- Program Change (for whammy mode bank).

### Tap tempo

- Tap timestamps are recorded with short history.
- Tempo is computed from recent valid intervals (< 5 seconds).
- New tempo immediately resets beat/subdivision clocks and sequence step index for tight sync.

### LED pulse

- LED turns on at each beat.
- LED turns off after ~100ms pulse width.

### EEPROM persistence

`main.ino` loads a `PersistedSettings` block from EEPROM with version checking.

- If EEPROM version mismatches, factory defaults are installed and saved.
- Defaults include several musical sequences to be useful immediately.

## Running simulation tests

From repository root:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -o sim/whammy_tests sim/WhammyArpEngineTests.cpp main/WhammyArpEngine.cpp
./sim/whammy_tests
```

## Adapting for your board

In `main/main.ino` adjust:

- `kTempoButtonPin`
- `kTempoLedPin`
- `kWhammyMidiChannel`
- factory preset sequences

for your hardware wiring and musical preferences.

## Notes

- This rewrite keeps the original Whammy interval mapping table as the canonical source.
- Intervals outside the valid range are clamped safely.
- Heavy work is avoided in interrupt context; ISR only sets a flag and debouncing is done in the main loop.
