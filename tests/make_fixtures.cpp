// Writes the test fixtures for the Godot demo's tests: a sine SoundFont and a song.
#include "fixtures.h"

#include <fstream>

int main(int argc, char **argv) {
	const std::string dir = argc > 1 ? argv[1] : ".";
	auto write = [&](const std::string &name, const fixtures::Bytes &b) {
		std::ofstream(dir + "/" + name, std::ios::binary).write(reinterpret_cast<const char *>(b.data()), std::streamsize(b.size()));
	};
	write("sine.sf2", fixtures::sine_sf2());
	std::vector<fixtures::Note> notes;
	for (int b = 0; b < 16; b++) notes.push_back({ double(b), 0.5, b < 8 ? 0 : 1, b < 8 ? 69 : 81, 100 });
	write("two_sections.mid", fixtures::smf(notes, 120.0));
	write("sting.mid", fixtures::smf({ { 0.0, 1.0, 0, 69, 110 } }, 120.0));
	return 0;
}
