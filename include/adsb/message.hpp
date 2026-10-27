#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "adsb/crc.hpp"

namespace adsb {

enum class SpeedKind { GroundSpeed, IndicatedAirspeed, TrueAirspeed };

struct Identification {
    std::string callsign;
    std::string category;  // e.g. "A3"
};

struct AirbornePosition {
    std::optional<int> altitudeFt;  // barometric, 25 ft resolution (Q-bit set)
    bool odd;
    std::uint32_t cprLat;
    std::uint32_t cprLon;
};

struct Velocity {
    SpeedKind kind;
    std::optional<double> speedKt;
    std::optional<double> headingDeg;
    std::optional<int> verticalRateFpm;
};

struct Unsupported {
    unsigned typeCode;
};

struct Message {
    std::uint32_t icao;
    unsigned capability;
    std::variant<Identification, AirbornePosition, Velocity, Unsupported> body;
};

/// Decodes a DF17 extended squitter. Does not check the CRC; returns nullopt for other formats.
[[nodiscard]] std::optional<Message> decodeMessage(const FrameBytes& frame);

}  // namespace adsb
