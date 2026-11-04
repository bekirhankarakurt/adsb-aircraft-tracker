#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "adsb/demod.hpp"
#include "adsb/input.hpp"
#include "adsb/tracker.hpp"
#include "check.hpp"

namespace {

const std::string kDir = ADSB_SAMPLES_DIR;

adsb::Tracker replay(const std::string& file, bool iq, adsb::DemodStats* stats = nullptr) {
    adsb::Tracker tracker;
    std::ifstream in(kDir + "/" + file, std::ios::binary);
    const auto handler = [&](const adsb::RawFrame& f) {
        if (const auto m = adsb::decodeMessage(f.bytes)) {
            tracker.update(*m, f.time);
        }
    };
    if (iq) {
        adsb::Demodulator demod(true, handler);
        adsb::readIq(in, demod);
        if (stats != nullptr) {
            *stats = demod.stats();
        }
    } else {
        (void)adsb::readHex(in, true, [] { return 0.0; }, handler);
    }
    return tracker;
}

const adsb::Aircraft* find(const std::vector<adsb::Aircraft>& all, std::uint32_t icao) {
    for (const adsb::Aircraft& a : all) {
        if (a.icao == icao) {
            return &a;
        }
    }
    return nullptr;
}

}  // namespace

ADSB_TEST(samples, sample_iq_decodes_three_aircraft) {
    adsb::DemodStats stats;
    const auto all = replay("sample.iq", true, &stats).snapshot();
    CHECK(all.size() == 3);
    CHECK(stats.corrected > 0);
    CHECK(stats.valid + stats.corrected == 60);

    const adsb::Aircraft* thy = find(all, 0x4BA8F2);
    CHECK(thy != nullptr && thy->callsign == "THY4KL" && thy->position.has_value());
    if (thy != nullptr && thy->position) {
        CHECK_NEAR(thy->position->lat, 41.2, 0.05);
        CHECK_NEAR(thy->position->lon, 28.8, 0.1);
        CHECK_NEAR(*thy->headingDeg, 75.0, 1.0);
    }
    const adsb::Aircraft* pgt = find(all, 0x4BC1D5);
    CHECK(pgt != nullptr && pgt->callsign == "PGT2312" && pgt->position.has_value());
    if (pgt != nullptr) {
        CHECK(pgt->altitudeFt == 36000);
    }
    const adsb::Aircraft* tkj = find(all, 0x4BB3A7);
    CHECK(tkj != nullptr && tkj->verticalRateFpm.has_value() && *tkj->verticalRateFpm < 0);
}

ADSB_TEST(samples, sample_hex_matches_iq_result) {
    const auto fromHex = replay("sample.hex", false).snapshot();
    const auto fromIq = replay("sample.iq", true).snapshot();
    CHECK(fromHex.size() == fromIq.size());
    for (std::size_t i = 0; i < fromHex.size() && i < fromIq.size(); ++i) {
        CHECK(fromHex[i].icao == fromIq[i].icao);
        CHECK(fromHex[i].callsign == fromIq[i].callsign);
        CHECK(fromHex[i].position.has_value() == fromIq[i].position.has_value());
    }
}

ADSB_TEST(samples, reference_frames_decode_published_position) {
    const auto all = replay("reference.hex", false).snapshot();
    const adsb::Aircraft* a = find(all, 0x40621D);
    CHECK(a != nullptr && a->position.has_value() && a->altitudeFt == 38000);
    if (a != nullptr && a->position) {
        CHECK_NEAR(a->position->lat, 52.2657, 1e-3);
        CHECK_NEAR(a->position->lon, 3.9389, 1e-3);
    }
    const adsb::Aircraft* klm = find(all, 0x4840D6);
    CHECK(klm != nullptr && klm->callsign == "KLM1023");
}
