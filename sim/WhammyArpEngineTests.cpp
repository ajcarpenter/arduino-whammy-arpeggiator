#include <assert.h>
#include <stdio.h>
#include <vector>

#include "../main/WhammyArpEngine.h"

class RecordingOutput : public whammy::MidiOutput {
 public:
  std::vector<whammy::MidiCompositeCommand> commands;

  void sendWhammyCommand(const whammy::MidiCompositeCommand& command) override {
    commands.push_back(command);
  }
};

whammy::Sequence makeSequence(const int8_t* intervals, uint8_t count,
                              unsigned int bpm, uint8_t subdivisions,
                              const char* name) {
  whammy::Sequence seq = {};
  for (uint8_t i = 0; i < count; ++i) {
    seq.intervals[i] = intervals[i];
  }
  seq.stepCount = count;
  seq.initialTempoBpm = bpm;
  seq.tickSubdivisions = subdivisions;
  snprintf(seq.name, sizeof(seq.name), "%s", name);
  return seq;
}

void testSequencePlaybackRespectsSubdivisions() {
  const int8_t notes[] = {0, 4, 7, 12};
  whammy::Sequence seq = makeSequence(notes, 4, 120, 4, "triad");

  whammy::WhammyArpeggiatorEngine engine;
  engine.loadSequences(&seq, 1);
  engine.start(0);

  RecordingOutput out;
  for (unsigned long t = 0; t <= 1000; t += 125) {
    engine.update(t, out);
  }

  assert(out.commands.size() == 8);
  assert(out.commands[0].programChange == 25);
  assert(out.commands[1].programChange == 4);
  assert(out.commands[2].programChange == 3);
  assert(out.commands[3].programChange == 2);
}

void testTapTempoChangesBpm() {
  const int8_t notes[] = {0};
  whammy::Sequence seq = makeSequence(notes, 1, 120, 1, "single");

  whammy::WhammyArpeggiatorEngine fastEngine;
  fastEngine.loadSequences(&seq, 1);
  fastEngine.start(0);
  fastEngine.onTapTempo(1000);
  fastEngine.onTapTempo(1500);
  fastEngine.onTapTempo(2000);
  // 500ms between taps => 120 BPM.
  assert(fastEngine.state().tempoBpm == 120);

  whammy::WhammyArpeggiatorEngine slowEngine;
  slowEngine.loadSequences(&seq, 1);
  slowEngine.start(0);
  slowEngine.onTapTempo(0);
  slowEngine.onTapTempo(1000);
  slowEngine.onTapTempo(2000);
  // 1000ms between taps => 60 BPM.
  assert(slowEngine.state().tempoBpm >= 59 && slowEngine.state().tempoBpm <= 61);
}

void testOutOfRangeIntervalsAreClamped() {
  const int8_t notes[] = {-10, 40};
  whammy::Sequence seq = makeSequence(notes, 2, 120, 2, "clamp");

  whammy::WhammyArpeggiatorEngine engine;
  engine.loadSequences(&seq, 1);
  engine.start(0);

  RecordingOutput out;
  engine.update(250, out);
  engine.update(500, out);

  assert(out.commands.size() == 2);
  assert(out.commands[0].programChange == 25);  // clamped to interval 0.
  assert(out.commands[1].programChange == 2);   // clamped to max interval 12.
}

void testStopPreventsFurtherOutput() {
  const int8_t notes[] = {0, 4};
  whammy::Sequence seq = makeSequence(notes, 2, 120, 2, "stop");

  whammy::WhammyArpeggiatorEngine engine;
  engine.loadSequences(&seq, 1);
  engine.start(0);

  RecordingOutput out;
  engine.update(250, out);
  assert(!out.commands.empty());

  engine.stop();
  size_t commandCountBefore = out.commands.size();
  engine.update(500, out);
  engine.update(1000, out);
  assert(out.commands.size() == commandCountBefore);
}

int main() {
  testSequencePlaybackRespectsSubdivisions();
  testTapTempoChangesBpm();
  testOutOfRangeIntervalsAreClamped();
  testStopPreventsFurtherOutput();
  printf("All Whammy arpeggiator simulation tests passed.\n");
  return 0;
}
