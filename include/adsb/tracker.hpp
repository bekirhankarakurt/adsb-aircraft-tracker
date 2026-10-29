#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "adsb/cpr.hpp"
#include "adsb/message.hpp"

namespace adsb {

struct CprSample {
    CprCoordinates coords{};
    double time = 0.0;
    bool valid = false;
};

struct Aircraft {
    std::uint32_t icao = 0;
    std::string callsign;
    std::string category;
    std::optional<int> altitudeFt;
    std::optional<GeoPoint> position;
    std::optional<double> speedKt;
    SpeedKind speedKind = SpeedKind::GroundSpeed;
    std::optional<double> headingDeg;
    std::optional<int> verticalRateFpm;
    std::uint32_t messages = 0;
    double firstSeen = 0.0;
    double lastSeen = 0.0;
    double positionTime = 0.0;
    CprSample even;
    CprSample odd;
};

/// Maintains one record per ICAO address and resolves positions from CPR frames.
class Tracker {
public:
    /// `receiver` enables local CPR decoding of a lone frame (aircraft within ~180 NM).
    explicit Tracker(std::optional<GeoPoint> receiver = std::nullopt);

    void update(const Message& message, double time);

    /// Removes aircraft not heard for more than `ttlSeconds`; returns how many were dropped.
    std::size_t prune(double now, double ttlSeconds);

    /// Copies of all tracked aircraft ordered by ICAO address.
    [[nodiscard]] std::vector<Aircraft> snapshot() const;

private:
    void applyPosition(Aircraft& ac, const AirbornePosition& p, double time) const;

    std::optional<GeoPoint> receiver_;
    std::map<std::uint32_t, Aircraft> aircraft_;
};

}  // namespace adsb
