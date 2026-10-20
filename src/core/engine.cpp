#include "engine.h"

#include "tsf.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace amidi {

namespace {

constexpr double EPS = 1e-9;
constexpr float SILENT_DB = -60.0f;  // a layer at or under this is off

tsf *make_synth(tsf *master) {
	tsf *f = tsf_copy(master);
	tsf_set_output(f, TSF_STEREO_INTERLEAVED, Engine::SAMPLE_RATE, 0.0f);
	tsf_set_max_voices(f, Engine::MAX_VOICES);
	for (int ch = 0; ch < 16; ch++) tsf_channel_set_volume(f, ch, 1.0f);  // allocates every channel now
	tsf_channel_set_bank_preset(f, 9, 128, 0);
	return f;
}

} // namespace

void Engine::Ramp::set(float to, float seconds) {
	target = to;
	const float blocks = seconds * SAMPLE_RATE / BLOCK;
	if (blocks <= 1.0f) {
		value = to;
		step = 0.0f;
	} else {
		step = (to - value) / blocks;
	}
}

bool Engine::Ramp::advance() {
	if (value == target) return false;
	value += step;
	if ((step >= 0.0f && value >= target) || (step < 0.0f && value <= target)) value = target;
	return true;
}

Engine::Engine(std::shared_ptr<Bank> bank, std::shared_ptr<const Song> song,
		std::vector<std::shared_ptr<const Song>> stings) :
		bank_(std::move(bank)), song_(std::move(song)), stings_(std::move(stings)) {
	if (stings_.size() > MAX_STINGS) stings_.resize(MAX_STINGS);
	music_.song = song_.get();
	music_.synth = make_synth(bank_->master());
	sting_.synth = make_synth(bank_->master());
	std::memset(channel_layer_, 0xFF, sizeof(channel_layer_));
	for (size_t l = 0; l < song_->layers.size() && l < MAX_LAYERS; l++)
		for (int ch = 0; ch < 16; ch++)
			if (song_->layers[l].channels & (1u << ch) && channel_layer_[ch] == 0xFF) channel_layer_[ch] = uint8_t(l);
	tempo_.value = tempo_.target = 1.0f;
	bpm_out_.store(float(song_->bpm));
}

Engine::~Engine() {
	tsf_close(music_.synth);
	tsf_close(sting_.synth);
}

// --- main thread: queue the controls ---

void Engine::start(const char *section) {
	int s = section ? song_->find_section(section) : 0;
	commands_.push({ START, std::max(s, 0) });
}

void Engine::set_layer_gain(int layer, float db, float seconds) {
	if (layer >= 0 && layer < MAX_LAYERS) commands_.push({ LAYER_GAIN, layer, 0, 0, db, seconds });
}

void Engine::set_tempo_scale(float scale, float seconds) {
	commands_.push({ TEMPO, 0, 0, 0, std::clamp(scale, 0.25f, 4.0f), seconds });
}

void Engine::queue_section(int section, Quantize q) {
	if (section >= 0 && size_t(section) < song_->sections.size()) commands_.push({ QUEUE_SECTION, section, int(q) });
}

void Engine::play_sting(int index, int semitones, Quantize q, float duck_db) {
	if (index >= 0 && size_t(index) < stings_.size()) commands_.push({ STING, index, semitones, int(q), duck_db });
}

void Engine::set_duck_times(float attack_s, float release_s) {
	commands_.push({ DUCK_TIMES, 0, 0, 0, attack_s, release_s });
}

// --- audio thread ---

