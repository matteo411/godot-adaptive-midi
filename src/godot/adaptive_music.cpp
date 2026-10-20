#include "adaptive_music.h"
#include "adaptive_music_playback.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

namespace godot {

void AdaptiveMusic::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bank", "bank"), &AdaptiveMusic::set_bank);
	ClassDB::bind_method(D_METHOD("get_bank"), &AdaptiveMusic::get_bank);
	ClassDB::bind_method(D_METHOD("set_song", "song"), &AdaptiveMusic::set_song);
	ClassDB::bind_method(D_METHOD("get_song"), &AdaptiveMusic::get_song);
	ClassDB::bind_method(D_METHOD("set_stings", "stings"), &AdaptiveMusic::set_stings);
	ClassDB::bind_method(D_METHOD("get_stings"), &AdaptiveMusic::get_stings);
	ClassDB::bind_method(D_METHOD("set_start_section", "section"), &AdaptiveMusic::set_start_section);
	ClassDB::bind_method(D_METHOD("get_start_section"), &AdaptiveMusic::get_start_section);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "bank", PROPERTY_HINT_RESOURCE_TYPE, "SoundFontBank"), "set_bank", "get_bank");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "song", PROPERTY_HINT_RESOURCE_TYPE, "MidiSong"), "set_song", "get_song");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "stings", PROPERTY_HINT_ARRAY_TYPE, "MidiSong"), "set_stings", "get_stings");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "start_section"), "set_start_section", "get_start_section");
	BIND_ENUM_CONSTANT(QUANTIZE_NOW);
	BIND_ENUM_CONSTANT(QUANTIZE_BEAT);
	BIND_ENUM_CONSTANT(QUANTIZE_BAR);
	BIND_ENUM_CONSTANT(QUANTIZE_SECTION_END);
}

Ref<AudioStreamPlayback> AdaptiveMusic::_instantiate_playback() const {
	Ref<AdaptiveMusicPlayback> p;
	p.instantiate();
	if (bank_.is_null() || !bank_->bank() || song_.is_null()) {
		UtilityFunctions::printerr("AdaptiveMusic: a bank and a song are needed; it plays silence");
		return p;
	}
	std::vector<std::shared_ptr<const amidi::Song>> stings;
	for (int64_t i = 0; i < stings_.size(); i++) {
		Ref<MidiSong> s = stings_[i];
		if (s.is_valid()) stings.push_back(s->song());
	}
	p->setup(std::make_unique<amidi::Engine>(bank_->bank(), song_->song(), stings), start_section_);
	return p;
}

double AdaptiveMusic::_get_bpm() const { return song_.is_valid() ? song_->get_bpm() : 0.0; }

int32_t AdaptiveMusic::_get_beat_count() const {
	return song_.is_valid() ? int32_t(std::lround(song_->get_length_beats())) : 0;
}

int32_t AdaptiveMusic::_get_bar_beats() const { return song_.is_valid() ? song_->get_beats_per_bar() : 4; }

} // namespace godot
