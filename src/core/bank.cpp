#define TSF_IMPLEMENTATION
#include "tsf.h"

#include "bank.h"

namespace amidi {

std::shared_ptr<Bank> Bank::load(const uint8_t *data, size_t size, std::string *error) {
	tsf *f = tsf_load_memory(data, int(size));
	if (!f) {
		if (error) *error = "not a SoundFont 2 bank";
		return nullptr;
	}
	auto bank = std::shared_ptr<Bank>(new Bank());
	bank->master_ = f;
	return bank;
}

Bank::~Bank() {
	if (master_) tsf_close(master_);
}

int Bank::preset_count() const { return tsf_get_presetcount(master_); }

std::string Bank::preset_name(int index) const {
	const char *n = tsf_get_presetname(master_, index);
	return n ? n : "";
}

} // namespace amidi