void Engine::apply(const Command &c) {
	switch (c.type) {
		case START:
			begin(music_, c.i);
			started_ = true;
			break;
		case LAYER_GAIN:
			layer_db_[c.i].set(c.f, c.g);
			break;
		case TEMPO:
			tempo_.set(c.f, c.g);
			break;
		case QUEUE_SECTION:
			if (!music_.playing) {
				begin(music_, c.i);
			} else {
				music_.pending = c.i;
				music_.beats_to_pending = next_boundary(music_, Quantize(c.j)) - music_.beat;
			}
			break;
		case STING:
			sting_pending_ = c.i;
			sting_pending_transpose_ = c.j;
			pending_duck_db_ = c.f;
			sting_beats_to_start_ = music_.playing ? next_boundary(music_, Quantize(c.k)) - music_.beat : 0.0;
			break;
		case DUCK_TIMES:
			duck_attack_ = c.f;
			duck_release_ = c.g;
			break;
	}
}

double Engine::next_boundary(const Track &t, Quantize q) const {
	switch (q) {
		case Quantize::NOW: return t.beat;
		case Quantize::BEAT: return std::ceil(t.beat - EPS);
		case Quantize::BAR: {
			const double bpb = t.song->beats_per_bar;
			return std::ceil(t.beat / bpb - EPS) * bpb;
		}
		case Quantize::SECTION_END: return t.song->sections[t.section].to;
	}
	return t.beat;
}

void Engine::begin(Track &t, int section) {
	tsf_note_off_all(t.synth);
	t.section = section;
	t.pending = -1;
	t.beat = t.song->sections[section].from;
	chase(t, t.beat);
	t.cursor = t.song->lower_bound(t.beat);
	t.playing = true;
}

void Engine::chase(Track &t, double beat) {
	// the programs and controllers in force at `beat`, as if the song had played to it
	std::memset(t.volume, 100, sizeof(t.volume));
	std::memset(t.expression, 127, sizeof(t.expression));
	for (int ch = 0; ch < 16; ch++) {
		tsf_channel_set_pitchwheel(t.synth, ch, 8192);
		if (ch != 9) tsf_channel_set_presetnumber(t.synth, ch, 0, 0);
	}
	tsf_channel_set_bank_preset(t.synth, 9, 128, 0);
	for (const Event &e : t.song->events) {
		if (e.beat >= beat - EPS) break;
		if (e.type != NOTE_ON && e.type != NOTE_OFF) send(t, e);
	}
}

void Engine::send(Track &t, const Event &e) {
	const int ch = e.channel;
	switch (e.type) {
		case NOTE_ON:
		case NOTE_OFF: {
			const int key = ch == 9 ? e.a : std::clamp(int(e.a) + t.transpose, 0, 127);
			if (e.type == NOTE_ON) tsf_channel_note_on(t.synth, ch, key, e.b / 127.0f);
			else tsf_channel_note_off(t.synth, ch, key);
			break;
		}
		case CONTROL:
			if (e.a == 7) t.volume[ch] = e.b;
			else if (e.a == 11) t.expression[ch] = e.b;
			else tsf_channel_midi_control(t.synth, ch, e.a, e.b);
			break;
		case PROGRAM:
			tsf_channel_set_presetnumber(t.synth, ch, e.a, ch == 9);
			break;
		case PITCH_BEND:
			tsf_channel_set_pitchwheel(t.synth, ch, e.a | e.b << 7);
			break;
	}
}

void Engine::update_channel_gains(Track &t, const float *layer_db, float duck_db) {
	for (int ch = 0; ch < 16; ch++) {
		// TinySoundFont's own MIDI volume curve: (volume x expression) cubed
		const float midi = std::pow(t.volume[ch] / 127.0f * (t.expression[ch] / 127.0f), 3.0f);
		const float ldb = layer_db && channel_layer_[ch] != 0xFF ? layer_db[channel_layer_[ch]] : 0.0f;
		const float gain = ldb <= SILENT_DB ? 0.0f : midi * std::pow(10.0f, (ldb + duck_db) / 20.0f);
		tsf_channel_set_volume(t.synth, ch, gain);
	}
}

