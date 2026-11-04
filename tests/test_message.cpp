#include <variant>

#include "adsb/input.hpp"
#include "adsb/message.hpp"
#include "adsb/sim.hpp"
#include "check.hpp"

namespace {
adsb::Message decode(const char* hex) {
    return *adsb::decodeMessage(adsb::parseHexLine(hex)->bytes);
}
}  // namespace

ADSB_TEST(message, identification) {
    const adsb::Message m = decode("8D4840D6202CC371C32CE0576098");
    CHECK(m.icao == 0x4840D6);
    const auto* id = std::get_if<adsb::Identification>(&m.body);
    CHECK(id != nullptr);
    CHECK(id->callsign == "KLM1023");
    CHECK(id->category == "A0");
}

ADSB_TEST(message, airborne_position) {
    const adsb::Message m = decode("8D40621D58C382D690C8AC2863A7");
    const auto* p = std::get_if<adsb::AirbornePosition>(&m.body);
    CHECK(p != nullptr);
    CHECK(p->altitudeFt == 38000);
    CHECK(!p->odd);
    CHECK(p->cprLat == 93000);
    CHECK(p->cprLon == 51372);
}

ADSB_TEST(message, ground_velocity) {
    const adsb::Message m = decode("8D485020994409940838175B284F");
    const auto* v = std::get_if<adsb::Velocity>(&m.body);
    CHECK(v != nullptr);
    CHECK(v->kind == adsb::SpeedKind::GroundSpeed);
    CHECK_NEAR(*v->speedKt, 159.2, 0.1);
    CHECK_NEAR(*v->headingDeg, 182.88, 0.01);
    CHECK(v->verticalRateFpm == -832);
}

ADSB_TEST(message, airspeed_velocity) {
    const adsb::Message m = decode("8DA05F219B06B6AF189400CBC33F");
    const auto* v = std::get_if<adsb::Velocity>(&m.body);
    CHECK(v != nullptr);
    CHECK(v->kind == adsb::SpeedKind::TrueAirspeed);
    CHECK(v->speedKt == 375.0);
    CHECK_NEAR(*v->headingDeg, 243.98, 0.01);
    CHECK(v->verticalRateFpm == -2304);
}

ADSB_TEST(message, other_downlink_formats_are_ignored) {
    adsb::FrameBytes f = adsb::parseHexLine("8D4840D6202CC371C32CE0576098")->bytes;
    f[0] = 0x5D;
    CHECK(!adsb::decodeMessage(f).has_value());
}

ADSB_TEST(message, unsupported_type_code) {
    adsb::FrameBytes f = adsb::parseHexLine("8D4840D6202CC371C32CE0576098")->bytes;
    f[4] = 0xF8;  // TC 31
    const auto m = adsb::decodeMessage(f);
    CHECK(m.has_value() && std::holds_alternative<adsb::Unsupported>(m->body));
}

ADSB_TEST(message, encoder_round_trip) {
    const auto f = adsb::sim::encodeIdentification(0xABCDEF, "TEST12", 4, 3);
    CHECK(adsb::crcRemainder(f) == 0);
    const auto m = adsb::decodeMessage(f);
    const auto* id = std::get_if<adsb::Identification>(&m->body);
    CHECK(id != nullptr && id->callsign == "TEST12" && id->category == "A3");

    const auto g = adsb::sim::encodeGroundVelocity(1, 300.0, 225.0, -1280);
    const auto decoded = adsb::decodeMessage(g);
    const auto* v = std::get_if<adsb::Velocity>(&decoded->body);
    CHECK(v != nullptr);
    CHECK_NEAR(*v->speedKt, 300.0, 1.5);
    CHECK_NEAR(*v->headingDeg, 225.0, 0.5);
    CHECK(v->verticalRateFpm == -1280);
}
