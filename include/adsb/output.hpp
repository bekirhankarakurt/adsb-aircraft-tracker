#pragma once

#include <string>
#include <vector>

#include "adsb/message.hpp"
#include "adsb/tracker.hpp"

namespace adsb {

[[nodiscard]] std::string formatIcao(std::uint32_t icao);

/// Fixed-width aircraft table; `now` is used to compute the "seen" column.
[[nodiscard]] std::string formatTable(const std::vector<Aircraft>& aircraft, double now);

/// One-line human description of a decoded message.
[[nodiscard]] std::string describeMessage(const Message& message);

[[nodiscard]] std::string toJson(const std::vector<Aircraft>& aircraft, double now);

/// Self-contained HTML page (canvas plot, no external assets) embedding the JSON snapshot.
[[nodiscard]] std::string renderMapHtml(const std::string& json);

}  // namespace adsb
