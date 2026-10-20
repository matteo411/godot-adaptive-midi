// A SoundFont loaded once; every playback renders through its own tsf copy, sharing
// the samples. Load and free on the main thread.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct tsf;

namespace amidi {

class Bank {
public:
	static std::shared_ptr<Bank> load(const uint8_t *data, size_t size, std::string *error);
	~Bank();
	tsf *master() const { return master_; }
	int preset_count() const;
	std::string preset_name(int index) const;

private:
	tsf *master_ = nullptr;
};

} // namespace amidi
