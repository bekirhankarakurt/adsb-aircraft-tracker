#include "adsb/cpr.hpp"

#include <algorithm>
#include <cmath>

namespace adsb {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kZonesPerHemisphere = 15;
constexpr double kScale = 131072.0;  // 2^17

double floorMod(double x, double y) {
    return x - y * std::floor(x / y);
}

double latitudeZoneSize(bool odd) {
    return 360.0 / (odd ? 59.0 : 60.0);
}

double normalizeLongitude(double lon) {
    return lon >= 180.0 ? lon - 360.0 : lon;
}

double normalizeLatitude(double lat) {
    return lat >= 270.0 ? lat - 360.0 : lat;
}

int longitudeZonesForFrame(double lat, bool odd) {
    return std::max(cprLongitudeZones(lat) - (odd ? 1 : 0), 1);
}

}  // namespace

int cprLongitudeZones(double latitudeDeg) {
    const double lat = std::fabs(latitudeDeg);
    if (lat < 1e-9) {
        return 59;
    }
    if (std::fabs(lat - 87.0) < 1e-9) {
        return 2;
    }
    if (lat > 87.0) {
        return 1;
    }
    const double c = std::cos(kPi / 180.0 * lat);
    const double a = 1.0 - (1.0 - std::cos(kPi / (2.0 * kZonesPerHemisphere))) / (c * c);
    return static_cast<int>(std::floor(2.0 * kPi / std::acos(a)));
}

CprCoordinates cprEncode(GeoPoint position, bool odd) {
    const double dLat = latitudeZoneSize(odd);
    const double yz = std::floor(kScale * floorMod(position.lat, dLat) / dLat + 0.5);
    const double rLat = dLat * (std::floor(position.lat / dLat) + yz / kScale);
    const double dLon = 360.0 / longitudeZonesForFrame(rLat, odd);
    const double xz = std::floor(kScale * floorMod(position.lon, dLon) / dLon + 0.5);
    constexpr std::uint32_t mask = (1u << kCprBits) - 1u;
    return {static_cast<std::uint32_t>(yz) & mask, static_cast<std::uint32_t>(xz) & mask};
}

std::optional<GeoPoint> cprDecodeGlobal(CprCoordinates even, CprCoordinates odd, bool latestIsOdd) {
    const double lat0 = even.lat / kScale;
    const double lat1 = odd.lat / kScale;
    const double lon0 = even.lon / kScale;
    const double lon1 = odd.lon / kScale;

    const double j = std::floor(59.0 * lat0 - 60.0 * lat1 + 0.5);
    const double latEven = normalizeLatitude(latitudeZoneSize(false) * (floorMod(j, 60.0) + lat0));
    const double latOdd = normalizeLatitude(latitudeZoneSize(true) * (floorMod(j, 59.0) + lat1));
    if (latEven < -90.0 || latEven > 90.0 || latOdd < -90.0 || latOdd > 90.0) {
        return std::nullopt;
    }
    const int nl = cprLongitudeZones(latEven);
    if (nl != cprLongitudeZones(latOdd)) {
        return std::nullopt;
    }

    const double lat = latestIsOdd ? latOdd : latEven;
    const int n = std::max(nl - (latestIsOdd ? 1 : 0), 1);
    const double dLon = 360.0 / n;
    const double m = std::floor(lon0 * (nl - 1) - lon1 * nl + 0.5);
    const double lon = dLon * (floorMod(m, n) + (latestIsOdd ? lon1 : lon0));
    return GeoPoint{lat, normalizeLongitude(lon)};
}

GeoPoint cprDecodeLocal(CprCoordinates frame, bool odd, GeoPoint reference) {
    const double cprLat = frame.lat / kScale;
    const double cprLon = frame.lon / kScale;

    const double dLat = latitudeZoneSize(odd);
    const double j = std::floor(reference.lat / dLat) +
                     std::floor(0.5 + floorMod(reference.lat, dLat) / dLat - cprLat);
    const double lat = dLat * (j + cprLat);

    const double dLon = 360.0 / longitudeZonesForFrame(lat, odd);
    const double m = std::floor(reference.lon / dLon) +
                     std::floor(0.5 + floorMod(reference.lon, dLon) / dLon - cprLon);
    return GeoPoint{lat, normalizeLongitude(dLon * (m + cprLon))};
}

}  // namespace adsb
