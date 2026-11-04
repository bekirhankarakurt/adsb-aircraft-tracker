#include "adsb/tracker.hpp"

#include <cmath>
#include <type_traits>

namespace adsb {
namespace {

constexpr double kPairWindowSeconds = 10.0;
constexpr double kLocalWindowSeconds = 60.0;

bool validLatitude(const GeoPoint& p) {
    return p.lat >= -90.0 && p.lat <= 90.0;
}

}  // namespace

Tracker::Tracker(std::optional<GeoPoint> receiver) : receiver_(receiver) {}

void Tracker::update(const Message& message, double time) {
    auto [it, inserted] = aircraft_.try_emplace(message.icao);
    Aircraft& ac = it->second;
    if (inserted) {
        ac.icao = message.icao;
        ac.firstSeen = time;
    }
    ac.lastSeen = time;
    ++ac.messages;

    std::visit(
        [&](const auto& body) {
            using T = std::decay_t<decltype(body)>;
            if constexpr (std::is_same_v<T, Identification>) {
                ac.callsign = body.callsign;
                ac.category = body.category;
            } else if constexpr (std::is_same_v<T, AirbornePosition>) {
                if (body.altitudeFt) {
                    ac.altitudeFt = body.altitudeFt;
                }
                applyPosition(ac, body, time);
            } else if constexpr (std::is_same_v<T, Velocity>) {
                ac.speedKind = body.kind;
                if (body.speedKt) {
                    ac.speedKt = body.speedKt;
                }
                if (body.headingDeg) {
                    ac.headingDeg = body.headingDeg;
                }
                if (body.verticalRateFpm) {
                    ac.verticalRateFpm = body.verticalRateFpm;
                }
            }
        },
        message.body);
}

void Tracker::applyPosition(Aircraft& ac, const AirbornePosition& p, double time) const {
    CprSample& mine = p.odd ? ac.odd : ac.even;
    const CprSample& other = p.odd ? ac.even : ac.odd;
    mine = CprSample{{p.cprLat, p.cprLon}, time, true};

    std::optional<GeoPoint> fix;
    if (other.valid && std::fabs(time - other.time) <= kPairWindowSeconds) {
        fix = cprDecodeGlobal(p.odd ? other.coords : mine.coords, p.odd ? mine.coords : other.coords,
                              p.odd);
    }
    if (!fix) {
        std::optional<GeoPoint> reference = receiver_;
        if (ac.position && time - ac.positionTime <= kLocalWindowSeconds) {
            reference = ac.position;
        }
        if (reference) {
            fix = cprDecodeLocal(mine.coords, p.odd, *reference);
        }
    }
    if (fix && validLatitude(*fix)) {
        ac.position = fix;
        ac.positionTime = time;
    }
}

std::size_t Tracker::prune(double now, double ttlSeconds) {
    std::size_t removed = 0;
    for (auto it = aircraft_.begin(); it != aircraft_.end();) {
        if (now - it->second.lastSeen > ttlSeconds) {
            it = aircraft_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

std::vector<Aircraft> Tracker::snapshot() const {
    std::vector<Aircraft> out;
    out.reserve(aircraft_.size());
    for (const auto& entry : aircraft_) {
        out.push_back(entry.second);
    }
    return out;
}

}  // namespace adsb
