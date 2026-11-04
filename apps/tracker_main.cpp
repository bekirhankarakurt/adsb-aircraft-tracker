#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "adsb/demod.hpp"
#include "adsb/input.hpp"
#include "adsb/message.hpp"
#include "adsb/output.hpp"
#include "adsb/tracker.hpp"
#include "adsb/version.hpp"

namespace {

enum class Format { Auto, Iq, Hex };

struct Options {
    std::string input = "-";
    Format format = Format::Auto;
    std::optional<adsb::GeoPoint> reference;
    std::string jsonPath;
    std::string htmlPath;
    bool live = false;
    bool quiet = false;
    bool verbose = false;
    bool correct = true;
    double ttl = 60.0;
};

constexpr const char kUsage[] =
    "Usage: adsb-tracker [options] [FILE|-]\n"
    "  -f, --format iq|hex   input format (default: by extension; .hex/.txt = hex, else iq)\n"
    "      --ref LAT,LON     receiver position for single-frame (local) CPR decoding\n"
    "      --json FILE       write aircraft snapshot as JSON ('-' = stdout)\n"
    "      --html FILE       write a self-contained map page\n"
    "      --live            refresh the table (and files) once per second while reading\n"
    "      --ttl SECONDS     drop aircraft silent for this long in --live mode (default 60)\n"
    "      --no-fix          disable single-bit error correction\n"
    "  -v, --verbose         print every decoded message\n"
    "  -q, --quiet           do not print the aircraft table\n"
    "      --version         print version\n"
    "  -h, --help            show this help\n"
    "IQ input is unsigned 8-bit interleaved I/Q at 2 Msps (rtl_sdr -s 2000000).\n"
    "Hex input accepts '*HEX;', '@TTTTTTTTTTTTHEX;' and bare HEX lines.\n";

bool parseReference(const std::string& text, adsb::GeoPoint& out) {
    const auto comma = text.find(',');
    if (comma == std::string::npos) {
        return false;
    }
    try {
        std::size_t used = 0;
        out.lat = std::stod(text.substr(0, comma), &used);
        if (used != comma) {
            return false;
        }
        const std::string lon = text.substr(comma + 1);
        out.lon = std::stod(lon, &used);
        return used == lon.size() && out.lat >= -90.0 && out.lat <= 90.0 && out.lon >= -180.0 &&
               out.lon <= 180.0;
    } catch (const std::exception&) {
        return false;
    }
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void writeFile(const std::string& path, const std::string& content) {
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary);
        out << content;
        if (!out) {
            throw std::runtime_error("cannot write " + path);
        }
    }
    std::filesystem::rename(temp, path);
}

class App {
public:
    explicit App(Options options)
        : options_(std::move(options)), tracker_(options_.reference), start_(Clock::now()) {}

    int run() {
        std::ifstream file;
        std::istream* in = &std::cin;
        if (options_.input != "-") {
            file.open(options_.input, std::ios::binary);
            if (!file) {
                std::fprintf(stderr, "adsb-tracker: cannot open %s\n", options_.input.c_str());
                return 1;
            }
            in = &file;
        }
        const bool hex = options_.format == Format::Hex ||
                         (options_.format == Format::Auto &&
                          (endsWith(options_.input, ".hex") || endsWith(options_.input, ".txt")));

        if (hex) {
            const adsb::HexStats stats = adsb::readHex(
                *in, options_.correct, [this] { return wallSeconds(); },
                [this](const adsb::RawFrame& f) { onFrame(f); });
            now_ = frameTime_;
            emit(true);
            std::printf("%llu lines: %llu valid, %llu corrected, %llu rejected\n",
                        static_cast<unsigned long long>(stats.lines),
                        static_cast<unsigned long long>(stats.valid),
                        static_cast<unsigned long long>(stats.corrected),
                        static_cast<unsigned long long>(stats.rejected));
        } else {
            adsb::Demodulator demod(options_.correct, [this](const adsb::RawFrame& f) { onFrame(f); });
            adsb::readIq(*in, demod, [&] {
                now_ = demod.streamTime();
                emit(false);
            });
            now_ = frameTime_;
            emit(true);
            const adsb::DemodStats& s = demod.stats();
            std::printf("%llu samples (%.2f s): %llu valid, %llu corrected, %llu rejected\n",
                        static_cast<unsigned long long>(s.samples), demod.streamTime(),
                        static_cast<unsigned long long>(s.valid),
                        static_cast<unsigned long long>(s.corrected),
                        static_cast<unsigned long long>(s.rejected));
        }
        return 0;
    }

private:
    using Clock = std::chrono::steady_clock;

