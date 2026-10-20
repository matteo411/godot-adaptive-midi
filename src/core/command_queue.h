// Single-producer (main thread), single-consumer (audio thread) ring of commands.
#pragma once
#include <atomic>
#include <cstddef>

namespace amidi {

template <typename T, size_t N>
class CommandQueue {
	static_assert((N & (N - 1)) == 0, "N must be a power of two");

public:
	bool push(const T &item) {
		const size_t w = write_.load(std::memory_order_relaxed);
		if (w - read_.load(std::memory_order_acquire) == N) return false;  // full: the command is dropped
		items_[w & (N - 1)] = item;
		write_.store(w + 1, std::memory_order_release);
		return true;
	}
	bool pop(T &item) {
		const size_t r = read_.load(std::memory_order_relaxed);
		if (r == write_.load(std::memory_order_acquire)) return false;
		item = items_[r & (N - 1)];
		read_.store(r + 1, std::memory_order_release);
		return true;
	}

private:
	T items_[N];
	std::atomic<size_t> write_{ 0 }, read_{ 0 };
};

} // namespace amidi
