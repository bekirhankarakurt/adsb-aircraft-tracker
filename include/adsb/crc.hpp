#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace adsb {

inline constexpr std::size_t kLongFrameBytes = 14;
inline constexpr unsigned kDfExtendedSquitter = 17;

using FrameBytes = std::array<std::uint8_t, kLongFrameBytes>;

enum class Integrity { Valid, Corrected, Invalid };

/// CRC-24 (generator 0xFFF409) over `length` bytes, MSB first, zero initial value.
[[nodiscard]] std::uint32_t crc24(const std::uint8_t* data, std::size_t length) noexcept;

/// Remainder over the whole 112-bit frame; zero for an intact frame.
[[nodiscard]] std::uint32_t crcRemainder(const FrameBytes& frame) noexcept;

/// Overwrites the 24 parity bits so that the frame has a zero remainder.
void sealParity(FrameBytes& frame) noexcept;

/// Checks the CRC and, when `allowCorrection` is set, repairs a single flipped bit in place.
[[nodiscard]] Integrity verifyAndCorrect(FrameBytes& frame, bool allowCorrection);

}  // namespace adsb
