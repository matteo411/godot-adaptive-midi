#include "midi_song.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void MidiSong::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_file", "path"), &MidiSong::load_file);
	ClassDB::bind_method(D_METHOD("load_bytes", "bytes"), &MidiSong::load_bytes);
	ClassDB::bind_method(D_METHOD("set_section", "name", "from_beat", "to_beat", "next"), &MidiSong::set_section, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("clear_sections"), &MidiSong::clear_sections);
	ClassDB::bind_method(D_METHOD("set_layer", "name", "channels"), &MidiSong::set_layer);
	ClassDB::bind_method(D_METHOD("get_bpm"), &MidiSong::get_bpm);
	ClassDB::bind_method(D_METHOD("get_length_beats"), &MidiSong::get_length_beats);
	ClassDB::bind_method(D_METHOD("get_beats_per_bar"), &MidiSong::get_beats_per_bar);
	ClassDB::bind_method(D_METHOD("get_section_names"), &MidiSong::get_section_names);
	ClassDB::bind_method(D_METHOD("get_layer_names"), &MidiSong::get_layer_names);
}

amidi::Song &MidiSong::edit() {
	if (song_.use_count() > 1) song_ = std::make_shared<amidi::Song>(*song_);
	return *song_;
}

Error MidiSong::load_file(const String &path) {
	PackedByteArray bytes = FileAccess::get_file_as_bytes(path);
	if (bytes.is_empty()) {
		UtilityFunctions::printerr("MidiSong: can't read ", path);
		return ERR_FILE_CANT_OPEN;
	}
	return load_bytes(bytes);
}

Error MidiSong::load_bytes(const PackedByteArray &bytes) {
	auto s = std::make_shared<amidi::Song>();
	std::string err;
	if (!s->load_smf(bytes.ptr(), size_t(bytes.size()), &err)) {
		UtilityFunctions::printerr("MidiSong: ", err.c_str());
		return ERR_FILE_CORRUPT;
	}
	song_ = s;
	nexts_.assign(song_->sections.size(), "");
	return OK;
}

void MidiSong::clear_sections() {
	edit().sections.clear();
	nexts_.clear();
}

void MidiSong::set_section(const String &name, double from_beat, double to_beat, const String &next) {
	amidi::Song &s = edit();
	if (s.sections.size() == 1 && s.sections[0].name == "all") {  // the default replaced by the first real one
		s.sections.clear();
		nexts_.clear();
	}
	const std::string n = name.utf8().get_data();
	int i = s.find_section(n);
	if (i < 0) {
		s.sections.push_back({ n, 0, 0, -1 });
		nexts_.push_back("");
		i = int(s.sections.size()) - 1;
	}
	s.sections[size_t(i)].from = from_beat;
	s.sections[size_t(i)].to = to_beat > from_beat ? to_beat : from_beat + s.beats_per_bar;
	nexts_[size_t(i)] = next.utf8().get_data();
	for (size_t k = 0; k < s.sections.size(); k++)
		s.sections[k].next = nexts_[k].empty() ? -1 : s.find_section(nexts_[k]);
}

void MidiSong::set_layer(const String &name, const PackedInt32Array &channels) {
	amidi::Song &s = edit();
	const std::string n = name.utf8().get_data();
	uint16_t mask = 0;
	for (int64_t k = 0; k < channels.size(); k++)
		if (channels[k] >= 0 && channels[k] < 16) mask |= uint16_t(1u << channels[k]);
	int i = s.find_layer(n);
	if (i < 0) s.layers.push_back({ n, mask });
	else s.layers[size_t(i)].channels = mask;
}

PackedStringArray MidiSong::get_section_names() const {
	PackedStringArray out;
	for (auto &s : song_->sections) out.push_back(String(s.name.c_str()));
	return out;
}

PackedStringArray MidiSong::get_layer_names() const {
	PackedStringArray out;
	for (auto &l : song_->layers) out.push_back(String(l.name.c_str()));
	return out;
}

} // namespace godot
