// PluginEditor.cpp — chrome assembly + engine-swap + snapshot distribution.
#include "gui/PluginEditor.h"

#include "PluginProcessor.h"
#include "elastic-euclid/gui/ElasticView.h"
#include "gui/Theme.h"
#include "measured-euclid/gui/MeasuredView.h"
#include "probable-euclid/gui/ProbableView.h"

using namespace orrery;

namespace {
// Shell-level rows appended to every engine's rail (routing + the VOICE synth).
std::vector<ParamRail::Row> commonTail() {
    return {
        { "",            "", "", 0 },
        { "quantizeOut", "quantize", "",   2 },
        { "gateMs",      "gate",     "ms", -1 },
        { "gain",        "gain",     "",   2 },
        { "",            "VOICE",    "",   0 },
        { "voiceTune",     "tune",      "st", -1 },
        { "voiceDecay",    "decay",     "ms", -1 },
        { "voiceTransient","transient", "",    2 },
        { "voiceDrop",     "drop",      "st",  1 },
    };
}

struct RailSpec { juce::String title; std::vector<ParamRail::Row> rows; };

RailSpec railFor(int engine) {
    std::vector<ParamRail::Row> r;
    juce::String title;
    if (engine == 1) {          // Measured
        title = "MEASURED EUCLID";
        r = { { "m_k",       "onsets k", "", -1 },
              { "m_n",       "grid n",   "", -1 },
              { "",          "", "", 0 },
              { "m_phase",   "phase",    "",  2 },
              { "m_quantize","quantize", "",  2 },
              { "m_breathePeriod", "breathe", "bar", -1 } };
    } else if (engine == 2) {   // Probable
        title = "PROBABLE EUCLID";
        r = { { "p_n",          "grid n",      "", -1 },
              { "p_density",    "density",     "",  1 },
              { "",             "", "", 0 },
              { "p_temperature","temperature", "",  2 },
              { "p_clump",      "clump",       "",  2 },
              { "p_anchor",     "anchor",      "",  2 } };
    } else {                    // Elastic
        title = "ELASTIC EUCLID";
        r = { { "sources",   "particles k", "", -1 },
              { "wells",     "lattice n",   "", -1 },
              { "",          "", "", 0 },
              { "repulsion", "repulsion",   "",  2 },
              { "lattice",   "lattice pull","",  2 },
              { "damping",   "damping",     "",  2 },
              { "relax",     "relax",       "s", 2 } };
    }
    auto tail = commonTail();
    r.insert(r.end(), tail.begin(), tail.end());
    return { title, std::move(r) };
}
} // namespace

OrreryEditor::OrreryEditor(OrreryProcessor& p)
    : juce::AudioProcessorEditor(p), proc_(p) {
    setLookAndFeel(&lnf_);

    addAndMakeVisible(header_);
    header_.onTick = [this] { proc_.requestManualTick(); };
    header_.bindRun(proc_.apvts());

    tabs_.setTabs({ { "ELASTIC",  "equilibrium", true },
                    { "MEASURED", "measure",     true },
                    { "PROBABLE", "field",       true } });
    tabs_.onSelect = [this](int i) { setEngine(i); };
    addAndMakeVisible(tabs_);

    lane_ = std::make_unique<OffsetLane>(proc_.apvts());
    lane_->onEdit = [this](const OffsetEdit& e) { proc_.pushOffsetEdit(e); };
    addAndMakeVisible(*lane_);

    routing_ = std::make_unique<RoutingBar>(proc_.apvts(),
        [this] { return proc_.virtualMidiOpen(); },
        "0x" + juce::String::toHexString((juce::int64)proc_.projectSeed()).toUpperCase());
    addAndMakeVisible(*routing_);

    // Build the view + rail for whatever engine is currently selected.
    engine_ = (int)proc_.apvts().getRawParameterValue("engine")->load();
    tabs_.selected = engine_;
    setEngine(engine_);

    setResizable(true, true);
    setResizeLimits(900, 660, 1600, 1100);
    setSize(980, 720);
    startTimerHz(30);
}

OrreryEditor::~OrreryEditor() { setLookAndFeel(nullptr); }

void OrreryEditor::setEngine(int index) {
    engine_ = index;
    // Tell the processor (choice param, normalized 0..1 over the 3 choices).
    if (auto* param = proc_.apvts().getParameter("engine"))
        param->setValueNotifyingHost(index / 2.0f);

    if (view_) removeChildComponent(view_.get());
    switch (index) {
        case 1:  view_ = std::make_unique<MeasuredView>(); break;
        case 2:  view_ = std::make_unique<ProbableView>(); break;
        default: view_ = std::make_unique<ElasticView>(); break;
    }
    view_->onGesture = [this](const GestureEvent& g) { proc_.pushGesture(g); };
    addAndMakeVisible(*view_);

    auto spec = railFor(index);
    rail_ = std::make_unique<ParamRail>(proc_.apvts(), spec.title, spec.rows);
    addAndMakeVisible(*rail_);

    resized();
}

void OrreryEditor::timerCallback() {
    GuiSnapshot s;
    if (proc_.readSnapshot(s)) {
        header_.update(s);
        view_->updateSnapshot(s);
        lane_->update(s);
    }
    routing_->repaint();
}

void OrreryEditor::paint(juce::Graphics& g) { g.fillAll(theme::bg); }

void OrreryEditor::resized() {
    auto r = getLocalBounds();
    header_.setBounds(r.removeFromTop(theme::headerH));
    tabs_.setBounds(r.removeFromTop(theme::tabsH));
    routing_->setBounds(r.removeFromBottom(theme::footH));
    lane_->setBounds(r.removeFromBottom(theme::laneH));
    if (rail_) rail_->setBounds(r.removeFromRight(theme::railW));
    if (view_) view_->setBounds(r);
}
