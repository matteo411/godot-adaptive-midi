// amidi-render: renders a song through the engine to a 44.1 kHz float WAV, with a
// timed script of controls (for previews and checks without Godot).
//
//   amidi-render <bank.sf2> <song.mid> <out.wav> [options]
//     --length S                    seconds to render (default: the song once, plus 2 s)
//     --section NAME:FROM-TO[:NEXT] a section in beats (NEXT: the section after it)
//     --layer NAME:CH,CH,...        a layer of MIDI channels (0-15)
//     --sting FILE.mid              a sting song (index in order of the options)
//     --start NAME                  the section to start in
//     --at S:COMMAND                at S seconds, one of:
//          layer:NAME:DB:SECONDS    tempo:SCALE:SECONDS
//          section:NAME:now|beat|bar|end
//          sting:INDEX:SEMITONES:now|beat|bar|end:DUCK_DB
#include "core/engine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace amidi;

namespace {

std::vector<uint8_t> read_file(const std::string &path) {
	std::ifstream f(path, std::ios::binary);
	if (!f) { std::fprintf(stderr, "can't read %s\n", path.c_str()); std::exit(2); }
	return { std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
}

std::vector<std::string> split(const std::string &s, char sep) {
	std::vector<std::string> out;
	std::stringstream ss(s);
	for (std::string part; std::getline(ss, part, sep);) out.push_back(part);
	return out;
}

std::shared_ptr<Song> load_song(const std::string &path) {
	auto song = std::make_shared<Song>();
	auto data = read_file(path);
	std::string err;
	if (!song->load_smf(data.data(), data.size(), &err)) { std::fprintf(stderr, "%s: %s\n", path.c_str(), err.c_str()); std::exit(2); }
	return song;
}

Quantize quantize(const std::string &q) {
	return q == "beat" ? Quantize::BEAT : q == "bar" ? Quantize::BAR : q == "end" ? Quantize::SECTION_END : Quantize::NOW;
}

void write_wav(const std::string &path, const std::vector<float> &x) {
	std::ofstream f(path, std::ios::binary);
	auto u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char *>(&v), 4); };
	auto u16 = [&](uint16_t v) { f.write(reinterpret_cast<const char *>(&v), 2); };
	const uint32_t bytes = uint32_t(x.size() * 4);
	f.write("RIFF", 4); u32(36 + bytes); f.write("WAVEfmt ", 8); u32(16);
	u16(3); u16(2); u32(Engine::SAMPLE_RATE); u32(Engine::SAMPLE_RATE * 8); u16(8); u16(32);
	f.write("data", 4); u32(bytes);
	f.write(reinterpret_cast<const char *>(x.data()), bytes);
}

} // namespace

int main(int argc, char **argv) {
	if (argc < 4) { std::fprintf(stderr, "usage: amidi-render <bank.sf2> <song.mid> <out.wav> [options]\n"); return 1; }
	auto bank_data = read_file(argv[1]);
	std::string err;
	auto bank = Bank::load(bank_data.data(), bank_data.size(), &err);
	if (!bank) { std::fprintf(stderr, "%s: %s\n", argv[1], err.c_str()); return 2; }
	auto song = load_song(argv[2]);
	std::vector<std::shared_ptr<const Song>> stings;
	std::vector<std::pair<double, std::vector<std::string>>> script;
	std::vector<std::pair<std::string, std::string>> sections;  // name, spec (resolved once all are known)
	std::string start;
	double length = -1;
	for (int i = 4; i + 1 < argc; i += 2) {
		const std::string opt = argv[i], val = argv[i + 1];
		if (opt == "--length") length = std::atof(val.c_str());
		else if (opt == "--start") start = val;
		else if (opt == "--sting") stings.push_back(load_song(val));
		else if (opt == "--layer") {
			auto p = split(val, ':');
			Layer l{ p[0], 0 };
			for (auto &ch : split(p.at(1), ',')) l.channels |= uint16_t(1u << std::atoi(ch.c_str()));
			song->layers.push_back(l);
		} else if (opt == "--section") {
			auto p = split(val, ':');
			sections.push_back({ p[0], val.substr(p[0].size() + 1) });
		} else if (opt == "--at") {
			auto p = split(val, ':');
			script.push_back({ std::atof(p[0].c_str()), std::vector<std::string>(p.begin() + 1, p.end()) });
		} else {
			std::fprintf(stderr, "unknown option %s\n", opt.c_str());
			return 1;
		}
	}
	if (!sections.empty()) {
		song->sections.clear();
		for (auto &s : sections) song->sections.push_back({ s.first, 0, 0, -1 });
		for (size_t i = 0; i < sections.size(); i++) {
			auto p = split(sections[i].second, ':');
			auto range = split(p[0], '-');
			song->sections[i].from = std::atof(range.at(0).c_str());
			song->sections[i].to = std::atof(range.at(1).c_str());
			if (p.size() > 1) song->sections[i].next = song->find_section(p[1]);
		}
	}
	if (length < 0) length = song->length_beats * 60.0 / song->bpm + 2.0;
	std::sort(script.begin(), script.end(), [](auto &a, auto &b) { return a.first < b.first; });

	Engine engine(bank, song, stings);
	engine.start(start.empty() ? nullptr : start.c_str());
	const int total = int(length * Engine::SAMPLE_RATE);
	std::vector<float> out(size_t(total) * 2);
	size_t next = 0;
	for (int pos = 0; pos < total;) {
		const double t = double(pos) / Engine::SAMPLE_RATE;
		for (; next < script.size() && script[next].first <= t; next++) {
			auto &c = script[next].second;
			if (c[0] == "layer") engine.set_layer_gain(song->find_layer(c.at(1)), std::atof(c.at(2).c_str()), std::atof(c.at(3).c_str()));
			else if (c[0] == "tempo") engine.set_tempo_scale(std::atof(c.at(1).c_str()), std::atof(c.at(2).c_str()));
			else if (c[0] == "section") engine.queue_section(song->find_section(c.at(1)), quantize(c.size() > 2 ? c[2] : "bar"));
			else if (c[0] == "sting") engine.play_sting(std::atoi(c.at(1).c_str()), std::atoi(c.at(2).c_str()), quantize(c.at(3)), std::atof(c.at(4).c_str()));
			else std::fprintf(stderr, "unknown command %s\n", c[0].c_str());
		}
		// the script's next time, or 512 frames
		int n = std::min(512, total - pos);
		if (next < script.size()) n = std::max(1, std::min(n, int(script[next].first * Engine::SAMPLE_RATE) - pos));
		engine.render(out.data() + size_t(pos) * 2, n);
		pos += n;
	}
	write_wav(argv[3], out);
	std::printf("%s: %.1f s, %.1f BPM, %zu events\n", argv[3], length, song->bpm, song->events.size());
	return 0;
}
