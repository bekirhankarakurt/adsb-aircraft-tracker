#include "adsb/crc.hpp"
#include "adsb/input.hpp"
#include "check.hpp"

namespace {
adsb::FrameBytes klm() {
    return adsb::parseHexLine("8D4840D6202CC371C32CE0576098")->bytes;
}
}  // namespace

ADSB_TEST(crc, valid_frame_has_zero_remainder) {
    CHECK(adsb::crcRemainder(klm()) == 0);
}

ADSB_TEST(crc, seal_parity_reproduces_published_parity) {
    adsb::FrameBytes f = klm();
    f[11] = f[12] = f[13] = 0;
    adsb::sealParity(f);
    CHECK(f == klm());
}

ADSB_TEST(crc, every_single_bit_error_is_corrected) {
    const adsb::FrameBytes good = klm();
    for (std::size_t bit = 0; bit < 112; ++bit) {
        adsb::FrameBytes f = good;
        f[bit / 8] = static_cast<std::uint8_t>(f[bit / 8] ^ (0x80u >> (bit % 8)));
        CHECK(adsb::verifyAndCorrect(f, true) == adsb::Integrity::Corrected);
        CHECK(f == good);
    }
}

ADSB_TEST(crc, correction_can_be_disabled) {
    adsb::FrameBytes f = klm();
    f[5] = static_cast<std::uint8_t>(f[5] ^ 0x01u);
    CHECK(adsb::verifyAndCorrect(f, false) == adsb::Integrity::Invalid);
}

ADSB_TEST(crc, double_bit_error_is_rejected) {
    adsb::FrameBytes f = klm();
    f[2] = static_cast<std::uint8_t>(f[2] ^ 0x81u);
    CHECK(adsb::verifyAndCorrect(f, true) == adsb::Integrity::Invalid);
}

ADSB_TEST(crc, intact_frame_reports_valid) {
    adsb::FrameBytes f = klm();
    CHECK(adsb::verifyAndCorrect(f, true) == adsb::Integrity::Valid);
}
