#include "song.h"

#include <algorithm>
#include <cmath>

namespace amidi {

namespace {

struct Reader {
	const uint8_t *p, *end;
	bool ok() const { return p <= end; }
	uint32_t u32() { uint32_t v = 0; for (int i = 0; i < 4 && p < end; i++) v = v << 8 | *p++; return v; }
	uint16_t u16() { uint16_t v = 0; for (int i = 0; i < 2 && p < end; i++) v = uint16_t(v << 8 | *p++); return v; }
	uint32_t var() {
		uint32_t v = 0;
		for (int i = 0; i < 4 && p < end; i++) {
			uint8_t c = *p++;
			v = v << 7 | (c & 0x7F);
			if (!(c & 0x80)) break;
		}
		return v;
	}
};

} // namespace

bool Song::load_smf(const uint8_t *data, size_t size, std::string *error) {
	auto fail = [&](const char *msg) { if (error) *error = msg; return false; };
	Reader r{ data, data + size };
	if (size < 14 || r.u32() != 0x4D546864) return fail("not a MIDI file (no MThd)");
	uint32_t hlen = r.u32();
	uint16_t format = r.u16(), ntracks = r.u16(), division = r.u16();
	if (format > 1) return fail("MIDI format 2 is not supported");
	if (division & 0x8000) return fail("SMPTE time division is not supported");
	r.p = data + 8 + hlen;
	const double tpq = division ? division : 480;
	events.clear();
	bool have_tempo = false, have_sig = false;
	double last = 0.0;
	for (int t = 0; t < ntracks && r.p + 8 <= r.end; t++) {
		uint32_t id = r.u32(), len = r.u32();
		const uint8_t *track_end = std::min(r.p + len, r.end);
		if (id != 0x4D54726B) { r.p = track_end; continue; }
		uint64_t tick = 0;
		uint8_t status = 0;
		while (r.p < track_end) {
			tick += r.var();
			uint8_t c = *r.p;
			if (c & 0x80) { status = c; r.p++; } else if (!status) return fail("running status without a status byte");
			const double beat = tick / tpq;
			if (status == 0xFF) {
				uint8_t meta = *r.p++;
				uint32_t mlen = r.var();
				if (meta == 0x51 && mlen == 3 && !have_tempo) {
					uint32_t us = r.p[0] << 16 | r.p[1] << 8 | r.p[2];
					if (us) bpm = 60000000.0 / us;
					have_tempo = true;
				} else if (meta == 0x58 && mlen >= 2 && !have_sig) {
					beats_per_bar = std::max(1, int(r.p[0] * 4 / (1 << r.p[1])));
					have_sig = true;
				}
				r.p += mlen;
				status = 0;  // meta and sysex events cancel running status
				continue;
			}
			if (status == 0xF0 || status == 0xF7) { r.p += r.var(); status = 0; continue; }
			uint8_t type = status & 0xF0, ch = status & 0x0F;
			uint8_t a = *r.p++, b = 0;
			if (type != 0xC0 && type != 0xD0) b = *r.p++;
			if (type == NOTE_ON && b == 0) type = NOTE_OFF;
			if (type == NOTE_OFF || type == NOTE_ON || type == CONTROL || type == PROGRAM || type == PITCH_BEND)
				events.push_back({ beat, type, ch, a, b });
			last = std::max(last, beat);
		}
		r.p = track_end;
	}
	if (!r.ok()) return fail("truncated MIDI file");
	// at one beat: controls and programs, then note-offs, then note-ons
	auto rank = [](uint8_t type) { return type == NOTE_ON ? 2 : type == NOTE_OFF ? 1 : 0; };
	std::stable_sort(events.begin(), events.end(), [&](const Event &x, const Event &y) {
		return x.beat != y.beat ? x.beat < y.beat : rank(x.type) < rank(y.type);
	});
	// the length is whole bars
	length_beats = std::max(1.0, std::ceil(last / beats_per_bar - 1e-6)) * beats_per_bar;
	if (sections.empty()) sections.push_back({ "all", 0.0, length_beats, -1 });
	return true;
}

int Song::find_section(const std::string &name) const {
	for (size_t i = 0; i < sections.size(); i++)
		if (sections[i].name == name) return int(i);
	return -1;
}

int Song::find_layer(const std::string &name) const {
	for (size_t i = 0; i < layers.size(); i++)
		if (layers[i].name == name) return int(i);
	return -1;
}

size_t Song::lower_bound(double beat) const {
	return size_t(std::lower_bound(events.begin(), events.end(), beat - 1e-9,
			[](const Event &e, double b) { return e.beat < b; }) - events.begin());
}

} // namespace amidi
