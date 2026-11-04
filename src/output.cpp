#include "adsb/output.hpp"

#include <cstdio>
#include <type_traits>

namespace adsb {
namespace {

template <typename... Args>
std::string format(const char* fmt, Args... args) {
    char buffer[256];
    const int n = std::snprintf(buffer, sizeof buffer, fmt, args...);
    return n > 0 ? std::string(buffer, static_cast<std::size_t>(n)) : std::string{};
}

const char* speedLabel(SpeedKind kind) {
    switch (kind) {
        case SpeedKind::GroundSpeed: return "gs";
        case SpeedKind::IndicatedAirspeed: return "ias";
        case SpeedKind::TrueAirspeed: return "tas";
    }
    return "gs";
}

std::string jsonString(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (static_cast<unsigned char>(c) < 0x20) {
            out += format("\\u%04x", static_cast<unsigned>(c));
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

std::string jsonOr(const std::optional<int>& v) {
    return v ? format("%d", *v) : "null";
}

std::string jsonOr(const std::optional<double>& v, const char* fmt) {
    return v ? format(fmt, *v) : "null";
}

std::string cell(const std::optional<int>& v, const char* fmt) {
    return v ? format(fmt, *v) : "-";
}

std::string cell(const std::optional<double>& v, const char* fmt) {
    return v ? format(fmt, *v) : "-";
}

constexpr const char kHtmlTemplate[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ADS-B aircraft map</title>
<style>
body{margin:0;font:13px system-ui,sans-serif;background:#0d1b2a;color:#e0e6ed;display:flex;height:100vh}
canvas{flex:1;min-width:0}
aside{width:340px;overflow:auto;border-left:1px solid #1f3349;padding:8px}
table{border-collapse:collapse;width:100%}td,th{padding:2px 6px;text-align:right}th{color:#7f9bb8}
td:first-child,th:first-child{text-align:left}
@media(max-width:700px){body{flex-direction:column}aside{width:auto;border:0;max-height:40vh}}
</style></head><body>
<canvas id="map"></canvas>
<aside><div id="stamp"></div><table id="list"></table></aside>
<script>
let snapshot = /*SNAPSHOT*/null;
const canvas = document.getElementById('map'), ctx = canvas.getContext('2d');
function draw() {
  const dpr = window.devicePixelRatio || 1;
  canvas.width = canvas.clientWidth * dpr; canvas.height = canvas.clientHeight * dpr;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  const w = canvas.clientWidth, h = canvas.clientHeight;
  ctx.clearRect(0, 0, w, h);
  const list = snapshot ? snapshot.aircraft : [];
  const fixed = list.filter(a => a.lat !== null && a.lon !== null);
  document.getElementById('stamp').textContent = list.length + ' aircraft, ' + fixed.length + ' with position';
  document.getElementById('list').innerHTML = '<tr><th>ICAO</th><th>Call</th><th>Alt</th><th>Spd</th></tr>' +
    list.map(a => '<tr><td>' + a.icao + '</td><td>' + (a.callsign || '-') + '</td><td>' +
      (a.alt_ft === null ? '-' : a.alt_ft) + '</td><td>' + (a.speed_kt === null ? '-' : Math.round(a.speed_kt)) + '</td></tr>').join('');
  if (!fixed.length) return;
  let lat0 = Math.min(...fixed.map(a => a.lat)), lat1 = Math.max(...fixed.map(a => a.lat));
  let lon0 = Math.min(...fixed.map(a => a.lon)), lon1 = Math.max(...fixed.map(a => a.lon));
  const midLat = (lat0 + lat1) / 2, k = Math.cos(midLat * Math.PI / 180);
  const spanLat = Math.max(lat1 - lat0, 0.05), spanLon = Math.max((lon1 - lon0) * k, 0.05);
  const scale = Math.min((w - 120) / spanLon, (h - 80) / spanLat);
  const cx = (lon0 + lon1) / 2, cy = midLat;
  const px = a => w / 2 + (a.lon - cx) * k * scale, py = a => h / 2 - (a.lat - cy) * scale;
  ctx.font = '12px system-ui'; ctx.strokeStyle = '#1f3349'; ctx.strokeRect(0.5, 0.5, w - 1, h - 1);
  for (const a of fixed) {
    const x = px(a), y = py(a), hd = (a.heading || 0) * Math.PI / 180;
    ctx.fillStyle = '#4cc9f0'; ctx.beginPath();
    ctx.moveTo(x + 9 * Math.sin(hd), y - 9 * Math.cos(hd));
    ctx.lineTo(x + 5 * Math.sin(hd + 2.5), y - 5 * Math.cos(hd + 2.5));
    ctx.lineTo(x + 5 * Math.sin(hd - 2.5), y - 5 * Math.cos(hd - 2.5));
    ctx.closePath(); ctx.fill();
    ctx.fillStyle = '#e0e6ed';
    ctx.fillText((a.callsign || a.icao) + (a.alt_ft === null ? '' : ' FL' + Math.round(a.alt_ft / 100)), x + 10, y + 4);
  }
}
async function poll() {
  try { const r = await fetch('aircraft.json', {cache: 'no-store'}); if (r.ok) { snapshot = await r.json(); draw(); } } catch (e) {}
}
window.addEventListener('resize', draw); draw();
if (location.protocol.startsWith('http')) setInterval(poll, 1000);
</script></body></html>
)HTML";

}  // namespace

std::string formatIcao(std::uint32_t icao) {
    return format("%06X", static_cast<unsigned>(icao));
}

std::string formatTable(const std::vector<Aircraft>& aircraft, double now) {
    std::string out = format("%-6s %-8s %7s %6s %4s %6s %9s %10s %5s %7s\n", "ICAO", "CALLSIGN",
                             "ALT(ft)", "SPD", "HDG", "VS", "LAT", "LON", "MSGS", "SEEN(s)");
    for (const Aircraft& a : aircraft) {
        const std::optional<double> lat = a.position ? std::optional<double>(a.position->lat)
                                                     : std::nullopt;
        const std::optional<double> lon = a.position ? std::optional<double>(a.position->lon)
                                                     : std::nullopt;
        out += format("%-6s %-8s %7s %6s %4s %6s %9s %10s %5u %7.1f\n", formatIcao(a.icao).c_str(),
                      a.callsign.empty() ? "-" : a.callsign.c_str(),
                      cell(a.altitudeFt, "%d").c_str(), cell(a.speedKt, "%.0f").c_str(),
                      cell(a.headingDeg, "%.0f").c_str(), cell(a.verticalRateFpm, "%+d").c_str(),
                      cell(lat, "%.4f").c_str(), cell(lon, "%.4f").c_str(),
                      static_cast<unsigned>(a.messages), now - a.lastSeen);
    }
    return out;
}

std::string describeMessage(const Message& message) {
    std::string out = formatIcao(message.icao) + " ";
    std::visit(
        [&](const auto& body) {
            using T = std::decay_t<decltype(body)>;
            if constexpr (std::is_same_v<T, Identification>) {
                out += format("IDENT    %s (%s)", body.callsign.c_str(), body.category.c_str());
            } else if constexpr (std::is_same_v<T, AirbornePosition>) {
                out += format("POSITION %s alt=%s cpr=(%u,%u)", body.odd ? "odd " : "even",
                              cell(body.altitudeFt, "%d").c_str(),
                              static_cast<unsigned>(body.cprLat),
                              static_cast<unsigned>(body.cprLon));
            } else if constexpr (std::is_same_v<T, Velocity>) {
                out += format("VELOCITY %s=%s hdg=%s vs=%s", speedLabel(body.kind),
                              cell(body.speedKt, "%.0f").c_str(),
                              cell(body.headingDeg, "%.1f").c_str(),
                              cell(body.verticalRateFpm, "%+d").c_str());
            } else {
                out += format("TC%u (not decoded)", body.typeCode);
            }
        },
        message.body);
    return out;
}

std::string toJson(const std::vector<Aircraft>& aircraft, double now) {
    std::string out = format("{\"now\":%.3f,\"aircraft\":[", now);
    bool first = true;
    for (const Aircraft& a : aircraft) {
        out += first ? "\n" : ",\n";
        first = false;
        const std::optional<double> lat = a.position ? std::optional<double>(a.position->lat)
                                                     : std::nullopt;
        const std::optional<double> lon = a.position ? std::optional<double>(a.position->lon)
                                                     : std::nullopt;
        out += "  {\"icao\":" + jsonString(formatIcao(a.icao));
        out += ",\"callsign\":" + (a.callsign.empty() ? std::string("null") : jsonString(a.callsign));
        out += ",\"category\":" + (a.category.empty() ? std::string("null") : jsonString(a.category));
        out += ",\"alt_ft\":" + jsonOr(a.altitudeFt);
        out += ",\"lat\":" + jsonOr(lat, "%.5f");
        out += ",\"lon\":" + jsonOr(lon, "%.5f");
        out += ",\"speed_kt\":" + jsonOr(a.speedKt, "%.1f");
        out += std::string(",\"speed_type\":\"") + speedLabel(a.speedKind) + "\"";
        out += ",\"heading\":" + jsonOr(a.headingDeg, "%.1f");
        out += ",\"vrate_fpm\":" + jsonOr(a.verticalRateFpm);
        out += format(",\"messages\":%u", static_cast<unsigned>(a.messages));
        out += format(",\"seen_s\":%.1f}", now - a.lastSeen);
    }
    out += first ? "]}\n" : "\n]}\n";
    return out;
}

std::string renderMapHtml(const std::string& json) {
    std::string page = kHtmlTemplate;
    const std::string marker = "/*SNAPSHOT*/null";
    page.replace(page.find(marker), marker.size(), json);
    return page;
}

}  // namespace adsb
