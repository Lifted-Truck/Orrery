// Trace.cpp — JSONL (de)serialization for the §6 trace substrate.
//
// The format is fully controlled here, so a compact purpose-built parser is
// used rather than pulling in a JSON dependency (framework-free core). Doubles
// are written with %.17g and read with strtod → exact IEEE round-trip.
#include "orrery/Trace.h"

#include <cstdio>
#include <cstdlib>

namespace orrery::trace {

static std::string dbl(double v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

void writeEngineGeneration(int64_t gen, std::span<const TriggerEvent> events,
                           TraceWriter& tw) {
    std::string s = "{\"kind\":\"engine\",\"gen\":" + std::to_string(gen) + ",\"ev\":[";
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        if (i) s += ',';
        s += "{\"p\":" + dbl(e.barPhase)
           + ",\"s\":" + std::to_string(e.sourceId)
           + ",\"e\":" + dbl(static_cast<double>(e.energy)) + "}";
    }
    s += "]}";
    tw.writeLine(s);
}

// Read a numeric value that follows `key` at/after `from`. Advances `from` past
// the number. Returns false if the key is not found.
static bool numAfter(const std::string& s, const char* key, size_t& from, double& out) {
    size_t k = s.find(key, from);
    if (k == std::string::npos) return false;
    k += std::char_traits<char>::length(key);
    const char* start = s.c_str() + k;
    char* end = nullptr;
    out = std::strtod(start, &end);
    if (end == start) return false;
    from = k + static_cast<size_t>(end - start);
    return true;
}

bool parseEngineLine(const std::string& line, EngineRecord& out) {
    if (line.find("\"kind\":\"engine\"") == std::string::npos) return false;
    out.events.clear();

    size_t pos = 0;
    double g = 0.0;
    if (!numAfter(line, "\"gen\":", pos, g)) return false;
    out.gen = static_cast<int64_t>(g);

    const size_t arr = line.find("\"ev\":[", pos);
    if (arr == std::string::npos) return false;
    pos = arr;

    // Each event object contributes exactly one "p", one "s", one "e" in order.
    while (true) {
        size_t probe = pos;
        double p = 0.0, s = 0.0, e = 0.0;
        if (!numAfter(line, "\"p\":", probe, p)) break;  // no more events
        if (!numAfter(line, "\"s\":", probe, s)) return false;
        if (!numAfter(line, "\"e\":", probe, e)) return false;
        TriggerEvent ev;
        ev.barPhase = p;
        ev.sourceId = static_cast<int32_t>(s);
        ev.energy   = static_cast<float>(e);
        out.events.push_back(ev);
        pos = probe;
    }
    return true;
}

} // namespace orrery::trace
