// ParamRail.cpp — parameter rows on panel ground.
#include "gui/ParamRail.h"

using namespace orrery;

namespace {
constexpr int kRowH = 26, kDivH = 13, kTitleH = 24, kSecH = 24;
// A spec row with empty paramId is a divider; empty paramId + a label is a
// section header (e.g. "VOICE").
bool isHeader(const juce::String& id, const juce::String& label) {
    return id.isEmpty() && label.isNotEmpty();
}
}

ParamRail::ParamRail(juce::AudioProcessorValueTreeState& apvts,
                     juce::String engineTitle, std::vector<Row> rows)
    : title_(std::move(engineTitle)) {
    for (auto& r : rows) {
        Bound b;
        b.row = r;
        if (r.paramId.isNotEmpty()) {
            b.slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                      juce::Slider::NoTextBox);
            b.value = std::make_unique<juce::Label>();
            b.value->setFont(theme::mono(11.0f));
            b.value->setColour(juce::Label::textColourId, theme::text);
            b.value->setJustificationType(juce::Justification::centredRight);
            b.value->setInterceptsMouseClicks(false, false);
            b.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, r.paramId, *b.slider);
            addAndMakeVisible(*b.slider);
            addAndMakeVisible(*b.value);
        }
        rows_.push_back(std::move(b));
    }
    // Wire callbacks AFTER rows_ is fully built, capturing the INDEX — a
    // pointer to the loop-local Bound (or into a still-growing vector) dangles
    // once moved/reallocated, and the first slider touch dereferences it.
    for (size_t i = 0; i < rows_.size(); ++i) {
        if (!rows_[i].slider) continue;
        rows_[i].slider->onValueChange = [this, i] { refreshValue(rows_[i]); };
        refreshValue(rows_[i]);
    }
}

void ParamRail::refreshValue(Bound& b) {
    const double v = b.slider->getValue();
    juce::String t = b.row.decimals < 0 ? juce::String((int)std::lround(v))
                                        : juce::String(v, b.row.decimals);
    b.value->setText(t + b.row.suffix, juce::dontSendNotification);
}

int ParamRail::preferredHeight() const {
    int y = theme::pad + kTitleH;
    for (auto& b : rows_)
        y += b.slider ? kRowH : (isHeader(b.row.paramId, b.row.label) ? kSecH : kDivH);
    return y + theme::pad;
}

void ParamRail::resized() {
    int y = theme::pad + kTitleH;
    for (auto& b : rows_) {
        if (!b.slider) { y += isHeader(b.row.paramId, b.row.label) ? kSecH : kDivH; continue; }
        const int w = getWidth() - theme::pad * 2;
        b.slider->setBounds(theme::pad + 78, y + 3, w - 78 - 48, kRowH - 6);
        b.value->setBounds(getWidth() - theme::pad - 46, y, 46, kRowH);
        y += kRowH;
    }
}

void ParamRail::paint(juce::Graphics& g) {
    g.fillAll(theme::panel);
    g.setColour(theme::line);
    g.fillRect(0, 0, 1, getHeight());   // left hairline against the stage

    g.setFont(theme::label(10.0f));
    g.setColour(theme::amber);
    g.drawText(title_, theme::pad, theme::pad - 4, getWidth() - theme::pad * 2, 16,
               juce::Justification::centredLeft);

    g.setFont(theme::sans(11.0f));
    int y = theme::pad + kTitleH;
    for (auto& b : rows_) {
        if (!b.slider) {
            if (isHeader(b.row.paramId, b.row.label)) {
                g.setColour(theme::line);
                g.fillRect(theme::pad, y + 3, getWidth() - theme::pad * 2, 1);
                g.setFont(theme::label(10.0f));
                g.setColour(theme::amber);
                g.drawText(b.row.label, theme::pad, y + 6, getWidth() - theme::pad * 2, 16,
                           juce::Justification::centredLeft);
                y += kSecH;
            } else {
                g.setColour(theme::line);
                g.fillRect(theme::pad, y + kDivH / 2, getWidth() - theme::pad * 2, 1);
                y += kDivH;
            }
            continue;
        }
        g.setFont(theme::sans(11.0f));
        g.setColour(theme::dim);
        g.drawText(b.row.label, theme::pad, y, 76, kRowH, juce::Justification::centredLeft);
        y += kRowH;
    }
}
