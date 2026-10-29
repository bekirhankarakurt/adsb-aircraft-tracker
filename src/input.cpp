#include "adsb/input.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace adsb {
namespace {

constexpr std::size_t kChunkBytes = std::size_t{64} * 1024;
constexpr double kMlatClockHz = 12.0e6;
constexpr std::size_t kTimestampDigits = 12;
constexpr std::size_t kFrameDigits = kLongFrameBytes * 2u;

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

template <typename Out>
bool parseHex(std::string_view digits, Out first, std::size_t count) {
    if (digits.size() != count * 2) {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const int hi = hexValue(digits[2 * i]);
        const int lo = hexValue(digits[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        first[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

}  // namespace

std::optional<HexLine> parseHexLine(std::string_view line) {
    std::string_view s = trim(line);
    HexLine out{};
    if (!s.empty() && s.back() == ';') {
        s.remove_suffix(1);
    }
    if (!s.empty() && s.front() == '*') {
        s.remove_prefix(1);
    } else if (!s.empty() && s.front() == '@') {
        s.remove_prefix(1);
        if (s.size() != kTimestampDigits + kFrameDigits) {
            return std::nullopt;
        }
        std::array<std::uint8_t, 6> raw{};
        if (!parseHex(s.substr(0, kTimestampDigits), raw.data(), raw.size())) {
            return std::nullopt;
        }
        std::uint64_t ticks = 0;
        for (const std::uint8_t b : raw) {
            ticks = (ticks << 8) | b;
        }
        out.timestamp = static_cast<double>(ticks) / kMlatClockHz;
        s.remove_prefix(kTimestampDigits);
    }
    if (!parseHex(s, out.bytes.data(), out.bytes.size())) {
        return std::nullopt;
    }
    return out;
}

void readIq(std::istream& in, Demodulator& demod, const std::function<void()>& onChunk) {
    std::vector<std::uint8_t> buffer(kChunkBytes);
    while (in) {
        in.read(reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
        const auto got = static_cast<std::size_t>(in.gcount());
        if (got == 0) {
            break;
        }
        demod.feed(buffer.data(), got);
        if (onChunk) {
            onChunk();
        }
    }
}

HexStats readHex(std::istream& in, bool correctErrors, const std::function<double()>& clock,
                 const FrameHandler& handler) {
    HexStats stats;
    std::string line;
    while (std::getline(in, line)) {
        const std::string_view text = trim(line);
        if (text.empty() || text.front() == '#') {
            continue;
        }
        ++stats.lines;
        auto parsed = parseHexLine(text);
        if (!parsed) {
            ++stats.rejected;
            continue;
        }
        const Integrity integrity = verifyAndCorrect(parsed->bytes, correctErrors);
        if (integrity == Integrity::Invalid) {
            ++stats.rejected;
            continue;
        }
        const bool corrected = integrity == Integrity::Corrected;
        ++(corrected ? stats.corrected : stats.valid);
        handler(RawFrame{parsed->bytes, parsed->timestamp.value_or(clock()), corrected});
    }
    return stats;
}

}  // namespace adsb
