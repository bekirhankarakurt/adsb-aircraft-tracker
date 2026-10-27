#include "adsb/message.hpp"

#include <cmath>

namespace adsb {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr char kCharset[] = "?ABCDEFGHIJKLMNOPQRSTUVWXYZ????? ???????????????0123456789??????";

using Me = const std::uint8_t*;

Identification decodeIdentification(Me me, unsigned typeCode) {
    std::uint64_t bits = 0;
    for (int i = 1; i <= 6; ++i) {
        bits = (bits << 8) | me[i];
    }
    Identification id;
    for (unsigned c = 0; c < 8; ++c) {
        id.callsign.push_back(kCharset[(bits >> (42u - 6u * c)) & 0x3Fu]);
    }
    while (!id.callsign.empty() && id.callsign.back() == ' ') {
        id.callsign.pop_back();
    }
    const char set = static_cast<char>('A' + (4 - static_cast<int>(typeCode)));
    id.category = std::string{set, static_cast<char>('0' + (me[0] & 7))};
    return id;
}

AirbornePosition decodePosition(Me me, unsigned typeCode) {
    AirbornePosition p{};
    const unsigned alt = (static_cast<unsigned>(me[1]) << 4) | (me[2] >> 4);
    if (typeCode <= 18 && (alt & 0x10u) != 0) {
        const unsigned n = ((alt & 0xFE0u) >> 1) | (alt & 0xFu);
        p.altitudeFt = static_cast<int>(n) * 25 - 1000;
    }
    p.odd = ((me[2] >> 2) & 1u) != 0;
    p.cprLat = (static_cast<std::uint32_t>(me[2] & 3u) << 15) |
               (static_cast<std::uint32_t>(me[3]) << 7) | (me[4] >> 1);
    p.cprLon = (static_cast<std::uint32_t>(me[4] & 1u) << 16) |
               (static_cast<std::uint32_t>(me[5]) << 8) | me[6];
    return p;
}

double headingFromComponents(double east, double north) {
    double deg = std::atan2(east, north) * 180.0 / kPi;
    if (deg < 0.0) {
        deg += 360.0;
    }
    return deg;
}

std::optional<Velocity> decodeVelocity(Me me) {
    const unsigned subtype = me[0] & 7u;
    if (subtype < 1 || subtype > 4) {
        return std::nullopt;
    }
    const bool westOrFlag = ((me[1] >> 2) & 1u) != 0;
    const int field1 = ((me[1] & 3) << 8) | me[2];
    const bool southOrType = (me[3] >> 7) != 0;
    const int field2 = ((me[3] & 0x7F) << 3) | (me[4] >> 5);
    const bool descending = ((me[4] >> 3) & 1u) != 0;
    const int vr = ((me[4] & 7) << 6) | (me[5] >> 2);

    Velocity v{};
    if (vr != 0) {
        const int rate = (vr - 1) * 64;
        v.verticalRateFpm = descending ? -rate : rate;
    }
    if (subtype <= 2) {
        v.kind = SpeedKind::GroundSpeed;
        if (field1 != 0 && field2 != 0) {
            const int scale = subtype == 2 ? 4 : 1;
            const double east = (westOrFlag ? -1.0 : 1.0) * (field1 - 1) * scale;
            const double north = (southOrType ? -1.0 : 1.0) * (field2 - 1) * scale;
            v.speedKt = std::hypot(east, north);
            v.headingDeg = headingFromComponents(east, north);
        }
    } else {
        v.kind = southOrType ? SpeedKind::TrueAirspeed : SpeedKind::IndicatedAirspeed;
        if (westOrFlag) {
            v.headingDeg = field1 * 360.0 / 1024.0;
        }
        if (field2 != 0) {
            v.speedKt = (field2 - 1) * (subtype == 4 ? 4.0 : 1.0);
        }
    }
    return v;
}

}  // namespace

std::optional<Message> decodeMessage(const FrameBytes& frame) {
    if ((frame[0] >> 3) != kDfExtendedSquitter) {
        return std::nullopt;
    }
    Message msg{};
    msg.capability = frame[0] & 7u;
    msg.icao = (static_cast<std::uint32_t>(frame[1]) << 16) |
               (static_cast<std::uint32_t>(frame[2]) << 8) | frame[3];

    const Me me = frame.data() + 4;
    const unsigned typeCode = me[0] >> 3;
    if (typeCode >= 1 && typeCode <= 4) {
        msg.body = decodeIdentification(me, typeCode);
    } else if ((typeCode >= 9 && typeCode <= 18) || (typeCode >= 20 && typeCode <= 22)) {
        msg.body = decodePosition(me, typeCode);
    } else if (typeCode == 19) {
        if (const auto v = decodeVelocity(me)) {
            msg.body = *v;
        } else {
            msg.body = Unsupported{typeCode};
        }
    } else {
        msg.body = Unsupported{typeCode};
    }
    return msg;
}

}  // namespace adsb
