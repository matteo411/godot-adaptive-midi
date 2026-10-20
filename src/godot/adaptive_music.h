#pragma once
#include "midi_song.h"
#include "sound_font_bank.h"

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace godot {

// A MIDI song played live through a SoundFont: each playback is an adaptive music
// engine (layers, tempo, sections, stings), driven through AdaptiveMusicPlayback.
class AdaptiveMusic : public AudioStream {
	GDCLASS(AdaptiveMusic, AudioStream)

public:
	enum Quantize { QUANTIZE_NOW, QUANTIZE_BEAT, QUANTIZE_BAR, QUANTIZE_SECTION_END };

	void set_bank(const Ref<SoundFontBank> &b) { bank_ = b; }
	Ref<SoundFontBank> get_bank() const { return bank_; }
	void set_song(const Ref<MidiSong> &s) { song_ = s; }
	Ref<MidiSong> get_song() const { return song_; }
	void set_stings(const TypedArray<MidiSong> &s) { stings_ = s; }
	TypedArray<MidiSong> get_stings() const { return stings_; }
	void set_start_section(const String &s) { start_section_ = s; }
	String get_start_section() const { return start_section_; }

	Ref<AudioStreamPlayback> _instantiate_playback() const override;
	String _get_stream_name() const override { return "AdaptiveMusic"; }
	double _get_length() const override { return 0.0; }  // it plays until stopped
	bool _is_monophonic() const override { return false; }
	double _get_bpm() const override;
	int32_t _get_beat_count() const override;
	int32_t _get_bar_beats() const override;

protected:
	static void _bind_methods();

private:
	Ref<SoundFontBank> bank_;
	Ref<MidiSong> song_;
	TypedArray<MidiSong> stings_;
	String start_section_;
};

} // namespace godot

VARIANT_ENUM_CAST(AdaptiveMusic::Quantize);
