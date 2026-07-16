// Lockfree.h — single-producer/single-consumer ring for the shell adapters.
//
// Two uses (contract §5, §6): GUI→audio gesture delivery and audio→drain trace
// hand-off. Header-only, framework-free, fixed capacity, POD elements — no
// allocation or locking on the audio thread. One producer + one consumer only.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

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

// Wait-free single-writer/single-reader triple buffer (contract §5: lock-free
// snapshot per engine for the GUI). The audio thread fills writeSlot() and
// publish()es; the message thread read()s the freshest published snapshot.
// Neither side ever blocks or allocates; a slow reader just skips frames.
template <typename T>
class TripleBuffer {
public:
    static_assert(std::is_trivially_copyable_v<T>, "snapshots must be POD");

    // Producer thread only.
    T&   writeSlot() { return buf_[writeIdx_]; }
    void publish() {
        writeIdx_ = middle_.exchange(writeIdx_ | kDirty, std::memory_order_acq_rel) & kIdx;
    }

    // Consumer thread only. Returns false when nothing new was published.
    bool read(T& out) {
        if ((middle_.load(std::memory_order_acquire) & kDirty) == 0) return false;
        readIdx_ = middle_.exchange(readIdx_, std::memory_order_acq_rel) & kIdx;
        out = buf_[readIdx_];
        return true;
    }

private:
    static constexpr uint32_t kIdx = 3u, kDirty = 4u;
    T                     buf_[3] = {};
    std::atomic<uint32_t> middle_{1};
    uint32_t              writeIdx_ = 0;  // producer-owned
    uint32_t              readIdx_  = 2;  // consumer-owned
};

} // namespace orrery
