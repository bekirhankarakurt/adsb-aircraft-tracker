#pragma once

#include <cstdint>
#include <optional>

namespace adsb {

struct GeoPoint {
    double lat;
    double lon;
};

struct CprCoordinates {
    std::uint32_t lat;  // 17-bit
    std::uint32_t lon;  // 17-bit
};

inline constexpr std::uint32_t kCprBits = 17;

/// Number of longitude zones at a given latitude (1..59).
[[nodiscard]] int cprLongitudeZones(double latitudeDeg);

/// Encodes a position as an even (odd == false) or odd CPR pair.
[[nodiscard]] CprCoordinates cprEncode(GeoPoint position, bool odd);

/// Unambiguous decode from one even and one odd frame; `latestIsOdd` selects the newer one.
[[nodiscard]] std::optional<GeoPoint> cprDecodeGlobal(CprCoordinates even, CprCoordinates odd,
                                                      bool latestIsOdd);

/// Decode of a single frame relative to a reference within roughly 180 NM.
[[nodiscard]] GeoPoint cprDecodeLocal(CprCoordinates frame, bool odd, GeoPoint reference);

}  // namespace adsb
