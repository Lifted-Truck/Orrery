// PluginEditor.h — the shell chrome hosting one IEngineView.
//
// Assembly only: header / tabs / stage(view) / rail / lane / routing, laid out
// per the approved mockup. All live data arrives through GuiSnapshot at timer
// rate; all interaction leaves through the processor's SPSC queues. Swapping
// or adding an engine view touches the view registry here and nothing else.
#pragma once

#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>

#include "gui/Chrome.h"
#include "gui/IEngineView.h"
#include "gui/OffsetLane.h"
#include "gui/OrreryLookAndFeel.h"
#include "gui/ParamRail.h"

class OrreryProcessor;

class OrreryEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OrreryEditor(OrreryProcessor&);
    ~OrreryEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    OrreryProcessor& proc_;
    OrreryLookAndFeel lnf_;

    HeaderBar  header_;
    EngineTabs tabs_;
    std::unique_ptr<IEngineView> view_;   // the active engine's view
    std::unique_ptr<ParamRail>   rail_;
    std::unique_ptr<OffsetLane>  lane_;
    std::unique_ptr<RoutingBar>  routing_;
    juce::TooltipWindow tooltips_ { this };
};
