#include <EEPROM.h>
#include <MIDI.h>

#include "WhammyArpEngine.h"

using whammy::MidiCompositeCommand;
using whammy::Sequence;
using whammy::WhammyArpeggiatorEngine;

MIDI_CREATE_DEFAULT_INSTANCE();

namespace {

const int kTempoButtonPin = 2;
const int kTempoLedPin = 13;
const uint8_t kWhammyMidiChannel = 1;

const unsigned long kDebounceMs = 40;
const unsigned long kStartupGraceMs = 250;

const char kConfigVersion[4] = "W3";
const int kConfigStartAddress = 64;

struct PersistedSettings {
  char version[4];
  uint8_t sequenceCount;
  Sequence sequences[whammy::kMaxSequences];
};

const PersistedSettings kFactorySettings = {
    "W3",
    6,
    {
        {{0, 4, 7, 12}, 4, 120, 4, "Major Triad"},
        {{0, 3, 7, 10}, 4, 96, 4, "Minor7"},
        {{0, 5, 7, 12}, 4, 128, 8, "Sus4 Drive"},
        {{12, 7, 5, 3, 0}, 5, 90, 4, "Descending"},
        {{0, 12, 7, 12, 4, 12}, 6, 140, 8, "Shimmer"},
        {{0, 2, 4, 5, 7, 9, 11, 12}, 8, 110, 4, "Ionian Run"},
    },
};

PersistedSettings settings;
WhammyArpeggiatorEngine engine;

volatile bool buttonPressed = false;
unsigned long lastDebouncedTapMs = 0;
unsigned long setupMs = 0;

class ArduinoMidiOutput : public whammy::MidiOutput {
 public:
  void sendWhammyCommand(const MidiCompositeCommand& command) {
    MIDI.sendControlChange(11, command.ccValue, kWhammyMidiChannel);
    MIDI.sendProgramChange(command.programChange - 1, kWhammyMidiChannel);
  }
};

ArduinoMidiOutput midiOutput;

void copyFactorySettings() { settings = kFactorySettings; }

void saveSettings() {
  for (unsigned int i = 0; i < sizeof(PersistedSettings); ++i) {
    EEPROM.update(kConfigStartAddress + i, *((const uint8_t*)&settings + i));
  }
}

bool settingsVersionMatches() {
  for (uint8_t i = 0; i < 2; ++i) {
    if (settings.version[i] != kConfigVersion[i]) {
      return false;
    }
  }
  return true;
}

uint8_t clampSequenceCount(uint8_t count) {
  if (count == 0 || count > whammy::kMaxSequences) {
    return kFactorySettings.sequenceCount;
  }
  return count;
}

void loadSettings() {
  for (unsigned int i = 0; i < sizeof(PersistedSettings); ++i) {
    *((uint8_t*)&settings + i) = EEPROM.read(kConfigStartAddress + i);
  }

  if (!settingsVersionMatches()) {
    copyFactorySettings();
    saveSettings();
    return;
  }

  settings.sequenceCount = clampSequenceCount(settings.sequenceCount);
}

void onTempoButtonInterrupt() { buttonPressed = true; }

void applyTapTempoIfNeeded(unsigned long nowMs) {
  if (!buttonPressed) {
    return;
  }
  buttonPressed = false;

  if (nowMs - setupMs < kStartupGraceMs) {
    return;
  }

  if (nowMs - lastDebouncedTapMs < kDebounceMs) {
    return;
  }

  lastDebouncedTapMs = nowMs;
  engine.onTapTempo(nowMs);
}

}  // namespace

void setup() {
  pinMode(kTempoLedPin, OUTPUT);
  pinMode(kTempoButtonPin, INPUT_PULLUP);

  loadSettings();
  engine.loadSequences(settings.sequences, settings.sequenceCount);
  engine.selectSequence(0);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  attachInterrupt(digitalPinToInterrupt(kTempoButtonPin), onTempoButtonInterrupt, FALLING);

  setupMs = millis();
  engine.start(setupMs);
}

void loop() {
  const unsigned long nowMs = millis();
  applyTapTempoIfNeeded(nowMs);

  const bool ledOn = engine.update(nowMs, midiOutput);
  digitalWrite(kTempoLedPin, ledOn ? HIGH : LOW);
}
