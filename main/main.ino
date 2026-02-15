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

const char kConfigVersion[4] = "W2";
const int kConfigStartAddress = 64;

struct PersistedSettings {
  char version[4];
  Sequence sequences[whammy::kMaxSequences];
};

const PersistedSettings kFactorySettings = {
    "W2",
    {
        {{0, 4, 7, 12}, 4, 120, 4, "Major Triad"},
        {{0, 3, 7, 10}, 4, 100, 4, "Minor7"},
        {{0, 5, 7, 12}, 4, 130, 8, "Sus4 Drive"},
        {{12, 7, 5, 3, 0}, 5, 90, 4, "Descending"},
    },
};

PersistedSettings settings;
WhammyArpeggiatorEngine engine;

volatile bool buttonPressed = false;
unsigned long lastDebouncedTapMs = 0;

class ArduinoMidiOutput : public whammy::MidiOutput {
 public:
  void sendWhammyCommand(const MidiCompositeCommand& command) {
    MIDI.sendControlChange(11, command.ccValue, kWhammyMidiChannel);
    MIDI.sendProgramChange(command.programChange - 1, kWhammyMidiChannel);
  }
};

ArduinoMidiOutput midiOutput;

void copyFactorySettings() {
  settings = kFactorySettings;
}

void saveSettings() {
  for (unsigned int i = 0; i < sizeof(PersistedSettings); ++i) {
    EEPROM.write(kConfigStartAddress + i, *((const char*)&settings + i));
  }
}

void loadSettings() {
  for (unsigned int i = 0; i < sizeof(PersistedSettings); ++i) {
    *((char*)&settings + i) = EEPROM.read(kConfigStartAddress + i);
  }

  if (strncmp(settings.version, kConfigVersion, 2) != 0) {
    copyFactorySettings();
    saveSettings();
  }
}

void onTempoButtonInterrupt() { buttonPressed = true; }

void applyTapTempoIfNeeded(unsigned long nowMs) {
  if (!buttonPressed) {
    return;
  }
  buttonPressed = false;

  if (nowMs - lastDebouncedTapMs < kDebounceMs) {
    return;
  }

  lastDebouncedTapMs = nowMs;
  engine.onTapTempo(nowMs);
}

}  // namespace

void setup() {
  pinMode(kTempoLedPin, OUTPUT);
  pinMode(kTempoButtonPin, INPUT);

  loadSettings();
  engine.loadSequences(settings.sequences, whammy::kMaxSequences);
  engine.selectSequence(0);
  engine.start(millis());

  MIDI.begin(MIDI_CHANNEL_OMNI);
  attachInterrupt(digitalPinToInterrupt(kTempoButtonPin), onTempoButtonInterrupt, RISING);
}

void loop() {
  unsigned long nowMs = millis();
  applyTapTempoIfNeeded(nowMs);

  bool ledOn = engine.update(nowMs, midiOutput);
  digitalWrite(kTempoLedPin, ledOn ? HIGH : LOW);
}
