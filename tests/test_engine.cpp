#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "core/engine.h"
#include "fixtures.h"

#include <cmath>

using namespace amidi;
using fixtures::Note;

namespace {

constexpr int SR = Engine::SAMPLE_RATE;

std::shared_ptr<Bank> bank() {
	static auto b = [] { auto d = fixtures::sine_sf2(); return Bank::load(d.data(), d.size(), nullptr); }();
	return b;
}

std::shared_ptr<Song> song(const std::vector<Note> &notes, double bpm = 120.0, double end_beat = -1.0) {
	auto s = std::make_shared<Song>();
	auto d = fixtures::smf(notes, bpm, end_beat);
	std::string err;
	REQUIRE(s->load_smf(d.data(), d.size(), &err));
	return s;
}

// Renders `seconds` in chunks of `chunk` frames; returns the left channel.
std::vector<float> render(Engine &e, double seconds, int chunk = 512) {
	std::vector<float> left, buf(size_t(chunk) * 2);
	for (int done = 0, total = int(seconds * SR); done < total; done += chunk) {
		const int n = std::min(chunk, total - done);
		e.render(buf.data(), n);
		for (int i = 0; i < n; i++) left.push_back(buf[size_t(i) * 2]);
	}
	return left;
}

// Frames where the sound starts after at least `gap` frames of silence.
std::vector<int> onsets(const std::vector<float> &x, float threshold = 1e-3f, int gap = 2000) {
	std::vector<int> on;
	int quiet = gap;
	for (size_t i = 0; i < x.size(); i++) {
		if (std::fabs(x[i]) > threshold) {
			if (quiet >= gap) on.push_back(int(i));
			quiet = 0;
		} else {
			quiet++;
		}
	}
	return on;
}

double rms_db(const std::vector<float> &x, int from, int to) {
	double s = 0;
	for (int i = from; i < to; i++) s += double(x[size_t(i)]) * x[size_t(i)];
	return 10 * std::log10(s / (to - from) + 1e-20);
}

double frequency(const std::vector<float> &x, int from, int to) {
	int crossings = 0;
	for (int i = from + 1; i < to; i++)
		if ((x[size_t(i - 1)] < 0) != (x[size_t(i)] < 0)) crossings++;
	return crossings / 2.0 / (double(to - from) / SR);
}

} // namespace

TEST_CASE("a MIDI file parses to beat-timed events") {
	auto s = song({ { 1.0, 0.5, 0, 69, 100 }, { 2.0, 1.0, 9, 36, 90 } }, 100.0);
	CHECK(s->bpm == doctest::Approx(100.0).epsilon(0.001));
	CHECK(s->length_beats == 4.0);
	REQUIRE(s->events.size() == 4);
	CHECK(s->events[0].beat == 1.0);
	CHECK(s->events[0].type == NOTE_ON);
	CHECK(s->events[1].beat == 1.5);
	CHECK(s->events[1].type == NOTE_OFF);
	CHECK(s->events[2].channel == 9);
}

TEST_CASE("a note lands on its frame") {
	Engine e(bank(), song({ { 1.0, 0.25, 0, 69, 110 } }));
	e.start();
	auto x = render(e, 1.0);
	auto on = onsets(x);
	REQUIRE(on.size() == 1);
	CHECK(std::abs(on[0] - SR / 2) <= 2);  // beat 1 at 120 BPM: 0.5 s
}

TEST_CASE("tempo scale 2 halves the time to a beat") {
	Engine e(bank(), song({ { 1.0, 0.25, 0, 69, 110 } }));
	e.set_tempo_scale(2.0f, 0.0f);
	e.start();
	auto on = onsets(render(e, 1.0));
	REQUIRE(on.size() == 1);
	CHECK(std::abs(on[0] - SR / 4) <= 2);
}

TEST_CASE("a looping section wraps with no lost or doubled note") {
	Engine e(bank(), song({ { 0.0, 0.25, 0, 69, 110 } }));  // 4 beats: one bar
	e.start();
	auto on = onsets(render(e, 6.0));  // 12 beats: 3 loops
	REQUIRE(on.size() == 3);
	CHECK(std::abs(on[1] - 2 * SR) <= 2);
	CHECK(std::abs(on[2] - 4 * SR) <= 2);
}

