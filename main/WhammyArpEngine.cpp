#include "WhammyArpEngine.h"

#include <string.h>

namespace whammy {

const MidiCompositeCommand kWhammyIntervals[kWhammyIntervalCount] = {
    {25, 0},   // 0
    {4, 25},   // +1
    {4, 51},   // +2
    {4, 76},   // +3
    {4, 101},  // +4
    {4, 127},  // +5
    {3, 109},  // +6
    {3, 127},  // +7
    {2, 85},   // +8
    {2, 95},   // +9
    {2, 106},  // +10
    {2, 116},  // +11
    {2, 127},  // +12
};

TapTempo::TapTempo() : tempoBpm_(kDefaultTempoBpm) {
  for (uint8_t i = 0; i < kTapHistory; ++i) {
    taps_[i] = 0;
    hasTap_[i] = false;
  }
}

void TapTempo::recordTap(unsigned long nowMs) {
  for (int i = kTapHistory - 1; i > 0; --i) {
    taps_[i] = taps_[i - 1];
    hasTap_[i] = hasTap_[i - 1];
  }
  taps_[0] = nowMs;
  hasTap_[0] = true;
  recomputeTempo();
}

bool TapTempo::hasTempo() const { return hasTap_[1]; }

unsigned int TapTempo::tempoBpm() const { return tempoBpm_; }

void TapTempo::recomputeTempo() {
  const unsigned long maxGapMs = 5000;
  unsigned long sum = 0;
  uint8_t count = 0;

  for (uint8_t i = 0; i + 1 < kTapHistory; ++i) {
    if (!hasTap_[i] || !hasTap_[i + 1] || taps_[i] <= taps_[i + 1]) {
      continue;
    }

    const unsigned long diff = taps_[i] - taps_[i + 1];
    if (diff <= maxGapMs) {
      sum += diff;
      ++count;
    }
  }

  if (count == 0) {
    return;
  }

  const unsigned long avgMs = sum / count;
  if (avgMs == 0) {
    return;
  }

  unsigned int bpm = static_cast<unsigned int>(kMsInMinute / avgMs);
  if (bpm < kMinTempoBpm) {
    bpm = kMinTempoBpm;
  }
  if (bpm > kMaxTempoBpm) {
    bpm = kMaxTempoBpm;
  }
  tempoBpm_ = bpm;
}

WhammyArpeggiatorEngine::WhammyArpeggiatorEngine() : sequenceCount_(0) {
  state_.isPlaying = false;
  state_.selectedSequence = 0;
  state_.currentStep = 0;
  state_.tempoBpm = kDefaultTempoBpm;
  state_.lastBeatMs = 0;
  state_.lastSubdivisionMs = 0;
  state_.beatLedOn = false;
}

void WhammyArpeggiatorEngine::sanitizeSequence(Sequence& sequence) const {
  if (sequence.stepCount == 0 || sequence.stepCount > kMaxSequenceSteps) {
    sequence.stepCount = 1;
    sequence.intervals[0] = 0;
  }

  if (sequence.initialTempoBpm == 0) {
    sequence.initialTempoBpm = kDefaultTempoBpm;
  }

  if (sequence.tickSubdivisions == 0) {
    sequence.tickSubdivisions = 4;
  }

  sequence.name[sizeof(sequence.name) - 1] = '\0';
}

void WhammyArpeggiatorEngine::loadSequences(const Sequence* sequences, uint8_t count) {
  sequenceCount_ = count > kMaxSequences ? kMaxSequences : count;
  for (uint8_t i = 0; i < sequenceCount_; ++i) {
    sequences_[i] = sequences[i];
    sanitizeSequence(sequences_[i]);
  }
  if (sequenceCount_ > 0) {
    selectSequence(0);
  }
}

bool WhammyArpeggiatorEngine::selectSequence(uint8_t index) {
  if (index >= sequenceCount_) {
    return false;
  }

  state_.selectedSequence = index;
  const Sequence& sequence = sequences_[index];
  state_.tempoBpm = sequence.initialTempoBpm;
  state_.currentStep = 0;
  return true;
}

uint8_t WhammyArpeggiatorEngine::sequenceCount() const { return sequenceCount_; }

void WhammyArpeggiatorEngine::start(unsigned long nowMs) {
  state_.isPlaying = true;
  state_.currentStep = 0;
  state_.lastBeatMs = nowMs;
  state_.lastSubdivisionMs = nowMs;
  state_.beatLedOn = true;
}

void WhammyArpeggiatorEngine::stop() {
  state_.isPlaying = false;
  state_.beatLedOn = false;
}

void WhammyArpeggiatorEngine::onTapTempo(unsigned long nowMs) {
  tapTempo_.recordTap(nowMs);
  if (tapTempo_.hasTempo()) {
    state_.tempoBpm = tapTempo_.tempoBpm();
  }
  state_.currentStep = 0;
  state_.lastBeatMs = nowMs;
  state_.lastSubdivisionMs = nowMs;
  state_.beatLedOn = true;
}

bool WhammyArpeggiatorEngine::update(unsigned long nowMs, MidiOutput& output) {
  if (!state_.isPlaying || sequenceCount_ == 0) {
    state_.beatLedOn = false;
    return false;
  }

  const unsigned long beatMs = beatIntervalMs();
  const unsigned long subdivMs = subdivisionIntervalMs();

  const Sequence& sequence = sequences_[state_.selectedSequence];

  uint8_t emitted = 0;
  while (nowMs - state_.lastSubdivisionMs >= subdivMs && emitted < kMaxTicksPerUpdate) {
    int8_t interval = sequence.intervals[state_.currentStep % sequence.stepCount];
    if (interval < 0) {
      interval = 0;
    }
    if (interval >= static_cast<int8_t>(kWhammyIntervalCount)) {
      interval = kWhammyIntervalCount - 1;
    }
    output.sendWhammyCommand(kWhammyIntervals[interval]);
    state_.currentStep = (state_.currentStep + 1) % sequence.stepCount;
    state_.lastSubdivisionMs += subdivMs;
    ++emitted;
  }

  if (nowMs - state_.lastBeatMs >= beatMs) {
    // Preserve phase when loop jitter is high by stepping in beat-sized increments.
    state_.lastBeatMs += ((nowMs - state_.lastBeatMs) / beatMs) * beatMs;
    state_.beatLedOn = true;
  } else if (nowMs - state_.lastBeatMs >= kLedPulseMs) {
    state_.beatLedOn = false;
  }

  return state_.beatLedOn;
}

const RuntimeState& WhammyArpeggiatorEngine::state() const { return state_; }

const Sequence& WhammyArpeggiatorEngine::selectedSequence() const {
  return sequences_[state_.selectedSequence];
}

unsigned long WhammyArpeggiatorEngine::beatIntervalMs() const {
  const unsigned int tempo = state_.tempoBpm == 0 ? kDefaultTempoBpm : state_.tempoBpm;
  return kMsInMinute / tempo;
}

unsigned long WhammyArpeggiatorEngine::subdivisionIntervalMs() const {
  const Sequence& sequence = sequences_[state_.selectedSequence];
  const uint8_t subdivisions = sequence.tickSubdivisions == 0 ? 4 : sequence.tickSubdivisions;
  const unsigned long interval = beatIntervalMs() / subdivisions;
  return interval == 0 ? 1 : interval;
}

}  // namespace whammy
