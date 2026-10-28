#include "adsb/sim.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "adsb/cpr.hpp"

namespace adsb::sim {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr char kCharset[] = "?ABCDEFGHIJKLMNOPQRSTUVWXYZ????? ???????????????0123456789??????";

/// Places `value` into a 56-bit ME field; bit numbering follows the standard (1 = MSB).
void putBits(std::uint64_t& me, unsigned firstBit, unsigned width, std::uint64_t value) {
    me |= (value & ((std::uint64_t{1} << width) - 1)) << (56u - (firstBit - 1u) - width);
}

FrameBytes assemble(std::uint32_t icao, std::uint64_t me) {
    FrameBytes f{};
    f[0] = static_cast<std::uint8_t>(kDfExtendedSquitter << 3 | 5u);
    f[1] = static_cast<std::uint8_t>(icao >> 16);
    f[2] = static_cast<std::uint8_t>(icao >> 8);
    f[3] = static_cast<std::uint8_t>(icao);
    for (std::size_t i = 0; i < 7; ++i) {
        f[4 + i] = static_cast<std::uint8_t>(me >> (48u - 8u * i));
    }
    sealParity(f);
    return f;
}

unsigned charIndex(char c) {
    const char* p = std::find(std::begin(kCharset), std::end(kCharset) - 1, c);
    return p == std::end(kCharset) - 1 ? 0u : static_cast<unsigned>(p - kCharset);
}

class Xorshift {
public:
    explicit Xorshift(std::uint64_t seed) : state_(seed * 0x9E3779B97F4A7C15ull + 1ull) {}
    double uniform() {  // [0, 1)
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return static_cast<double>((state_ * 0x2545F4914F6CDD1Dull) >> 11) / 9007199254740992.0;
    }

private:
    std::uint64_t state_;
};

std::uint8_t toAdc(double v) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 255.0)));
}

struct Plan {
    std::uint32_t icao;
    const char* callsign;
    unsigned category;
    double lat;
    double lon;
    int altitudeFt;
    double speedKt;
    double headingDeg;
    int verticalRateFpm;
};

constexpr Plan kPlans[] = {
    {0x4BA8F2, "THY4KL", 3, 41.2000, 28.7500, 8000, 250.0, 75.0, 1500},
    {0x4BC1D5, "PGT2312", 3, 40.9000, 29.4000, 36000, 440.0, 270.0, 0},
    {0x4BB3A7, "TKJ77", 2, 41.4500, 28.2000, 22000, 320.0, 150.0, -1800},
};

}  // namespace

FrameBytes encodeIdentification(std::uint32_t icao, const std::string& callsign, unsigned typeCode,
                                unsigned category) {
    std::uint64_t me = 0;
    putBits(me, 1, 5, typeCode);
    putBits(me, 6, 3, category);
    for (unsigned i = 0; i < 8; ++i) {
        const char c = i < callsign.size() ? callsign[i] : ' ';
        putBits(me, 9 + 6 * i, 6, charIndex(c));
    }
    return assemble(icao, me);
}

FrameBytes encodePosition(std::uint32_t icao, int altitudeFt, double lat, double lon, bool odd) {
    const auto n = static_cast<std::uint64_t>((altitudeFt + 1000) / 25);
    const std::uint64_t alt = ((n & 0x7F0u) << 1) | 0x10u | (n & 0xFu);
    const CprCoordinates cpr = cprEncode({lat, lon}, odd);
    std::uint64_t me = 0;
    putBits(me, 1, 5, 11);
    putBits(me, 9, 12, alt);
    putBits(me, 22, 1, odd ? 1u : 0u);
    putBits(me, 23, 17, cpr.lat);
    putBits(me, 40, 17, cpr.lon);
    return assemble(icao, me);
}

