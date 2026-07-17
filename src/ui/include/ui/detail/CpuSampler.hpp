#pragma once

// Differencing CPU-usage calculator shared by the process and thread view models.
//
// Windows reports kernel/user time as cumulative 100 ns counters, so a usage
// percentage requires two samples and the wall time between them:
//
//     percent = delta(kernel + user) / (elapsed * processorCount) * 100
//
// Identity guard: process and thread ids are recycled by Windows. A sample is
// only comparable to its predecessor when the entity's creation time is
// unchanged; otherwise the id now belongs to something else and the old
// baseline must be discarded rather than producing a nonsense delta.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace wis::ui::detail {

// Result of one usage computation.
struct CpuUsage {
    bool measured = false;   // false on the first sample or after an id reuse
    double percent = 0.0;
};

class CpuSampler {
public:
    explicit CpuSampler(std::uint32_t processorCount) noexcept
        : processorCount_(processorCount > 0 ? processorCount : 1) {}

    // Opens a new sampling round at `now`. Call once per refresh, before the
    // per-entity update() calls.
    void beginRound(std::chrono::steady_clock::time_point now) {
        elapsed100ns_ = 0;
        if (previousTime_) {
            const auto delta =
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - *previousTime_);
            // A non-monotonic or zero-length interval yields no measurement.
            elapsed100ns_ = (delta.count() > 0)
                                ? static_cast<std::uint64_t>(delta.count() / 100)
                                : 0;
        }
        roundTime_ = now;
        current_.clear();
    }

    // Records this round's counters for `id` and returns its usage against the
    // previous round. `createTime` identifies the entity instance.
    [[nodiscard]] CpuUsage update(std::uint32_t id, std::uint64_t cpuTime100ns,
                                  std::uint64_t createTime) {
        current_.insert_or_assign(id, Sample{cpuTime100ns, createTime});

        CpuUsage usage;
        if (elapsed100ns_ == 0) {
            return usage;  // no baseline interval yet
        }

        const auto previous = previous_.find(id);
        if (previous == previous_.end()) {
            return usage;  // entity is new this round
        }
        if (previous->second.createTime != createTime) {
            return usage;  // id was reused by a different instance
        }
        if (cpuTime100ns < previous->second.cpuTime100ns) {
            return usage;  // counter went backwards; treat as unmeasurable
        }

        const std::uint64_t cpuDelta = cpuTime100ns - previous->second.cpuTime100ns;
        const double capacity =
            static_cast<double>(elapsed100ns_) * static_cast<double>(processorCount_);
        if (capacity <= 0.0) {
            return usage;
        }

        // Scheduling jitter between samples can push the ratio slightly over
        // 100; clamp instead of surfacing an impossible figure.
        usage.measured = true;
        usage.percent = std::clamp((static_cast<double>(cpuDelta) / capacity) * 100.0, 0.0,
                                   100.0);
        return usage;
    }

    // Closes the round, promoting this round's samples to the baseline. Call
    // once per refresh, after all update() calls.
    void endRound() {
        previous_ = std::move(current_);
        current_.clear();
        previousTime_ = roundTime_;
    }

    // Drops all history, so the next round measures nothing (used when the view
    // model switches to a different subject, e.g. another process's threads).
    void reset() {
        previous_.clear();
        current_.clear();
        previousTime_.reset();
        elapsed100ns_ = 0;
    }

private:
    struct Sample {
        std::uint64_t cpuTime100ns = 0;
        std::uint64_t createTime = 0;
    };

    std::uint32_t processorCount_ = 1;
    std::unordered_map<std::uint32_t, Sample> previous_;
    std::unordered_map<std::uint32_t, Sample> current_;
    std::optional<std::chrono::steady_clock::time_point> previousTime_;
    std::chrono::steady_clock::time_point roundTime_{};
    std::uint64_t elapsed100ns_ = 0;
};

}  // namespace wis::ui::detail
