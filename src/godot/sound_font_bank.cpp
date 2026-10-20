#include "sound_font_bank.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void SoundFontBank::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_file", "path"), &SoundFontBank::load_file);
	ClassDB::bind_method(D_METHOD("load_bytes", "bytes"), &SoundFontBank::load_bytes);
	ClassDB::bind_method(D_METHOD("get_preset_count"), &SoundFontBank::get_preset_count);
	ClassDB::bind_method(D_METHOD("get_preset_name", "index"), &SoundFontBank::get_preset_name);
}

Error SoundFontBank::load_file(const String &path) {
	PackedByteArray bytes = FileAccess::get_file_as_bytes(path);
	if (bytes.is_empty()) {
		UtilityFunctions::printerr("SoundFontBank: can't read ", path);
		return ERR_FILE_CANT_OPEN;
	}
	return load_bytes(bytes);
}

Error SoundFontBank::load_bytes(const PackedByteArray &bytes) {
	std::string err;
	auto b = amidi::Bank::load(bytes.ptr(), size_t(bytes.size()), &err);
	if (!b) {
		UtilityFunctions::printerr("SoundFontBank: ", err.c_str());
		return ERR_FILE_CORRUPT;
	}
	bank_ = b;
	return OK;
}

int SoundFontBank::get_preset_count() const { return bank_ ? bank_->preset_count() : 0; }

String SoundFontBank::get_preset_name(int index) const {
	return bank_ ? String(bank_->preset_name(index).c_str()) : String();
}

} // namespace godot
