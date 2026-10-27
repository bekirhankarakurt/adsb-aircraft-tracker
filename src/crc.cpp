#include "adsb/crc.hpp"

#include <vector>

namespace adsb {
namespace {

constexpr std::uint32_t kPolynomial = 0xFFF409u;
constexpr std::uint32_t kMask24 = 0xFFFFFFu;
constexpr std::size_t kFrameBits = kLongFrameBytes * 8;
constexpr std::size_t kParityOffset = kLongFrameBytes - 3;

constexpr std::array<std::uint32_t, 256> makeTable() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t crc = i << 16;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x800000u) != 0 ? ((crc << 1) ^ kPolynomial) : (crc << 1);
        }
        table[i] = crc & kMask24;
    }
    return table;
}

constexpr std::array<std::uint32_t, 256> kTable = makeTable();

struct Syndrome {
    std::uint32_t value;
    std::size_t bit;
};

const std::vector<Syndrome>& syndromes() {
    static const std::vector<Syndrome> table = [] {
        std::vector<Syndrome> t;
        t.reserve(kFrameBits);
        for (std::size_t bit = 0; bit < kFrameBits; ++bit) {
            FrameBytes probe{};
            probe[bit / 8] = static_cast<std::uint8_t>(0x80u >> (bit % 8));
            t.push_back({crcRemainder(probe), bit});
        }
        return t;
    }();
    return table;
}

}  // namespace

std::uint32_t crc24(const std::uint8_t* data, std::size_t length) noexcept {
    std::uint32_t crc = 0;
    for (std::size_t i = 0; i < length; ++i) {
        crc = ((crc << 8) & kMask24) ^ kTable[((crc >> 16) ^ data[i]) & 0xFFu];
    }
    return crc;
}

std::uint32_t crcRemainder(const FrameBytes& frame) noexcept {
    return crc24(frame.data(), frame.size());
}

void sealParity(FrameBytes& frame) noexcept {
    const std::uint32_t parity = crc24(frame.data(), kParityOffset);
    frame[kParityOffset] = static_cast<std::uint8_t>(parity >> 16);
    frame[kParityOffset + 1] = static_cast<std::uint8_t>(parity >> 8);
    frame[kParityOffset + 2] = static_cast<std::uint8_t>(parity);
}

Integrity verifyAndCorrect(FrameBytes& frame, bool allowCorrection) {
    const std::uint32_t remainder = crcRemainder(frame);
    if (remainder == 0) {
        return Integrity::Valid;
    }
    if (!allowCorrection) {
        return Integrity::Invalid;
    }
    for (const Syndrome& s : syndromes()) {
        if (s.value == remainder) {
            const auto mask = static_cast<std::uint8_t>(0x80u >> (s.bit % 8));
            frame[s.bit / 8] = static_cast<std::uint8_t>(frame[s.bit / 8] ^ mask);
            return Integrity::Corrected;
        }
    }
    return Integrity::Invalid;
}

}  // namespace adsb
