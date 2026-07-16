// OffsetLane.cpp — painted cell grid + drag editing + generator chips.
#include "gui/OffsetLane.h"

using namespace orrery;

namespace {
constexpr int kHdrH = 30, kLabelW = 66, kCellGap = 8, kTRowH = 30, kVRowH = 22, kRowGap = 5;
constexpr float kTSens = 0.15f, kVSens = 0.45f;   // value units per pixel of drag
} // namespace

OffsetLane::OffsetLane(juce::AudioProcessorValueTreeState& apvts) {
    for (auto* b : { &walk_, &accent_, &contour_, &unlockAll_ }) {
        b->setComponentID(b == &accent_ ? "chip-cyan" : "chip");
        addAndMakeVisible(*b);
    }
    walk_.setClickingTogglesState(true);
    accent_.setClickingTogglesState(true);
    walkAttach_   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "walkOn", walk_);
    accentAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "accentOn", accent_);
    contour_.setEnabled(false);
    contour_.setTooltip("contour generator ships at O5");
    unlockAll_.onClick = [this] {
        if (onEdit) onEdit({ OffsetEdit::Type::UnlockAll, 0, 0 });
    };
}

void OffsetLane::update(const GuiSnapshot& s) {
    snap_ = s;
    repaint();
}

void OffsetLane::resized() {
    const int y = 8, h = 22;
    walk_.setBounds(theme::pad + 96, y, 52, h);
    accent_.setBounds(theme::pad + 154, y, 62, h);
    contour_.setBounds(theme::pad + 222, y, 70, h);
    unlockAll_.setBounds(getWidth() - theme::pad - 92, y, 92, h);
}

juce::Rectangle<int> OffsetLane::cellRect(int row, int i) const {
    const int k = std::max(1, (int)snap_.k);
    const int gridX = theme::pad + kLabelW;
    const int gridW = getWidth() - gridX - theme::pad;
    const int cw = (gridW - (k - 1) * kCellGap) / k;
    const int y0 = kHdrH + 16;
    const int y = row == 0 ? y0 : y0 + kTRowH + kRowGap;
    return { gridX + i * (cw + kCellGap), y, cw, row == 0 ? kTRowH : kVRowH };
}

int OffsetLane::cellAt(const juce::Point<int>& p, int& rowOut) const {
    for (int row = 0; row < 2; ++row)
        for (int i = 0; i < snap_.k; ++i)
            if (cellRect(row, i).contains(p)) { rowOut = row; return i; }
    rowOut = -1;
    return -1;
}

void OffsetLane::paint(juce::Graphics& g) {
    g.fillAll(theme::bg);
    g.setColour(theme::line);
    g.fillRect(0, 0, getWidth(), 1);

    g.setFont(theme::label(10.0f));
    g.setColour(theme::dim);
    g.drawText("OFFSET LAYER", theme::pad, 8, 90, 22, juce::Justification::centredLeft);

    // Source index header row.
    g.setFont(theme::mono(9.5f));
    for (int i = 0; i < snap_.k; ++i) {
        auto r = cellRect(0, i);
        g.setColour(theme::dim);
        g.drawText(juce::String(i), r.getX(), kHdrH + 2, r.getWidth(), 12, juce::Justification::centred);
    }
    g.setFont(theme::mono(9.0f));
    g.setColour(theme::dim);
    g.drawText("transpose", theme::pad, cellRect(0, 0).getY(), kLabelW - 8, kTRowH, juce::Justification::centredLeft);
    g.drawText("velocity",  theme::pad, cellRect(1, 0).getY(), kLabelW - 8, kVRowH, juce::Justification::centredLeft);

    // Cells. Locked = amber tint + pin dot (hand edits are pins).
    for (int row = 0; row < 2; ++row) {
        for (int i = 0; i < snap_.k; ++i) {
            const auto& c = snap_.cells[i];
            const bool locked = row == 0 ? c.lockT : c.lockV;
            const int  v      = row == 0 ? c.transpose : c.velOffset;
            auto r = cellRect(row, i).toFloat();

            g.setColour(locked ? theme::amber.withAlpha(0.09f) : theme::panel);
            g.fillRoundedRectangle(r, theme::corner);
            g.setColour(locked ? theme::amberDim : theme::line);
            g.drawRoundedRectangle(r.reduced(0.5f), theme::corner, 1.0f);

            g.setFont(theme::mono(row == 0 ? 12.0f : 10.5f));
            g.setColour(locked ? theme::amber : row == 0 ? theme::text : theme::dim);
            g.drawText((v > 0 ? "+" : "") + juce::String(v), r.toNearestInt(), juce::Justification::centred);

            if (locked) {
                g.setColour(theme::amber);
                g.fillEllipse(r.getRight() - 7.0f, r.getY() + 4.0f, 3.0f, 3.0f);
            }
        }
    }
}

void OffsetLane::mouseDown(const juce::MouseEvent& e) {
    dragId_ = cellAt(e.getPosition(), dragRow_);
    if (dragId_ >= 0) {
        const auto& c = snap_.cells[dragId_];
        dragStart_ = dragRow_ == 0 ? c.transpose : c.velOffset;
        dragValue_ = dragStart_;
    }
}

void OffsetLane::mouseDrag(const juce::MouseEvent& e) {
    if (dragId_ < 0 || !onEdit) return;
    const float sens = dragRow_ == 0 ? kTSens : kVSens;
    const int lim = dragRow_ == 0 ? 24 : 64;
    const int v = juce::jlimit(-lim, lim,
        dragStart_ + (int)std::lround(-e.getDistanceFromDragStartY() * sens));
    if (v != dragValue_) {
        dragValue_ = v;
        onEdit({ dragRow_ == 0 ? OffsetEdit::Type::Transpose : OffsetEdit::Type::VelOffset,
                 dragId_, v });
    }
}

void OffsetLane::mouseDoubleClick(const juce::MouseEvent& e) {
    int row = -1;
    const int id = cellAt(e.getPosition(), row);
    if (id >= 0 && onEdit) onEdit({ OffsetEdit::Type::ResetCell, id, 0 });
}
