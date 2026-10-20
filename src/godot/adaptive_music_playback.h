#pragma once
#include "core/engine.h"

#include <godot_cpp/classes/audio_frame.hpp>
#include <godot_cpp/classes/audio_stream_playback_resampled.hpp>

#include <atomic>
#include <memory>
#include <string>

namespace godot {

// One AdaptiveMusic playback: the engine. Its controls are safe to call from the main
// thread while the audio thread mixes; they take effect at the next mix.
class AdaptiveMusicPlayback : public AudioStreamPlaybackResampled {
	GDCLASS(AdaptiveMusicPlayback, AudioStreamPlaybackResampled)

public:
	void setup(std::unique_ptr<amidi::Engine> engine, const String &start_section);

	void set_layer_gain(const String &layer, float db, float seconds);
	void set_tempo_scale(float scale, float seconds);
	void queue_section(const String &section, int quantize);
	void play_sting(int index, int semitones, int quantize, float duck_db);
	void set_duck_times(float attack, float release);
	double get_beat() const;
	int get_bar() const;
	String get_section() const;
	float get_bpm() const;
	bool is_sting_playing() const;

	int32_t _mix_resampled(AudioFrame *dst, int32_t frames) override;
	float _get_stream_sampling_rate() const override { return amidi::Engine::SAMPLE_RATE; }
	void _start(double from_pos) override;
	void _stop() override { active_ = false; }
	bool _is_playing() const override { return active_; }
	int32_t _get_loop_count() const override { return 0; }
	double _get_playback_position() const override;
	void _seek(double) override {}

protected:
	static void _bind_methods();

private:
	std::unique_ptr<amidi::Engine> engine_;
	std::string start_section_;
	std::atomic<bool> active_{ false };
};

} // namespace godot
