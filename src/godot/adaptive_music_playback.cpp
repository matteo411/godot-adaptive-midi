#include "adaptive_music_playback.h"

#include <godot_cpp/core/class_db.hpp>

#include <cmath>
#include <cstring>

namespace godot {

void AdaptiveMusicPlayback::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_layer_gain", "layer", "db", "seconds"), &AdaptiveMusicPlayback::set_layer_gain, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("set_tempo_scale", "scale", "seconds"), &AdaptiveMusicPlayback::set_tempo_scale, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("queue_section", "section", "quantize"), &AdaptiveMusicPlayback::queue_section, DEFVAL(2));
	ClassDB::bind_method(D_METHOD("play_sting", "index", "semitones", "quantize", "duck_db"), &AdaptiveMusicPlayback::play_sting, DEFVAL(0), DEFVAL(1), DEFVAL(-10.0f));
	ClassDB::bind_method(D_METHOD("set_duck_times", "attack", "release"), &AdaptiveMusicPlayback::set_duck_times);
	ClassDB::bind_method(D_METHOD("get_beat"), &AdaptiveMusicPlayback::get_beat);
	ClassDB::bind_method(D_METHOD("get_bar"), &AdaptiveMusicPlayback::get_bar);
	ClassDB::bind_method(D_METHOD("get_section"), &AdaptiveMusicPlayback::get_section);
	ClassDB::bind_method(D_METHOD("get_bpm"), &AdaptiveMusicPlayback::get_bpm);
	ClassDB::bind_method(D_METHOD("is_sting_playing"), &AdaptiveMusicPlayback::is_sting_playing);
}

void AdaptiveMusicPlayback::setup(std::unique_ptr<amidi::Engine> engine, const String &start_section) {
	engine_ = std::move(engine);
	start_section_ = start_section.utf8().get_data();
}

void AdaptiveMusicPlayback::_start(double) {
	if (engine_) engine_->start(start_section_.empty() ? nullptr : start_section_.c_str());
	begin_resample();
	active_ = true;
}

int32_t AdaptiveMusicPlayback::_mix_resampled(AudioFrame *dst, int32_t frames) {
	static_assert(sizeof(AudioFrame) == 2 * sizeof(float), "AudioFrame is two floats");
	if (!engine_ || !active_) {
		std::memset(static_cast<void *>(dst), 0, sizeof(AudioFrame) * size_t(frames));
		return frames;
	}
	engine_->render(reinterpret_cast<float *>(dst), frames);
	return frames;
}

double AdaptiveMusicPlayback::_get_playback_position() const {
	return engine_ ? engine_->beat() * 60.0 / engine_->song().bpm : 0.0;
}

void AdaptiveMusicPlayback::set_layer_gain(const String &layer, float db, float seconds) {
	if (engine_) engine_->set_layer_gain(engine_->song().find_layer(layer.utf8().get_data()), db, seconds);
}

void AdaptiveMusicPlayback::set_tempo_scale(float scale, float seconds) {
	if (engine_) engine_->set_tempo_scale(scale, seconds);
}

void AdaptiveMusicPlayback::queue_section(const String &section, int quantize) {
	if (engine_) engine_->queue_section(engine_->song().find_section(section.utf8().get_data()), amidi::Quantize(quantize));
}

void AdaptiveMusicPlayback::play_sting(int index, int semitones, int quantize, float duck_db) {
	if (engine_) engine_->play_sting(index, semitones, amidi::Quantize(quantize), duck_db);
}

void AdaptiveMusicPlayback::set_duck_times(float attack, float release) {
	if (engine_) engine_->set_duck_times(attack, release);
}

double AdaptiveMusicPlayback::get_beat() const { return engine_ ? engine_->beat() : 0.0; }

int AdaptiveMusicPlayback::get_bar() const {
	return engine_ ? int(std::floor(engine_->beat() / engine_->song().beats_per_bar)) : 0;
}

String AdaptiveMusicPlayback::get_section() const {
	if (!engine_) return String();
	const int s = engine_->section();
	const auto &secs = engine_->song().sections;
	return s >= 0 && size_t(s) < secs.size() ? String(secs[size_t(s)].name.c_str()) : String();
}

float AdaptiveMusicPlayback::get_bpm() const { return engine_ ? engine_->bpm() : 0.0f; }

bool AdaptiveMusicPlayback::is_sting_playing() const { return engine_ && engine_->sting_playing(); }

} // namespace godot
