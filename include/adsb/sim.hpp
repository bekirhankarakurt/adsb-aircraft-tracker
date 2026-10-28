#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "adsb/crc.hpp"

namespace adsb::sim {

[[nodiscard]] FrameBytes encodeIdentification(std::uint32_t icao, const std::string& callsign,
                                              unsigned typeCode, unsigned category);
[[nodiscard]] FrameBytes encodePosition(std::uint32_t icao, int altitudeFt, double lat, double lon,
                                        bool odd);
[[nodiscard]] FrameBytes encodeGroundVelocity(std::uint32_t icao, double speedKt,
                                              double headingDeg, int verticalRateFpm);

struct ScenarioOptions {
    unsigned cycles = 5;
    double cycleSeconds = 4.0;
    unsigned corruptEvery = 0;  // flip one bit in every Nth frame (0 = none)
};

/// Three aircraft near Istanbul; each cycle emits identification, even/odd position, velocity.
[[nodiscard]] std::vector<FrameBytes> buildScenario(const ScenarioOptions& options);

struct IqOptions {
    std::uint64_t seed = 1;
    double noise = 6.0;       // uniform noise amplitude in ADC counts
    double amplitude = 90.0;  // pulse amplitude in ADC counts
    std::size_t gapSamples = 600;
};

/// 2 Msps unsigned 8-bit IQ with a silent gap before, between and after the frames.
[[nodiscard]] std::vector<std::uint8_t> synthesizeIq(const std::vector<FrameBytes>& frames,
                                                     const IqOptions& options);

[[nodiscard]] std::string toHexLines(const std::vector<FrameBytes>& frames);

}  // namespace adsb::sim
