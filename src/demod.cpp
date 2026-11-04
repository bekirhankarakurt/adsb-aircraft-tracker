#include "adsb/demod.hpp"

#include <array>
#include <cmath>
#include <utility>

namespace adsb {
namespace {

constexpr std::size_t kPreambleSamples = 16;
constexpr std::size_t kDataBits = kLongFrameBytes * 8;
constexpr std::size_t kWindow = kPreambleSamples + 2 * kDataBits;

std::array<float, 256> makeSquares() {
    std::array<float, 256> t{};
    for (std::size_t v = 0; v < t.size(); ++v) {
        const float d = static_cast<float>(v) - 127.5f;
        t[v] = d * d;
    }
    return t;
}

const std::array<float, 256> kSquares = makeSquares();

bool hasPreamble(const float* m) {
    if (!(m[0] > m[1] && m[1] < m[2] && m[2] > m[3] && m[3] < m[0] && m[4] < m[0] &&
          m[5] < m[0] && m[6] < m[0] && m[7] > m[8] && m[8] < m[9] && m[9] > m[6])) {
        return false;
    }
    const float high = (m[0] + m[2] + m[7] + m[9]) / 6.0f;
    return m[4] < high && m[5] < high && m[11] < high && m[12] < high && m[13] < high &&
           m[14] < high;
}

FrameBytes sliceBits(const float* m) {
    FrameBytes frame{};
    const float* data = m + kPreambleSamples;
    for (std::size_t bit = 0; bit < kDataBits; ++bit) {
        if (data[2 * bit] > data[2 * bit + 1]) {
            frame[bit / 8] = static_cast<std::uint8_t>(frame[bit / 8] | (0x80u >> (bit % 8)));
        }
    }
    return frame;
}

}  // namespace

Demodulator::Demodulator(bool correctErrors, FrameHandler handler)
    : correctErrors_(correctErrors), handler_(std::move(handler)) {}

double Demodulator::streamTime() const noexcept {
    return static_cast<double>(stats_.samples) / kSampleRateHz;
}

void Demodulator::appendSample(std::uint8_t i, std::uint8_t q) {
    magnitude_.push_back(std::sqrt(kSquares[i] + kSquares[q]));
    ++stats_.samples;
}

void Demodulator::feed(const std::uint8_t* data, std::size_t length) {
    std::size_t i = 0;
    if (hasPending_ && length > 0) {
        appendSample(pendingByte_, data[0]);
        hasPending_ = false;
        i = 1;
    }
    for (; i + 1 < length; i += 2) {
        appendSample(data[i], data[i + 1]);
    }
    if (i < length) {
        pendingByte_ = data[i];
        hasPending_ = true;
    }
    scan();
}

void Demodulator::scan() {
    std::size_t pos = 0;
    while (pos + kWindow <= magnitude_.size()) {
        const float* m = magnitude_.data() + pos;
        if (!hasPreamble(m)) {
            ++pos;
            continue;
        }
        ++stats_.preambles;
        FrameBytes frame = sliceBits(m);
        if ((frame[0] >> 3) != kDfExtendedSquitter) {
            ++pos;
            continue;
        }
        const Integrity integrity = verifyAndCorrect(frame, correctErrors_);
        if (integrity == Integrity::Invalid || (frame[0] >> 3) != kDfExtendedSquitter) {
            ++stats_.rejected;
            ++pos;
            continue;
        }
        const bool corrected = integrity == Integrity::Corrected;
        ++(corrected ? stats_.corrected : stats_.valid);
        const double time = static_cast<double>(base_ + pos) / kSampleRateHz;
        handler_(RawFrame{frame, time, corrected});
        pos += kWindow;
    }
    magnitude_.erase(magnitude_.begin(), magnitude_.begin() + static_cast<std::ptrdiff_t>(pos));
    base_ += pos;
}

}  // namespace adsb
