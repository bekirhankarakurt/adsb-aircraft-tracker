#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "adsb/sim.hpp"
#include "adsb/version.hpp"

namespace {

constexpr const char kUsage[] =
    "Usage: adsb-sim --out FILE [options]\n"
    "  -o, --out FILE      output file ('-' = stdout)\n"
    "  -f, --format iq|hex output format (default iq)\n"
    "      --cycles N      report cycles per aircraft (default 5)\n"
    "      --corrupt N     flip one bit in every Nth frame (default 0 = off)\n"
    "      --seed N        noise seed (default 1)\n"
    "      --noise A       uniform noise amplitude, ADC counts (default 6)\n"
    "  -h, --help          show this help\n"
    "Writes a deterministic 3-aircraft scenario as 2 Msps 8-bit IQ or '*HEX;' lines.\n";

}  // namespace

int main(int argc, char** argv) {
    std::string out;
    bool hex = false;
    adsb::sim::ScenarioOptions scenario;
    adsb::sim::IqOptions iq;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "adsb-sim: %s needs a value\n", arg.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "-h" || arg == "--help") {
            std::fputs(kUsage, stdout);
            return 0;
        }
        if (arg == "-o" || arg == "--out") {
            out = value();
        } else if (arg == "-f" || arg == "--format") {
            const std::string v = value();
            if (v != "iq" && v != "hex") {
                std::fputs("adsb-sim: format must be iq or hex\n", stderr);
                return 2;
            }
            hex = v == "hex";
        } else if (arg == "--cycles") {
            scenario.cycles = static_cast<unsigned>(std::strtoul(value(), nullptr, 10));
        } else if (arg == "--corrupt") {
            scenario.corruptEvery = static_cast<unsigned>(std::strtoul(value(), nullptr, 10));
        } else if (arg == "--seed") {
            iq.seed = std::strtoull(value(), nullptr, 10);
        } else if (arg == "--noise") {
            iq.noise = std::strtod(value(), nullptr);
        } else {
            std::fprintf(stderr, "adsb-sim: unknown option '%s'\n%s", arg.c_str(), kUsage);
            return 2;
        }
    }
    if (out.empty() || scenario.cycles == 0) {
        std::fputs(kUsage, stderr);
        return 2;
    }

    const auto frames = adsb::sim::buildScenario(scenario);
    std::string payload;
    if (hex) {
        payload = adsb::sim::toHexLines(frames);
    } else {
        const auto bytes = adsb::sim::synthesizeIq(frames, iq);
        payload.assign(bytes.begin(), bytes.end());
    }
    if (out == "-") {
        std::fwrite(payload.data(), 1, payload.size(), stdout);
        return 0;
    }
    std::ofstream file(out, std::ios::binary);
    file.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!file) {
        std::fprintf(stderr, "adsb-sim: cannot write %s\n", out.c_str());
        return 1;
    }
    return 0;
}
