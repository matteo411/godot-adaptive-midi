#pragma once
#include "core/song.h"

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <memory>

namespace godot {

// A MIDI song with the sections and layers the game gives it. A playback keeps the
// song as it was when the playback was made; later changes apply to new playbacks.
class MidiSong : public Resource {
	GDCLASS(MidiSong, Resource)

public:
	Error load_file(const String &path);
	Error load_bytes(const PackedByteArray &bytes);
	// A section in beats; `next` is the section that follows its end ("": it loops).
	void set_section(const String &name, double from_beat, double to_beat, const String &next);
	void clear_sections();
	void set_layer(const String &name, const PackedInt32Array &channels);
	double get_bpm() const { return song_->bpm; }
	double get_length_beats() const { return song_->length_beats; }
	int get_beats_per_bar() const { return song_->beats_per_bar; }
	PackedStringArray get_section_names() const;
	PackedStringArray get_layer_names() const;
	std::shared_ptr<const amidi::Song> song() const { return song_; }

protected:
	static void _bind_methods();

private:
	amidi::Song &edit();  // copy-on-write: a playback may hold the current one
	std::shared_ptr<amidi::Song> song_ = std::make_shared<amidi::Song>();
	std::vector<std::string> nexts_;  // the sections' `next` names, resolved when all are set
};

} // namespace godot
