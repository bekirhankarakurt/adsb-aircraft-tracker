#include <string>

#include "adsb/output.hpp"
#include "adsb/sim.hpp"
#include "check.hpp"

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::vector<adsb::Aircraft> fixture() {
    adsb::Tracker t;
    t.update(*adsb::decodeMessage(adsb::sim::encodeIdentification(0xABC123, "TEST1", 4, 3)), 1.0);
    t.update(*adsb::decodeMessage(adsb::sim::encodePosition(0xABC123, 30000, 41.0, 29.0, false)), 1.0);
    t.update(*adsb::decodeMessage(adsb::sim::encodePosition(0xABC123, 30000, 41.0, 29.0, true)), 1.0);
    t.update(*adsb::decodeMessage(adsb::sim::encodeIdentification(0x000001, "BARE", 4, 0)), 1.0);
    return t.snapshot();
}

}  // namespace

ADSB_TEST(output, json_contains_fields_and_escapes) {
    const std::string json = adsb::toJson(fixture(), 3.0);
    CHECK(contains(json, "\"icao\":\"ABC123\""));
    CHECK(contains(json, "\"callsign\":\"TEST1\""));
    CHECK(contains(json, "\"alt_ft\":30000"));
    CHECK(contains(json, "\"lat\":41.00000"));
    CHECK(contains(json, "\"alt_ft\":null"));
    CHECK(contains(json, "\"seen_s\":2.0"));
}

ADSB_TEST(output, json_escapes_special_characters) {
    adsb::Aircraft a;
    a.callsign = "A\"B\\C";
    CHECK(contains(adsb::toJson({a}, 0.0), "\"callsign\":\"A\\\"B\\\\C\""));
}

ADSB_TEST(output, json_for_empty_table) {
    CHECK(adsb::toJson({}, 0.0) == "{\"now\":0.000,\"aircraft\":[]}\n");
}

ADSB_TEST(output, table_has_header_and_rows) {
    const std::string table = adsb::formatTable(fixture(), 3.0);
    CHECK(contains(table, "CALLSIGN"));
    CHECK(contains(table, "ABC123"));
    CHECK(contains(table, "30000"));
    CHECK(contains(table, "41.0000"));
}

ADSB_TEST(output, describe_message) {
    const auto m = *adsb::decodeMessage(adsb::sim::encodeIdentification(0x4BA8F2, "THY4KL", 4, 3));
    CHECK(adsb::describeMessage(m) == "4BA8F2 IDENT    THY4KL (A3)");
}

ADSB_TEST(output, html_embeds_snapshot) {
    const std::string json = adsb::toJson(fixture(), 3.0);
    const std::string page = adsb::renderMapHtml(json);
    CHECK(contains(page, "<canvas"));
    CHECK(contains(page, "let snapshot = {\"now\":3.000"));
    CHECK(!contains(page, "/*SNAPSHOT*/"));
}
