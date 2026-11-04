#include "adsb/sim.hpp"
#include "adsb/tracker.hpp"
#include "check.hpp"

namespace {

adsb::Message msg(const adsb::FrameBytes& f) {
    return *adsb::decodeMessage(f);
}

}  // namespace

ADSB_TEST(tracker, merges_messages_per_aircraft) {
    adsb::Tracker t;
    t.update(msg(adsb::sim::encodeIdentification(0x111111, "ABC123", 4, 3)), 1.0);
    t.update(msg(adsb::sim::encodePosition(0x111111, 12000, 41.0, 29.0, false)), 1.1);
    t.update(msg(adsb::sim::encodePosition(0x111111, 12000, 41.0, 29.0, true)), 1.2);
    t.update(msg(adsb::sim::encodeGroundVelocity(0x111111, 200.0, 90.0, 640)), 1.3);
    t.update(msg(adsb::sim::encodeIdentification(0x222222, "XYZ9", 4, 1)), 1.4);

    const auto all = t.snapshot();
    CHECK(all.size() == 2);
    const adsb::Aircraft& a = all[0];
    CHECK(a.icao == 0x111111 && a.callsign == "ABC123" && a.messages == 4);
    CHECK(a.altitudeFt == 12000);
    CHECK(a.position.has_value());
    CHECK_NEAR(a.position->lat, 41.0, 1e-3);
    CHECK_NEAR(a.position->lon, 29.0, 1e-3);
    CHECK_NEAR(*a.speedKt, 200.0, 1.5);
    CHECK(a.verticalRateFpm == 640);
    CHECK(all[1].icao == 0x222222 && !all[1].position.has_value());
}

ADSB_TEST(tracker, stale_pair_is_not_combined_without_reference) {
    adsb::Tracker t;
    t.update(msg(adsb::sim::encodePosition(1, 5000, 41.0, 29.0, false)), 0.0);
    t.update(msg(adsb::sim::encodePosition(1, 5000, 41.0, 29.0, true)), 30.0);
    CHECK(!t.snapshot()[0].position.has_value());
}

ADSB_TEST(tracker, receiver_reference_enables_single_frame_fix) {
    adsb::Tracker t(adsb::GeoPoint{41.1, 28.9});
    t.update(msg(adsb::sim::encodePosition(1, 5000, 41.0, 29.0, true)), 0.0);
    const auto a = t.snapshot()[0];
    CHECK(a.position.has_value());
    CHECK_NEAR(a.position->lat, 41.0, 1e-3);
    CHECK_NEAR(a.position->lon, 29.0, 1e-3);
}

ADSB_TEST(tracker, known_position_anchors_later_single_frames) {
    adsb::Tracker t;
    t.update(msg(adsb::sim::encodePosition(1, 5000, 41.0, 29.0, false)), 0.0);
    t.update(msg(adsb::sim::encodePosition(1, 5000, 41.0, 29.0, true)), 0.5);
    t.update(msg(adsb::sim::encodePosition(1, 5000, 41.01, 29.02, false)), 30.0);
    const auto a = t.snapshot()[0];
    CHECK_NEAR(a.position->lat, 41.01, 1e-3);
    CHECK_NEAR(a.position->lon, 29.02, 1e-3);
}

ADSB_TEST(tracker, prune_removes_silent_aircraft) {
    adsb::Tracker t;
    t.update(msg(adsb::sim::encodeIdentification(1, "OLD", 4, 0)), 0.0);
    t.update(msg(adsb::sim::encodeIdentification(2, "NEW", 4, 0)), 90.0);
    CHECK(t.prune(100.0, 60.0) == 1);
    CHECK(t.snapshot().size() == 1 && t.snapshot()[0].icao == 2);
}
