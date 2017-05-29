#include <MIDI.h>
#include <EEPROM.h>

#define CONFIG_VERSION "ls1"
#define CONFIG_START 32

struct Sequence;

MIDI_CREATE_DEFAULT_INSTANCE();

const int TEMPO_BUTTON_PIN = 2;
const int TEMPO_LED_PIN = 13;
const unsigned long TEMPO_LED_BLINK_TIME = 100.0;
const unsigned long MS_IN_MINUTE = 60 * 1000.0;

unsigned long tapTempoLastPressTimes[4];
unsigned long tempo = 60.0;
unsigned long lastTick = 0.0;
unsigned long lastTickSubdivision = 0.0;
int tickSubdivisionCount = 0;
int tickSubdivisions = 4;
bool isPlaying = false;

struct Sequence {
	int notes[16];
	int noteCount;
	unsigned long initialTempo;
	char name[16];
	int tickSubdivisions;
};

struct StoreStruct {
	Sequence sequences[10];
	char version_of_program[4];
} settings = {
	{
		{
			{ 1, 2, 3, 5 },
			4,
			120.0,
			"Sequence 1",
			4,
		}
	},
	CONFIG_VERSION
};


Sequence selectedSequence;

struct MidiCompositeCommand {
	int programChange;
	int cc;
};

struct MidiCompositeCommand notes[] = {
	{ 25, 0 }, // 0
	{ 4, 25 }, // +1
	{ 4, 51 }, // +2
	{ 4, 76 }, // +3
	{ 4, 101 }, // +4
	{ 4, 127 }, // +5
	{ 3, 109 }, // +6
	{ 3, 127 }, // +7
	{ 2, 85 }, // +8
	{ 2, 95 }, // +9
	{ 2, 106 }, // +10
	{ 2, 116 }, // +11
	{ 2, 127 }, // +12
};

void loadConfig() {
	if (
		EEPROM.read(CONFIG_START + sizeof(settings) - 2) == settings.version_of_program[2] &&
		EEPROM.read(CONFIG_START + sizeof(settings) - 3) == settings.version_of_program[1] &&
		EEPROM.read(CONFIG_START + sizeof(settings) - 4) == settings.version_of_program[0]
	){
		for (unsigned int t=0; t<sizeof(settings); t++) {
			*((char*)&settings + t) = EEPROM.read(CONFIG_START + t);
		}
	} else {
		saveConfig();
	}
}

void saveConfig() {
	for (unsigned int t=0; t<sizeof(settings); t++){
		EEPROM.write(CONFIG_START + t, *((char*)&settings + t));

		if (EEPROM.read(CONFIG_START + t) != *((char*)&settings + t)){
			// error writing to EEPROM
		}
	}
}

void setup() {
	loadConfig();
	selectSequence(0);

	MIDI.begin(MIDI_CHANNEL_OMNI);
	pinMode(TEMPO_LED_PIN, OUTPUT);
	attachInterrupt(digitalPinToInterrupt(TEMPO_BUTTON_PIN), onButtonPress, RISING);
}

void loop() {
	unsigned long timeNow = millis();
	unsigned long nextTickDueTime = lastTick + (MS_IN_MINUTE / tempo);
	unsigned long nextTickSubdivisionDueTime = lastTickSubdivision + ((MS_IN_MINUTE / tempo) / tickSubdivisions);
	unsigned long tempoLedOffDueTime = lastTick + TEMPO_LED_BLINK_TIME;

	bool nextTickDue = timeNow > nextTickDueTime;
	bool nextTickSubdivisionDue = timeNow > nextTickSubdivisionDueTime;
	bool tempoLedOffDue = timeNow > tempoLedOffDueTime;

	if (nextTickSubdivisionDue && isPlaying) {
		MidiCompositeCommand note = notes[selectedSequence.notes[tickSubdivisionCount]];
		MIDI.sendControlChange(11, note.cc, 1);
		MIDI.sendProgramChange(note.programChange - 1, 1);
		
		tickSubdivisionCount = (tickSubdivisionCount + 1) % (selectedSequence.noteCount);
		lastTickSubdivision = timeNow;
	}

	if (nextTickDue && isPlaying) {
		lastTick = timeNow;
		digitalWrite(TEMPO_LED_PIN, HIGH);
	} else if (tempoLedOffDue) {
		digitalWrite(TEMPO_LED_PIN, LOW);
	}
}

void start() {
	isPlaying = true;
	lastTick = lastTickSubdivision = millis();
}

void stop() {
	isPlaying = false;
}

void selectSequence(int index) {
	selectedSequence = settings.sequences[index];
	tempo = selectedSequence.initialTempo;
	tickSubdivisions = selectedSequence.tickSubdivisions;
}

void onButtonPress() {
	addTapTempoPressTime(millis());
}

void addTapTempoPressTime(unsigned long pressTime) {
	tapTempoLastPressTimes[3] = tapTempoLastPressTimes[2];
	tapTempoLastPressTimes[2] = tapTempoLastPressTimes[1];
	tapTempoLastPressTimes[1] = tapTempoLastPressTimes[0];
	tapTempoLastPressTimes[0] = pressTime;
	lastTick = lastTickSubdivision = millis();
	tickSubdivisionCount = 0;
	calculateTempo();
}

void calculateTempo() {
	unsigned long diffCutoff = 5000;
	int count = 0;
	unsigned long total = 0;

	for (int i = 0; i < (sizeof(tapTempoLastPressTimes) / sizeof(long)) - 2; i++) {
		unsigned long diff = tapTempoLastPressTimes[i] - tapTempoLastPressTimes[i + 1];
		if (diff < diffCutoff) {
			count++;
			total += diff;
		}
	}

	if (count > 0) {
		tempo = MS_IN_MINUTE / (total / count) ;
	}
}

