// Test fixtures built in memory: a one-sample SoundFont (a looping 440 Hz sine on
// presets 0:0 and 128:0) and small Standard MIDI Files.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fixtures {

using Bytes = std::vector<uint8_t>;

inline void u16(Bytes &b, uint32_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); }
inline void u32(Bytes &b, uint32_t v) { u16(b, v & 0xFFFF); u16(b, v >> 16); }
inline void tag(Bytes &b, const char *t) { b.insert(b.end(), t, t + 4); }
inline void name(Bytes &b, const char *n, size_t len) {
	for (size_t i = 0; i < len; i++) b.push_back(i < std::strlen(n) ? uint8_t(n[i]) : 0);
}
inline Bytes chunk(const char *id, const Bytes &data) {
	Bytes b; tag(b, id); u32(b, uint32_t(data.size())); b.insert(b.end(), data.begin(), data.end());
	if (data.size() & 1) b.push_back(0);
	return b;
}
inline Bytes list(const char *type, const std::vector<Bytes> &chunks) {
	Bytes d; tag(d, type);
	for (auto &c : chunks) d.insert(d.end(), c.begin(), c.end());
	return chunk("LIST", d);
}

inline Bytes sine_sf2() {
	const int rate = 44000, n = 1000;  // 440 Hz = 100 samples a cycle: the loop is whole cycles
	Bytes smpl;
	for (int i = 0; i < n + 46; i++) u16(smpl, uint16_t(int16_t(i < n ? 20000 * std::sin(2 * M_PI * i / 100.0) : 0)));
	Bytes ifil; u16(ifil, 2); u16(ifil, 1);
	Bytes phdr;
	auto preset = [&](const char *nm, int p, int bank, int bag) { name(phdr, nm, 20); u16(phdr, p); u16(phdr, bank); u16(phdr, bag); u32(phdr, 0); u32(phdr, 0); u32(phdr, 0); };
	preset("Sine", 0, 0, 0); preset("Drums", 0, 128, 1); preset("EOP", 0, 0, 2);
	Bytes pbag; u16(pbag, 0); u16(pbag, 0); u16(pbag, 1); u16(pbag, 0); u16(pbag, 2); u16(pbag, 0);
	Bytes pmod(10, 0);
	Bytes pgen; u16(pgen, 41); u16(pgen, 0); u16(pgen, 41); u16(pgen, 0); u16(pgen, 0); u16(pgen, 0);
	Bytes inst; name(inst, "Sine", 20); u16(inst, 0); name(inst, "EOI", 20); u16(inst, 1);
	Bytes ibag; u16(ibag, 0); u16(ibag, 0); u16(ibag, 2); u16(ibag, 0);
	Bytes imod(10, 0);
	Bytes igen; u16(igen, 54); u16(igen, 1); u16(igen, 53); u16(igen, 0); u16(igen, 0); u16(igen, 0);
	Bytes shdr;
	name(shdr, "sine", 20); u32(shdr, 0); u32(shdr, n); u32(shdr, 0); u32(shdr, n); u32(shdr, rate);
	shdr.push_back(69); shdr.push_back(0); u16(shdr, 0); u16(shdr, 1);
	name(shdr, "EOS", 20); for (int i = 0; i < 26; i++) shdr.push_back(0);
	Bytes body; tag(body, "sfbk");
	for (auto &c : { list("INFO", { chunk("ifil", ifil), chunk("isng", Bytes{ 'E', 'M', 'U', '8', '0', '0', '0', 0 }), chunk("INAM", Bytes{ 't', 'e', 's', 't', 0, 0 }) }),
			 list("sdta", { chunk("smpl", smpl) }),
			 list("pdta", { chunk("phdr", phdr), chunk("pbag", pbag), chunk("pmod", pmod), chunk("pgen", pgen),
								  chunk("inst", inst), chunk("ibag", ibag), chunk("imod", imod), chunk("igen", igen), chunk("shdr", shdr) }) })
		body.insert(body.end(), c.begin(), c.end());
	return chunk("RIFF", body);
}

struct Note { double beat, length; int channel, key, vel; };

inline Bytes smf(const std::vector<Note> &notes, double bpm = 120.0, double end_beat = -1.0, int tpq = 480) {
	struct Ev { long tick; int order; Bytes data; };
	std::vector<Ev> evs;
	for (auto &n : notes) {
		evs.push_back({ long(std::lround(n.beat * tpq)), 1, { uint8_t(0x90 | n.channel), uint8_t(n.key), uint8_t(n.vel) } });
		evs.push_back({ long(std::lround((n.beat + n.length) * tpq)), 0, { uint8_t(0x80 | n.channel), uint8_t(n.key), 0 } });
	}
	if (end_beat > 0) evs.push_back({ long(std::lround(end_beat * tpq)), 0, { 0xB0, 1, 0 } });  // a CC marks the length
	std::stable_sort(evs.begin(), evs.end(), [](const Ev &a, const Ev &b) { return a.tick != b.tick ? a.tick < b.tick : a.order < b.order; });
	Bytes trk;
	const uint32_t us = uint32_t(60000000.0 / bpm);
	trk.insert(trk.end(), { 0x00, 0xFF, 0x51, 0x03, uint8_t(us >> 16), uint8_t(us >> 8), uint8_t(us) });
	long last = 0;
	for (auto &e : evs) {
		uint32_t d = uint32_t(e.tick - last); last = e.tick;
		uint8_t buf[5]; int k = 0; buf[k++] = d & 0x7F;
		while (d >>= 7) buf[k++] = 0x80 | (d & 0x7F);
		while (k) trk.push_back(buf[--k]);
		trk.insert(trk.end(), e.data.begin(), e.data.end());
	}
	trk.insert(trk.end(), { 0x00, 0xFF, 0x2F, 0x00 });
	Bytes b = { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, uint8_t(tpq >> 8), uint8_t(tpq & 0xFF), 'M', 'T', 'r', 'k' };
	const uint32_t len = uint32_t(trk.size());
	b.insert(b.end(), { uint8_t(len >> 24), uint8_t(len >> 16), uint8_t(len >> 8), uint8_t(len) });
	b.insert(b.end(), trk.begin(), trk.end());
	return b;
}

} // namespace fixtures