FrameBytes encodeGroundVelocity(std::uint32_t icao, double speedKt, double headingDeg,
                                int verticalRateFpm) {
    const double rad = headingDeg * kPi / 180.0;
    const long east = std::lround(speedKt * std::sin(rad));
    const long north = std::lround(speedKt * std::cos(rad));
    std::uint64_t me = 0;
    putBits(me, 1, 5, 19);
    putBits(me, 6, 3, 1);
    putBits(me, 14, 1, east < 0 ? 1u : 0u);
    putBits(me, 15, 10, static_cast<std::uint64_t>(std::labs(east)) + 1u);
    putBits(me, 25, 1, north < 0 ? 1u : 0u);
    putBits(me, 26, 10, static_cast<std::uint64_t>(std::labs(north)) + 1u);
    putBits(me, 37, 1, verticalRateFpm < 0 ? 1u : 0u);
    putBits(me, 38, 9, static_cast<std::uint64_t>(std::abs(verticalRateFpm) / 64) + 1u);
    return assemble(icao, me);
}

std::vector<FrameBytes> buildScenario(const ScenarioOptions& options) {
    std::vector<FrameBytes> frames;
    for (unsigned cycle = 0; cycle < options.cycles; ++cycle) {
        const double t = cycle * options.cycleSeconds;
        for (const Plan& p : kPlans) {
            const double rad = p.headingDeg * kPi / 180.0;
            const double meters = p.speedKt * 0.514444 * t;
            const double lat = p.lat + meters * std::cos(rad) / 111320.0;
            const double lon =
                p.lon + meters * std::sin(rad) / (111320.0 * std::cos(p.lat * kPi / 180.0));
            const int alt = p.altitudeFt + static_cast<int>(p.verticalRateFpm * t / 60.0 / 25.0) * 25;
            frames.push_back(encodeIdentification(p.icao, p.callsign, 4, p.category));
            frames.push_back(encodePosition(p.icao, alt, lat, lon, false));
            frames.push_back(encodePosition(p.icao, alt, lat, lon, true));
            frames.push_back(
                encodeGroundVelocity(p.icao, p.speedKt, p.headingDeg, p.verticalRateFpm));
        }
    }
    if (options.corruptEvery > 0) {
        for (std::size_t i = options.corruptEvery - 1; i < frames.size(); i += options.corruptEvery) {
            const std::size_t bit = 8 + (i * 37) % 96;  // never touch the DF field
            frames[i][bit / 8] = static_cast<std::uint8_t>(frames[i][bit / 8] ^ (0x80u >> (bit % 8)));
        }
    }
    return frames;
}

std::vector<std::uint8_t> synthesizeIq(const std::vector<FrameBytes>& frames,
                                       const IqOptions& options) {
    Xorshift rng(options.seed);
    std::vector<std::uint8_t> out;
    out.reserve((frames.size() + 1) * (options.gapSamples + 240) * 2);

    const auto emit = [&](double pulseI, double pulseQ) {
        out.push_back(toAdc(127.5 + pulseI + (rng.uniform() * 2.0 - 1.0) * options.noise));
        out.push_back(toAdc(127.5 + pulseQ + (rng.uniform() * 2.0 - 1.0) * options.noise));
    };
    const auto silence = [&](std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            emit(0.0, 0.0);
        }
    };

    silence(options.gapSamples);
    for (const FrameBytes& frame : frames) {
        const double phase = rng.uniform() * 2.0 * kPi;
        const double pi = options.amplitude * std::cos(phase);
        const double pq = options.amplitude * std::sin(phase);
        std::array<bool, 240> chips{};
        for (const std::size_t s : {0u, 2u, 7u, 9u}) {
            chips[s] = true;
        }
        for (std::size_t bit = 0; bit < 112; ++bit) {
            const bool one = (frame[bit / 8] & (0x80u >> (bit % 8))) != 0;
            chips[16 + 2 * bit + (one ? 0 : 1)] = true;
        }
        for (const bool on : chips) {
            emit(on ? pi : 0.0, on ? pq : 0.0);
        }
        silence(options.gapSamples);
    }
    return out;
}

std::string toHexLines(const std::vector<FrameBytes>& frames) {
    std::string out;
    for (const FrameBytes& f : frames) {
        out.push_back('*');
        for (const std::uint8_t b : f) {
            char buf[3];
            std::snprintf(buf, sizeof buf, "%02X", static_cast<unsigned>(b));
            out += buf;
        }
        out += ";\n";
    }
    return out;
}

}  // namespace adsb::sim
