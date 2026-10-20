#pragma once
#include "core/bank.h"

#include <godot_cpp/classes/resource.hpp>

namespace godot {

// A SoundFont 2 instrument bank, loaded once and shared by every AdaptiveMusic stream.
class SoundFontBank : public Resource {
	GDCLASS(SoundFontBank, Resource)

public:
	Error load_file(const String &path);
	Error load_bytes(const PackedByteArray &bytes);
	int get_preset_count() const;
	String get_preset_name(int index) const;
	std::shared_ptr<amidi::Bank> bank() const { return bank_; }

protected:
	static void _bind_methods();

private:
	std::shared_ptr<amidi::Bank> bank_;
};

} // namespace godot
