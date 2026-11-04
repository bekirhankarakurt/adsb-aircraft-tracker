#pragma once

#include <cstdint>
#include <functional>
#include <istream>
#include <optional>
#include <string_view>

#include "adsb/demod.hpp"

namespace adsb {

struct HexLine {
    FrameBytes bytes{};
    std::optional<double> timestamp;  // seconds, from an '@' 12 MHz counter prefix
};

struct HexStats {
    std::uint64_t lines = 0;
    std::uint64_t valid = 0;
    std::uint64_t corrected = 0;
    std::uint64_t rejected = 0;
};

/// Parses "*HEX;", "@TTTTTTTTTTTTHEX;" or bare HEX for a 112-bit frame.
[[nodiscard]] std::optional<HexLine> parseHexLine(std::string_view line);

/// Reads 8-bit IQ in 64 KiB chunks; `onChunk` runs after each chunk (may be empty).
void readIq(std::istream& in, Demodulator& demod, const std::function<void()>& onChunk = {});

/// Reads hex lines. Lines without a timestamp are stamped with `clock()`.
HexStats readHex(std::istream& in, bool correctErrors, const std::function<double()>& clock,
                 const FrameHandler& handler);

}  // namespace adsb