bool Engine::advance(Track &t, float *out, int frames, double bpf, bool loops) {
	const Song &song = *t.song;
	int done = 0;
	for (int guard = 0; done < frames && guard < 4096; guard++) {
		if (!t.playing) {  // a finished sting: its release tail
			tsf_render_float(t.synth, out + done * 2, frames - done, 1);
			return tsf_active_voice_count(t.synth) > 0;
		}
		const Section &s = song.sections[t.section];
		double target = s.to;
		if (t.cursor < song.events.size()) target = std::min(target, song.events[t.cursor].beat);
		if (t.pending >= 0) target = std::min(target, t.beat + t.beats_to_pending);
		int step = target <= t.beat + EPS ? 0 : int(std::ceil((target - t.beat) / bpf - EPS));
		step = std::min(step, frames - done);
		if (step > 0) {
			tsf_render_float(t.synth, out + done * 2, step, 1);
			const double adv = step * bpf;
			t.beat += adv;
			if (t.pending >= 0) t.beats_to_pending -= adv;
			done += step;
		}
		while (t.cursor < song.events.size() && song.events[t.cursor].beat <= t.beat + EPS &&
				song.events[t.cursor].beat < s.to - EPS)
			send(t, song.events[t.cursor++]);
		if (t.pending >= 0 && t.beats_to_pending <= EPS) {
			const double over = -t.beats_to_pending;
			begin(t, t.pending);
			t.beat += over;
			continue;
		}
		if (t.beat >= s.to - EPS) {
			if (!loops) {
				tsf_note_off_all(t.synth);
				t.playing = false;
				continue;
			}
			const double over = t.beat - s.to;
			const int next = s.next >= 0 ? s.next : t.section;
			if (next != t.section) {
				begin(t, next);
			} else {
				tsf_note_off_all(t.synth);
				t.beat = s.from;
				t.cursor = song.lower_bound(s.from);
			}
			t.beat += over;
		}
	}
	return true;
}

void Engine::render(float *out, int frames) {
	Command c;
	while (commands_.pop(c)) apply(c);
	std::memset(out, 0, sizeof(float) * 2 * size_t(frames));
	float layer_db[MAX_LAYERS];
	for (int pos = 0; pos < frames;) {
		const int n = std::min(BLOCK, frames - pos);
		tempo_.advance();
		duck_.advance();
		for (int l = 0; l < MAX_LAYERS; l++) {
			layer_db_[l].advance();
			layer_db[l] = layer_db_[l].value;
		}
		const double music_bpf = song_->bpm * tempo_.value / 60.0 / SAMPLE_RATE;
		if (started_ && music_.playing) {
			update_channel_gains(music_, layer_db, duck_.value);
			advance(music_, out + pos * 2, n, music_bpf, true);
		}
		if (sting_pending_ >= 0) {
			sting_beats_to_start_ -= music_.playing ? n * music_bpf : 1e9;
			if (sting_beats_to_start_ <= EPS) {
				sting_.song = stings_[sting_pending_].get();
				sting_.transpose = sting_pending_transpose_;
				begin(sting_, 0);
				sting_.section = 0;
				sting_pending_ = -1;
				sting_duck_db_ = pending_duck_db_;
				duck_.set(sting_duck_db_, duck_attack_);
			}
		}
		if (sting_.song && (sting_.playing || tsf_active_voice_count(sting_.synth) > 0)) {
			const bool was_playing = sting_.playing;
			update_channel_gains(sting_, nullptr, 0.0f);
			const double bpf = music_.playing ? music_bpf : sting_.song->bpm / 60.0 / SAMPLE_RATE;
			advance(sting_, out + pos * 2, n, bpf, false);
			if (was_playing && !sting_.playing) duck_.set(0.0f, duck_release_);
		}
		pos += n;
	}
	beat_out_.store(music_.beat, std::memory_order_relaxed);
	section_out_.store(music_.section, std::memory_order_relaxed);
	sting_out_.store(sting_.playing, std::memory_order_relaxed);
	bpm_out_.store(float(song_->bpm * tempo_.value), std::memory_order_relaxed);
}

} // namespace amidi
