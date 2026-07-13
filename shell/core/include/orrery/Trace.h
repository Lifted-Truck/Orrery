// Trace.h — determinism & trace substrate (sequencer-studio-architecture.md §6).
//
// "Reduce, never invent": every emitted event carries a trace that reproduces
// it. A trace + project state fully reconstructs a performance (wend-compatible).
// The core ships a StringTraceWriter (in-memory JSONL) plus a matching parser so
// a round-trip is unit-testable; the shell's real writer drains a ring buffer
// off-thread onto disk with the identical line format.
#pragma once

#include <span>
#include <string>
#include <vector>

#include "orrery/Contract.h"
#include "orrery/Types.h"

namespace orrery {

// In-memory TraceWriter — collects JSONL lines for inspection / round-trip.
class StringTraceWriter : public TraceWriter {
public:
    void writeLine(const std::string& jsonLine) override { lines_.push_back(jsonLine); }
    const std::vector<std::string>& lines() const { return lines_; }
    void clear() { lines_.clear(); }
private:
    std::vector<std::string> lines_;
};

namespace trace {

// One engine-generation record: the latched event set for generation `gen`.
struct EngineRecord {
    int64_t                   gen = 0;
    std::vector<TriggerEvent> events;
};

// Serialize a generation's latched events to one JSONL line (full-precision
// numerics so the parse round-trips losslessly).
void writeEngineGeneration(int64_t gen, std::span<const TriggerEvent> events,
                           TraceWriter& tw);

// Parse a line produced by writeEngineGeneration back into an EngineRecord.
// Returns false if the line is not a well-formed engine record.
bool parseEngineLine(const std::string& line, EngineRecord& out);

} // namespace trace
} // namespace orrery
