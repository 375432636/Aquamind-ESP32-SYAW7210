#pragma once

#include <atomic>
#include <cstdint>

// Odd generations represent a held button; even generations are released.
// A queued start/stop belongs to one gesture, even across a slow connection.
class SyawPttRequest {
public:
    uint32_t Begin() {
        auto previous = generation_.load();
        while ((previous & 1U) == 0) {
            if (generation_.compare_exchange_weak(previous, previous + 1)) {
                return previous + 1;
            }
        }
        return 0;
    }

    uint32_t Cancel() {
        auto previous = generation_.load();
        while ((previous & 1U) != 0) {
            if (generation_.compare_exchange_weak(previous, previous + 1)) {
                return previous;
            }
        }
        return 0;
    }

    bool IsCurrent(uint32_t token) const {
        return (token & 1U) != 0 && generation_.load() == token;
    }

    void CancelIfCurrent(uint32_t token) {
        generation_.compare_exchange_strong(token, token + 1);
    }

private:
    std::atomic<uint32_t> generation_{0};
};
