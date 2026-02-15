#pragma once

#include <stdint.h>

namespace whammy {

// Whammy command table supports chromatic shifts from unison to +12 semitones.
static const uint8_t kWhammyIntervalCount = 13;

// Project limits kept intentionally small for AVR SRAM.
static const uint8_t kMaxSequenceSteps = 16;
static const uint8_t kMaxSequences = 8;

static const unsigned long kMsInMinute = 60000UL;
static const unsigned int kDefaultTempoBpm = 120;

struct MidiCompositeCommand {
  uint8_t programChange;  // 1-indexed in the original Whammy MIDI chart.
  uint8_t ccValue;
};

struct Sequence {
  int8_t intervals[kMaxSequenceSteps];
  uint8_t stepCount;
  unsigned int initialTempoBpm;
  uint8_t tickSubdivisions;
  char name[16];
};

struct RuntimeState {
  bool isPlaying;
  uint8_t selectedSequence;
  uint8_t currentStep;
  unsigned int tempoBpm;
  unsigned long lastBeatMs;
  unsigned long lastSubdivisionMs;
  bool beatLedOn;
};

class MidiOutput {
 public:
  virtual ~MidiOutput() {}
  virtual void sendWhammyCommand(const MidiCompositeCommand& command) = 0;
};

class TapTempo {
 public:
  TapTempo();

  void recordTap(unsigned long nowMs);
  bool hasTempo() const;
  unsigned int tempoBpm() const;

 private:
  static const uint8_t kTapHistory = 5;
  static const unsigned int kMinTempoBpm = 30;
  static const unsigned int kMaxTempoBpm = 300;

  unsigned long taps_[kTapHistory];
  bool hasTap_[kTapHistory];
  unsigned int tempoBpm_;

  void recomputeTempo();
};

class WhammyArpeggiatorEngine {
 public:
  WhammyArpeggiatorEngine();

  void loadSequences(const Sequence* sequences, uint8_t count);
  bool selectSequence(uint8_t index);
  uint8_t sequenceCount() const;

  void start(unsigned long nowMs);
  void stop();

  void onTapTempo(unsigned long nowMs);

  // Called in each main loop tick. Returns true when beat LED should be ON.
  bool update(unsigned long nowMs, MidiOutput& output);

  const RuntimeState& state() const;
  const Sequence& selectedSequence() const;

 private:
  static const unsigned long kLedPulseMs = 100;
  static const uint8_t kMaxTicksPerUpdate = 8;

  Sequence sequences_[kMaxSequences];
  uint8_t sequenceCount_;
  RuntimeState state_;
  TapTempo tapTempo_;

  void sanitizeSequence(Sequence& sequence) const;
  unsigned long beatIntervalMs() const;
  unsigned long subdivisionIntervalMs() const;
};

extern const MidiCompositeCommand kWhammyIntervals[kWhammyIntervalCount];

}  // namespace whammy
