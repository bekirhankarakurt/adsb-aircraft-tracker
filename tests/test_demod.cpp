#include <vector>

#include "adsb/demod.hpp"
#include "adsb/sim.hpp"
#include "check.hpp"

namespace {

struct Capture {
    std::vector<adsb::RawFrame> frames;
    adsb::DemodStats run(const std::vector<std::uint8_t>& iq, std::size_t chunk, bool fix = true) {
        adsb::Demodulator demod(fix, [this](const adsb::RawFrame& f) { frames.push_back(f); });
        for (std::size_t i = 0; i < iq.size(); i += chunk) {
            demod.feed(iq.data() + i, std::min(chunk, iq.size() - i));
        }
        return demod.stats();
    }
};

std::vector<adsb::FrameBytes> scenario(unsigned corruptEvery = 0) {
    adsb::sim::ScenarioOptions o;
    o.cycles = 2;
    o.corruptEvery = corruptEvery;
    return adsb::sim::buildScenario(o);
}

}  // namespace

ADSB_TEST(demod, recovers_every_frame) {
    const auto frames = scenario();
    Capture c;
    const auto stats = c.run(adsb::sim::synthesizeIq(frames, {}), 1 << 16);
    CHECK(c.frames.size() == frames.size());
    for (std::size_t i = 0; i < frames.size() && i < c.frames.size(); ++i) {
        CHECK(c.frames[i].bytes == frames[i]);
    }
    CHECK(stats.corrected == 0);
}

ADSB_TEST(demod, chunk_boundaries_do_not_matter) {
    const auto frames = scenario();
    const auto iq = adsb::sim::synthesizeIq(frames, {});
    for (const std::size_t chunk : {1u, 7u, 333u, 4097u}) {
        Capture c;
        (void)c.run(iq, chunk);
        CHECK(c.frames.size() == frames.size());
    }
}

ADSB_TEST(demod, timestamps_follow_sample_clock) {
    Capture c;
    (void)c.run(adsb::sim::synthesizeIq(scenario(), {}), 4096);
    CHECK(c.frames.size() > 2);
    CHECK_NEAR(c.frames[0].time, 600.0 / adsb::kSampleRateHz, 2.0 / adsb::kSampleRateHz);
    CHECK_NEAR(c.frames[1].time - c.frames[0].time, 840.0 / adsb::kSampleRateHz,
               2.0 / adsb::kSampleRateHz);
}

ADSB_TEST(demod, corrupted_frames_are_repaired) {
    const auto frames = scenario(5);
    const auto clean = scenario();
    Capture c;
    const auto stats = c.run(adsb::sim::synthesizeIq(frames, {}), 1 << 16);
    CHECK(c.frames.size() == frames.size());
    CHECK(stats.corrected == frames.size() / 5);
    for (std::size_t i = 0; i < clean.size() && i < c.frames.size(); ++i) {
        CHECK(c.frames[i].bytes == clean[i]);
    }
}

ADSB_TEST(demod, corrupted_frames_are_dropped_without_correction) {
    const auto frames = scenario(5);
    Capture c;
    const auto stats = c.run(adsb::sim::synthesizeIq(frames, {}), 1 << 16, false);
    CHECK(c.frames.size() == frames.size() - frames.size() / 5);
    CHECK(stats.rejected >= frames.size() / 5);
}

ADSB_TEST(demod, tolerates_heavy_noise) {
    adsb::sim::IqOptions o;
    o.noise = 25.0;
    o.seed = 9;
    const auto frames = scenario();
    Capture c;
    (void)c.run(adsb::sim::synthesizeIq(frames, o), 1 << 16);
    CHECK(c.frames.size() >= frames.size() * 9 / 10);
}

ADSB_TEST(demod, pure_noise_yields_nothing) {
    adsb::sim::IqOptions o;
    o.noise = 40.0;
    Capture c;
    (void)c.run(adsb::sim::synthesizeIq(std::vector<adsb::FrameBytes>(200, adsb::FrameBytes{}), o) , 1 << 16);
    CHECK(c.frames.empty());
}
