// IEngineView.h — the GUI seam (the visual analog of the IEngine contract).
//
// Each engine territory owns ONE view (engines/<name>/gui/) that plugs into the
// shell chrome here, exactly as its physics plugs into IEngine. A view:
//   - depends ONLY on this header, Theme.h, Snapshot.h, and its OWN engine's
//     headers — never on the shell's internals or a sibling engine's view;
//   - reads state exclusively through GuiSnapshot (no processor access);
//   - sends interaction upward via the callbacks below (the shell routes them
//     into the SPSC gesture queue) — it never mutates model state.
// The shell hosts exactly one active view in the stage area and calls
// updateSnapshot() at timer rate. Add an engine → add a view; the chrome,
// tokens, and plumbing never change. That is the anti-mess contract.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "gui/Snapshot.h"
#include "orrery/Contract.h"

class IEngineView : public juce::Component {
public:
    ~IEngineView() override = default;

    // Fresh snapshot from the audio thread (message thread, ~30 Hz). The view
    // copies what it needs and repaints.
    virtual void updateSnapshot(const orrery::GuiSnapshot&) = 0;

    // Mono tab label, e.g. "ELASTIC".
    virtual juce::String engineName() const = 0;

    // Interaction path: view → shell → SPSC queue → engine (contract §5).
    std::function<void(const orrery::GestureEvent&)> onGesture;
};
