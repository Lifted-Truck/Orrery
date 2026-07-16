// PluginEditor.cpp — chrome assembly + snapshot distribution.
#include "gui/PluginEditor.h"

#include "PluginProcessor.h"
#include "elastic-euclid/gui/ElasticView.h"
#include "gui/Theme.h"

using namespace orrery;

OrreryEditor::OrreryEditor(OrreryProcessor& p)
    : juce::AudioProcessorEditor(p), proc_(p) {
    setLookAndFeel(&lnf_);

    addAndMakeVisible(header_);
    header_.onTick = [this] { proc_.requestManualTick(); };

    tabs_.setTabs({ { "ELASTIC",  "equilibrium", true  },
                    { "MEASURED", "measure",     false },     // organs built; slot
                    { "PROBABLE", "field",       false } });  // hosting is next phase
    addAndMakeVisible(tabs_);

    view_ = std::make_unique<ElasticView>();
    view_->onGesture = [this](const GestureEvent& g) { proc_.pushGesture(g); };
    addAndMakeVisible(*view_);

    rail_ = std::make_unique<ParamRail>(proc_.apvts(), "ELASTIC EUCLID",
        std::vector<ParamRail::Row>{
            { "sources",     "particles k", "",   -1 },
            { "wells",       "lattice n",   "",   -1 },
            { "",            "", "", 0 },
            { "repulsion",   "repulsion",   "",    2 },
            { "lattice",     "lattice pull","",    2 },
            { "damping",     "damping",     "",    2 },
            { "relax",       "relax",       "s",   2 },
            { "",            "", "", 0 },
            { "quantizeOut", "quantize",    "",    2 },
            { "gateMs",      "gate",        "ms", -1 },
            { "gain",        "gain",        "",    2 },
        });
    addAndMakeVisible(*rail_);

    lane_ = std::make_unique<OffsetLane>(proc_.apvts());
    lane_->onEdit = [this](const OffsetEdit& e) { proc_.pushOffsetEdit(e); };
    addAndMakeVisible(*lane_);

    routing_ = std::make_unique<RoutingBar>(proc_.apvts(),
        [this] { return proc_.virtualMidiOpen(); },
        "0x" + juce::String::toHexString((juce::int64)proc_.projectSeed()).toUpperCase());
    addAndMakeVisible(*routing_);

    setResizable(true, true);
    setResizeLimits(880, 560, 1600, 1100);
    setSize(980, 640);
    startTimerHz(30);
}

OrreryEditor::~OrreryEditor() { setLookAndFeel(nullptr); }

void OrreryEditor::timerCallback() {
    GuiSnapshot s;
    if (proc_.readSnapshot(s)) {
        header_.update(s);
        view_->updateSnapshot(s);
        lane_->update(s);
    }
    routing_->repaint();   // port status is a cheap poll
}

void OrreryEditor::paint(juce::Graphics& g) { g.fillAll(theme::bg); }

void OrreryEditor::resized() {
    auto r = getLocalBounds();
    header_.setBounds(r.removeFromTop(theme::headerH));
    tabs_.setBounds(r.removeFromTop(theme::tabsH));
    routing_->setBounds(r.removeFromBottom(theme::footH));
    lane_->setBounds(r.removeFromBottom(theme::laneH));
    rail_->setBounds(r.removeFromRight(theme::railW));
    view_->setBounds(r);
}
