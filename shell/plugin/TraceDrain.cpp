// TraceDrain.cpp — background JSONL trace writer.
#include "TraceDrain.h"

#include "orrery/Trace.h"

using namespace orrery;

TraceDrain::TraceDrain(SpscRing<TraceRecordPod, 256>* ring, juce::File file)
    : juce::Thread("orrery-trace"), ring_(ring), file_(std::move(file)) {}

void TraceDrain::run() {
    file_.deleteFile();
    os_ = file_.createOutputStream();
    while (!threadShouldExit()) {
        drainOnce();
        wait(20);  // ms; woken early by stopThread
    }
    drainOnce();  // final flush
    if (os_) os_->flush();
}

void TraceDrain::drainOnce() {
    if (!os_) return;
    TraceRecordPod pod;
    while (ring_->pop(pod)) {
        StringTraceWriter tw;
        trace::writeEngineGeneration(
            pod.gen,
            std::span<const TriggerEvent>(pod.ev, static_cast<size_t>(pod.count)),
            tw);
        for (const auto& line : tw.lines()) {
            os_->writeText(line + "\n", false, false, nullptr);
        }
    }
}
