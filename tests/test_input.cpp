#include <sstream>
#include <vector>

#include "adsb/input.hpp"
#include "check.hpp"

ADSB_TEST(input, parses_all_line_styles) {
    const auto raw = adsb::parseHexLine("*8D4840D6202CC371C32CE0576098;");
    const auto bare = adsb::parseHexLine("  8d4840d6202cc371c32ce0576098\r");
    const auto stamped = adsb::parseHexLine("@000000B71B008D4840D6202CC371C32CE0576098;");
    CHECK(raw && bare && stamped);
    CHECK(raw->bytes == bare->bytes && raw->bytes == stamped->bytes);
    CHECK(!raw->timestamp.has_value());
    CHECK_NEAR(*stamped->timestamp, 1.0, 1e-9);
}

ADSB_TEST(input, rejects_malformed_lines) {
    CHECK(!adsb::parseHexLine("").has_value());
    CHECK(!adsb::parseHexLine("*8D4840D6;").has_value());
    CHECK(!adsb::parseHexLine("*ZZ4840D6202CC371C32CE0576098;").has_value());
    CHECK(!adsb::parseHexLine("@123;").has_value());
}

ADSB_TEST(input, reader_counts_valid_corrected_and_rejected) {
    std::istringstream in(
        "# comment\n"
        "*8D4840D6202CC371C32CE0576098;\n"
        "*8D4840D6202CC371C32CE0576099;\n"  // one bit off
        "*8D4840D6202CC371C32CE057FFFF;\n"  // unrecoverable
        "garbage\n"
        "\n");
    std::vector<adsb::RawFrame> frames;
    const auto stats = adsb::readHex(in, true, [] { return 12.5; },
                                     [&](const adsb::RawFrame& f) { frames.push_back(f); });
    CHECK(stats.lines == 4);
    CHECK(stats.valid == 1 && stats.corrected == 1 && stats.rejected == 2);
    CHECK(frames.size() == 2);
    CHECK(frames[0].time == 12.5);
    CHECK(frames[0].bytes == frames[1].bytes);
}