    double wallSeconds() const {
        return std::chrono::duration<double>(Clock::now() - start_).count();
    }

    void onFrame(const adsb::RawFrame& frame) {
        frameTime_ = frame.time;
        if (const auto message = adsb::decodeMessage(frame.bytes)) {
            if (options_.verbose) {
                std::printf("[%9.4f] %s%s\n", frame.time, adsb::describeMessage(*message).c_str(),
                            frame.corrected ? "  (bit corrected)" : "");
            }
            tracker_.update(*message, frame.time);
        }
        if (options_.live) {
            now_ = frame.time;
        }
    }

    void emit(bool final) {
        const double wall = wallSeconds();
        if (!final && (!options_.live || wall - lastEmit_ < 1.0)) {
            return;
        }
        lastEmit_ = wall;
        if (options_.live) {
            tracker_.prune(now_, options_.ttl);
        }
        const auto aircraft = tracker_.snapshot();
        const std::string json = adsb::toJson(aircraft, now_);
        if (!options_.jsonPath.empty()) {
            if (options_.jsonPath == "-") {
                std::fputs(json.c_str(), stdout);
            } else {
                writeFile(options_.jsonPath, json);
            }
        }
        if (!options_.htmlPath.empty()) {
            writeFile(options_.htmlPath, adsb::renderMapHtml(json));
        }
        if (!options_.quiet) {
            if (options_.live && !final) {
                std::fputs("\x1b[H\x1b[2J", stdout);
            }
            std::fputs(adsb::formatTable(aircraft, now_).c_str(), stdout);
            std::fflush(stdout);
        }
    }

    Options options_;
    adsb::Tracker tracker_;
    Clock::time_point start_;
    double frameTime_ = 0.0;
    double now_ = 0.0;
    double lastEmit_ = -1.0;
};

}  // namespace

int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "adsb-tracker: %s needs a value\n", arg.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "-h" || arg == "--help") {
            std::fputs(kUsage, stdout);
            return 0;
        }
        if (arg == "--version") {
            std::printf("adsb-tracker %s\n", adsb::kVersion);
            return 0;
        }
        if (arg == "-f" || arg == "--format") {
            const std::string v = value();
            if (v == "iq") {
                options.format = Format::Iq;
            } else if (v == "hex") {
                options.format = Format::Hex;
            } else {
                std::fprintf(stderr, "adsb-tracker: unknown format '%s'\n", v.c_str());
                return 2;
            }
        } else if (arg == "--ref") {
            adsb::GeoPoint ref{};
            if (!parseReference(value(), ref)) {
                std::fputs("adsb-tracker: --ref expects LAT,LON in degrees\n", stderr);
                return 2;
            }
            options.reference = ref;
        } else if (arg == "--json") {
            options.jsonPath = value();
        } else if (arg == "--html") {
            options.htmlPath = value();
        } else if (arg == "--ttl") {
            options.ttl = std::atof(value());
            if (options.ttl <= 0.0) {
                std::fputs("adsb-tracker: --ttl must be positive\n", stderr);
                return 2;
            }
        } else if (arg == "--live") {
            options.live = true;
        } else if (arg == "--no-fix") {
            options.correct = false;
        } else if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            options.quiet = true;
        } else if (arg == "-" || arg.rfind("-", 0) != 0) {
            options.input = arg;
        } else {
            std::fprintf(stderr, "adsb-tracker: unknown option '%s'\n%s", arg.c_str(), kUsage);
            return 2;
        }
    }
    try {
        return App(std::move(options)).run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "adsb-tracker: %s\n", e.what());
        return 1;
    }
}
