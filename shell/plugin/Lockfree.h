// Lockfree.h — single-producer/single-consumer ring for the shell adapters.
//
// Two uses (contract §5, §6): GUI→audio gesture delivery and audio→drain trace
// hand-off. Header-only, framework-free, fixed capacity, POD elements — no
// allocation or locking on the audio thread. One producer + one consumer only.
#pragma once

#include <atomic>
#include <cstddef>

namespace orrery {

template <typename T, size_t Capacity>
class SpscRing {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

    // Producer thread only.
    bool push(const T& v) {
        const size_t t = tail_.load(std::memory_order_relaxed);
        const size_t n = (t + 1) & kMask;
        if (n == head_.load(std::memory_order_acquire)) return false;  // full
        buf_[t] = v;
        tail_.store(n, std::memory_order_release);
        return true;
    }

    // Consumer thread only.
    bool pop(T& out) {
        const size_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) return false;  // empty
        out = buf_[h];
        head_.store((h + 1) & kMask, std::memory_order_release);
        return true;
    }

private:
    static constexpr size_t kMask = Capacity - 1;
    T                   buf_[Capacity];
    std::atomic<size_t> head_{0};  // consumer index
    std::atomic<size_t> tail_{0};  // producer index
};

} // namespace orrery