TEST_CASE("a section queued mid-bar starts on the next bar line") {
	std::vector<Note> notes;
	for (int b = 0; b < 16; b++) notes.push_back({ double(b), 0.25, 0, 69, 100 });
	auto s = song(notes);
	s->sections = { { "a", 0, 8, -1 }, { "b", 8, 16, -1 } };
	Engine e(bank(), s);
	e.start("a");
	std::vector<float> buf(128);
	auto render_beats = [&](double beats) { for (int i = 0, n = int(beats * SR / 2 / 64); i < n; i++) e.render(buf.data(), 64); };
	render_beats(1.25);
	e.queue_section(1, Quantize::BAR);
	render_beats(2.7);  // beat 3.95: still in a
	CHECK(e.section() == 0);
	render_beats(0.1);  // past beat 4: b, from its start
	CHECK(e.section() == 1);
	CHECK(e.beat() == doctest::Approx(8.05).epsilon(0.01));
}

TEST_CASE("a layer at -60 dB is silent, and a new ramp continues from the current gain") {
	auto s = song({ { 0.0, 16.0, 0, 69, 110 } }, 120.0, 16.0);
	s->layers = { { "keys", 1 << 0 } };
	{
		Engine e(bank(), s);
		e.set_layer_gain(0, -60.0f, 0.0f);
		e.start();
		auto x = render(e, 0.5);
		CHECK(rms_db(x, 0, int(x.size())) < -120.0);
	}
	Engine e(bank(), s);
	e.start();
	auto a = render(e, 0.5);
	e.set_layer_gain(0, -30.0f, 1.0f);
	auto b = render(e, 0.5);  // half way: -15 dB
	e.set_layer_gain(0, 0.0f, 1.0f);
	auto c = render(e, 0.1);
	const double before = rms_db(b, int(b.size()) - 2048, int(b.size()));
	const double after = rms_db(c, 0, 2048);
	CHECK(rms_db(a, 4096, 8192) - before == doctest::Approx(15.0).epsilon(0.1));
	CHECK(std::fabs(after - before) < 1.0);  // no jump back to 0 dB or to -30
}

TEST_CASE("a tempo change mid-note keeps its pitch") {
	Engine e(bank(), song({ { 0.0, 8.0, 0, 69, 110 } }, 120.0, 8.0));
	e.start();
	auto a = render(e, 0.5);
	e.set_tempo_scale(1.5f, 0.0f);
	auto b = render(e, 0.5);
	CHECK(frequency(a, 4096, 20000) == doctest::Approx(440.0).epsilon(0.01));
	CHECK(frequency(b, 1000, 20000) == doctest::Approx(440.0).epsilon(0.01));
}

TEST_CASE("a sting plays transposed, on the beat, with the music ducked") {
	auto music = song({ { 0.0, 16.0, 0, 57, 110 } }, 120.0, 16.0);  // A3, 220 Hz
	auto sting = song({ { 0.0, 1.0, 1, 69, 110 } });                // A4, played +12: A5
	Engine e(bank(), music, { sting });
	e.start();
	auto a = render(e, 0.6);  // beat 1.2
	e.play_sting(0, 12, Quantize::BEAT, -10.0f);
	auto b = render(e, 1.0);
	// it starts on beat 2 (0.4 s after the call), at 880 Hz over the 220 Hz music
	const int start = int(0.4 * SR);
	CHECK(e.sting_playing());  // a song is whole bars: the sting lasts 4 beats
	CHECK(rms_db(b, start - 4096, start - 64) == doctest::Approx(rms_db(a, 4096, 8192)).epsilon(0.02));
	CHECK(frequency(b, start + 2048, start + 12000) > 600.0);
}

TEST_CASE("a silent sting ducks the music once, even when replaced") {
	auto music = song({ { 0.0, 16.0, 0, 57, 110 } }, 120.0, 16.0);
	auto silent = song({}, 120.0, 4.0);  // 4 beats, no notes
	Engine e(bank(), music, { silent });
	e.set_duck_times(0.0f, 0.0f);
	e.start();
	auto a = render(e, 0.5);
	e.play_sting(0, 0, Quantize::NOW, -10.0f);
	auto b = render(e, 0.5);
	e.play_sting(0, 0, Quantize::NOW, -10.0f);  // a second one while the first plays
	auto c = render(e, 0.5);
	const double base = rms_db(a, 4096, 20000);
	CHECK(rms_db(b, 4096, 20000) - base == doctest::Approx(-10.0).epsilon(0.05));
	CHECK(rms_db(c, 4096, 20000) - base == doctest::Approx(-10.0).epsilon(0.05));
	auto d = render(e, 2.5);  // the sting's 2 s are over: the music is back
	CHECK(rms_db(d, int(d.size()) - 8192, int(d.size())) == doctest::Approx(base).epsilon(0.02));
}
