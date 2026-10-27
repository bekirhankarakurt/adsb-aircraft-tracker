#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "adsb/crc.hpp"

namespace adsb {

inline constexpr double kSampleRateHz = 2.0e6;

struct RawFrame {
    FrameBytes bytes;
    double time;  // seconds since start of stream
    bool corrected;
};

using FrameHandler = std::function<void(const RawFrame&)>;

struct DemodStats {
    std::uint64_t samples = 0;
    std::uint64_t preambles = 0;
    std::uint64_t valid = 0;
    std::uint64_t corrected = 0;
    std::uint64_t rejected = 0;
};

/// Streaming Mode S demodulator for 8-bit interleaved IQ captured at 2 Msps.
class Demodulator {
public:
    Demodulator(bool correctErrors, FrameHandler handler);

    /// Accepts any chunk size; frames spanning chunk boundaries are handled.
    void feed(const std::uint8_t* data, std::size_t length);

    [[nodiscard]] const DemodStats& stats() const noexcept { return stats_; }
    [[nodiscard]] double streamTime() const noexcept;

private:
    void appendSample(std::uint8_t i, std::uint8_t q);
    void scan();

    bool correctErrors_;
    FrameHandler handler_;
    std::vector<float> magnitude_;
    std::uint64_t base_ = 0;  // absolute index of magnitude_[0]
    std::uint8_t pendingByte_ = 0;
    bool hasPending_ = false;
    DemodStats stats_;
};

}  // namespace adsb
