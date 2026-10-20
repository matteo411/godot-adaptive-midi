// The adaptive music engine: plays a song through a SoundFont with layers, a tempo
// scale, bar-quantised sections and stings. Construct, configure and destroy on the
// main thread; render() on the audio thread (it never allocates, locks or logs). The
// controls are queued to the audio thread and take effect at the next render().
#pragma once
#include "bank.h"
#include "command_queue.h"
#include "song.h"

#include <atomic>
#include <memory>
#include <vector>

namespace amidi {

enum class Quantize : int { NOW = 0, BEAT = 1, BAR = 2, SECTION_END = 3 };

class Engine {
public:
	static constexpr int SAMPLE_RATE = 44100;
	static constexpr int MAX_LAYERS = 8;
	static constexpr int MAX_STINGS = 16;
	static constexpr int MAX_VOICES = 64;
	static constexpr int BLOCK = 64;  // frames per control step (ramps advance per block)

	// `stings` are the songs play_sting() can start (by index), fixed for the engine's life.
	Engine(std::shared_ptr<Bank> bank, std::shared_ptr<const Song> song,
			std::vector<std::shared_ptr<const Song>> stings = {});
	~Engine();

	// --- main thread ---
	void start(const char *section = nullptr);  // from that section's start (default the first)
	void set_layer_gain(int layer, float db, float seconds);
	void set_tempo_scale(float scale, float seconds);
	void queue_section(int section, Quantize q);
	// Plays sting `index` transposed by `semitones`; the music ducks `duck_db` while it plays.
	void play_sting(int index, int semitones, Quantize q, float duck_db);
	void set_duck_times(float attack_s, float release_s);

	// --- audio thread ---
	// Writes (not mixes) `frames` interleaved stereo frames.
	void render(float *out, int frames);

	// --- either thread (atomics written by the audio thread) ---
	double beat() const { return beat_out_.load(std::memory_order_relaxed); }
	int section() const { return section_out_.load(std::memory_order_relaxed); }
	bool sting_playing() const { return sting_out_.load(std::memory_order_relaxed); }
	float bpm() const { return bpm_out_.load(std::memory_order_relaxed); }
	const Song &song() const { return *song_; }

private:
	enum CommandType : int { START, LAYER_GAIN, TEMPO, QUEUE_SECTION, STING, DUCK_TIMES };
	struct Command {
		CommandType type = START;
		int i = 0, j = 0, k = 0;
		float f = 0.0f, g = 0.0f;
	};
	struct Ramp {  // a value moving linearly to a target over a number of blocks
		float value = 0.0f, target = 0.0f, step = 0.0f;
		void set(float to, float seconds);
		bool advance();  // true while it moves
	};
	struct Track {  // a song's playhead on its own tsf copy
		const Song *song = nullptr;
		tsf *synth = nullptr;
		double beat = 0.0;
		size_t cursor = 0;
		int section = 0;
		int pending = -1;            // the queued section
		double beats_to_pending = 0.0;
		int transpose = 0;
		bool playing = false;
		uint8_t volume[16], expression[16];
	};

	void apply(const Command &c);
	void begin(Track &t, int section);
	void chase(Track &t, double beat);
	void send(Track &t, const Event &e);
	void update_channel_gains(Track &t, const float *layer_db, float duck_db);
	// Advances `t` by `frames`, mixing its synth into `out`. Returns false once a
	// non-looping track (a sting) has ended.
	bool advance(Track &t, float *out, int frames, double beats_per_frame, bool loops);
	double next_boundary(const Track &t, Quantize q) const;

	std::shared_ptr<Bank> bank_;
	std::shared_ptr<const Song> song_;
	std::vector<std::shared_ptr<const Song>> stings_;
	CommandQueue<Command, 256> commands_;

	Track music_, sting_;
	Ramp layer_db_[MAX_LAYERS];
	uint8_t channel_layer_[16];  // the layer a channel is in, or 0xFF
	Ramp tempo_, duck_;
	float sting_duck_db_ = 0.0f;
	float duck_attack_ = 0.05f, duck_release_ = 0.4f;
	int sting_pending_ = -1, sting_pending_transpose_ = 0;
	double sting_beats_to_start_ = 0.0;
	float pending_duck_db_ = 0.0f;
	bool started_ = false;

	std::atomic<double> beat_out_{ 0.0 };
	std::atomic<int> section_out_{ 0 };
	std::atomic<bool> sting_out_{ false };
	std::atomic<float> bpm_out_{ 0.0f };
};

} // namespace amidi
