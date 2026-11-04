#include "adsb/cpr.hpp"
#include "check.hpp"

ADSB_TEST(cpr, zones_at_known_latitudes) {
    CHECK(adsb::cprLongitudeZones(0.0) == 59);
    CHECK(adsb::cprLongitudeZones(52.2572) == 36);
    CHECK(adsb::cprLongitudeZones(-52.2572) == 36);
    CHECK(adsb::cprLongitudeZones(87.0) == 2);
    CHECK(adsb::cprLongitudeZones(89.0) == 1);
}

ADSB_TEST(cpr, global_decode_published_pair) {
    const auto fix = adsb::cprDecodeGlobal({93000, 51372}, {74158, 50194}, false);
    CHECK(fix.has_value());
    CHECK_NEAR(fix->lat, 52.25720, 1e-4);
    CHECK_NEAR(fix->lon, 3.91937, 1e-4);
}

ADSB_TEST(cpr, global_decode_uses_latest_frame) {
    const auto fix = adsb::cprDecodeGlobal({93000, 51372}, {74158, 50194}, true);
    CHECK(fix.has_value());
    CHECK_NEAR(fix->lat, 52.26578, 1e-4);
    CHECK_NEAR(fix->lon, 3.93891, 1e-4);
}

ADSB_TEST(cpr, local_decode_matches_global) {
    const auto fix = adsb::cprDecodeLocal({93000, 51372}, false, {52.0, 4.0});
    CHECK_NEAR(fix.lat, 52.25720, 1e-4);
    CHECK_NEAR(fix.lon, 3.91937, 1e-4);
}

ADSB_TEST(cpr, encode_decode_round_trip_worldwide) {
    const adsb::GeoPoint points[] = {{41.2, 28.75}, {-33.9, 151.2}, {64.1, -21.9},
                                     {0.5, -0.5},   {-77.8, 166.7}, {35.0, 179.9}};
    for (const adsb::GeoPoint& p : points) {
        const auto even = adsb::cprEncode(p, false);
        const auto odd = adsb::cprEncode(p, true);
        const auto fix = adsb::cprDecodeGlobal(even, odd, true);
        CHECK(fix.has_value());
        if (fix) {
            CHECK_NEAR(fix->lat, p.lat, 1e-3);
            CHECK_NEAR(fix->lon, p.lon, 1e-3);
        }
        const auto local = adsb::cprDecodeLocal(odd, true, {p.lat + 0.7, p.lon - 0.9});
        CHECK_NEAR(local.lat, p.lat, 1e-3);
        CHECK_NEAR(local.lon, p.lon, 1e-3);
    }
}

ADSB_TEST(cpr, pair_straddling_a_zone_boundary_is_rejected) {
    const auto even = adsb::cprEncode({10.421, 20.0}, false);
    const auto odd = adsb::cprEncode({10.471, 20.0}, true);
    CHECK(!adsb::cprDecodeGlobal(even, odd, true).has_value());
}
