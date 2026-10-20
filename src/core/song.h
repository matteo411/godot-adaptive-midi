// A song: a Standard MIDI File flattened to beat-timed events, plus the sections and
// layers the game gives it. Immutable once a playback holds it.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace amidi {

enum EventType : uint8_t { NOTE_OFF = 0x80, NOTE_ON = 0x90, CONTROL = 0xB0, PROGRAM = 0xC0, PITCH_BEND = 0xE0 };

struct Event {
	double beat;      // quarter notes from the song's start
	uint8_t type;     // EventType
	uint8_t channel;  // 0-15 (9 = drums)
	uint8_t a;        // key, controller or program
	uint8_t b;        // velocity or value (pitch bend: the 14-bit value is a | b << 7)
};

struct Section {
	std::string name;
	double from = 0.0, to = 0.0;  // beats
	int next = -1;                // the section that follows at its end; -1 = loop itself
};

struct Layer {
	std::string name;
	uint16_t channels = 0;  // bit per MIDI channel
};

struct Song {
	std::vector<Event> events;  // sorted by beat; at one beat, note-offs before note-ons
	double length_beats = 0.0;
	double bpm = 120.0;         // the first tempo in the file
	int beats_per_bar = 4;      // from the first time signature
	std::vector<Section> sections;
	std::vector<Layer> layers;

	// Parses a format 0 or 1 SMF. Later tempo changes are ignored: a song has one tempo.
	bool load_smf(const uint8_t *data, size_t size, std::string *error);
	int find_section(const std::string &name) const;
	int find_layer(const std::string &name) const;
	// Index of the first event at or after `beat`.
	size_t lower_bound(double beat) const;
};

} // namespace amidi
