// MeasuredView.cpp — density lane (drawable) + onset lane + playhead.
#include "MeasuredView.h"

#include <cmath>

using namespace orrery;

namespace {
const char* kPresetNames[6] = { "FLAT", "RISE", "FALL", "WAVES", "BEATS", "RAND" };
double wrap01(double x) { return std::fmod(std::fmod(x, 1.0) + 1.0, 1.0); }
}

MeasuredView::MeasuredView() {
    setOpaque(true);
    for (int i = 0; i < 6; ++i) {
        presets_[i].setButtonText(kPresetNames[i]);
        presets_[i].setComponentID("chip");
        presets_[i].onClick = [this, i] {
            if (onGesture) onGesture({ GestureEvent::Type::CurveEdit, -1, (float)i });
        };
        addAndMakeVisible(presets_[i]);
    }
}

MeasuredView::Lane MeasuredView::densityLane() const {
    const auto b = getLocalBounds().toFloat();
    const float pad = (float)theme::pad;
    return { pad, b.getWidth() - pad, pad + 34.0f, b.getHeight() * 0.54f };
}

void MeasuredView::resized() {
    // Preset chips, top-right row above the density lane.
    const int w = 52, h = 20, gap = 5;
    int x = getWidth() - theme::pad - 6 * w - 5 * gap;
    for (int i = 0; i < 6; ++i) {
        presets_[i].setBounds(x, theme::pad - 6, w, h);
        x += w + gap;
    }
}

void MeasuredView::paintAt(const juce::MouseEvent& e) {
    if (!onGesture) return;
    const Lane L = densityLane();
    const float t = juce::jlimit(0.0f, 1.0f, (e.position.x - L.x0) / (L.x1 - L.x0));
    const float v = juce::jlimit(0.0f, 1.0f, (L.bot - e.position.y) / (L.bot - L.top));
    const int bin = (int)std::lround(t * (MeasuredEuclid::kM - 1));

    // Interpolate from the previous paint point so fast swipes leave no gaps
    // (the prototype's segment interpolation), one CurveEdit per touched bin.
    if (lastBin_ < 0) { lastBin_ = bin; lastVal_ = v; }
    const int from = lastBin_, step = bin >= from ? 1 : -1;
    const int nBins = std::abs(bin - from);
    for (int i = 0; i <= nBins; ++i) {
        const int bi = from + i * step;
        const float frac = nBins > 0 ? (float)i / nBins : 1.0f;
        onGesture({ GestureEvent::Type::CurveEdit, bi, lastVal_ + (v - lastVal_) * frac });
    }
    lastBin_ = bin; lastVal_ = v;
}

void MeasuredView::mouseDown(const juce::MouseEvent& e) {
    lastBin_ = -1;
    const Lane L = densityLane();
    if (e.position.y >= L.top - 8 && e.position.y <= L.bot + 8) paintAt(e);
}

void MeasuredView::mouseDrag(const juce::MouseEvent& e) {
    if (lastBin_ >= 0) paintAt(e);
}

void MeasuredView::mouseUp(const juce::MouseEvent&) { lastBin_ = -1; }

void MeasuredView::paint(juce::Graphics& g) {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(theme::well);

    const Lane L = densityLane();
    const float W = L.x1 - L.x0, dH = L.bot - L.top;
    const int n = std::max(1, snap_.n), k = snap_.k;

    auto curvePath = [&](const float* c, juce::Path& line) {
        for (int i = 0; i < 64; ++i) {
            const float x = L.x0 + W * (i / 63.0f);
            const float y = L.bot - dH * juce::jlimit(0.0f, 1.0f, c[i]);
            if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
        }
    };

    // ── Density lane: sounding curve (amber fill) ─────────────────────────────
    {
        juce::Path line; curvePath(snap_.curve, line);
        juce::Path area = line;
        area.lineTo(L.x1, L.bot); area.lineTo(L.x0, L.bot); area.closeSubPath();
        g.setColour(theme::amber.withAlpha(0.12f)); g.fillPath(area);
        g.setColour(theme::amber);                  g.strokePath(line, juce::PathStrokeType(1.4f));
    }
    // Pending hand edits: dimmed overlay + indicator, only when it differs.
    {
        bool pending = false;
        for (int i = 0; i < 64 && !pending; ++i)
            if (std::abs(snap_.curveDrawn[i] - snap_.curve[i]) > 0.02f) pending = true;
        if (pending) {
            juce::Path line; curvePath(snap_.curveDrawn, line);
            g.setColour(theme::text.withAlpha(0.35f));
            g.strokePath(line, juce::PathStrokeType(1.1f));
            g.setFont(theme::mono(9.5f));
            g.setColour(theme::amberDim);
            g.drawText("edits pending — next bar", (int)L.x0, (int)L.top - 14, 220, 12,
                       juce::Justification::left);
        }
    }

    // Beat gridlines through both lanes.
    const float gBot = b.getHeight() - (float)theme::pad;
    for (int i = 0; i <= n; ++i) {
        const float x = L.x0 + W * (i / (float)n);
        g.setColour(i % 4 == 0 ? theme::line2 : theme::line);
        g.drawVerticalLine((int)x, L.top, gBot);
    }

    // ── Onset lane: flat ghosts → warped onsets with deformation links ───────
    const float oY = b.getHeight() * 0.78f;
    g.setColour(theme::line2);
    g.drawHorizontalLine((int)oY, L.x0, L.x1);
    for (int i = 0; i < k; ++i) {
        const float ghost = L.x0 + W * ((i + 0.5f) / k);
        const float act   = L.x0 + W * (float)snap_.theta[i];
        g.setColour(theme::cyan.withAlpha(0.35f));
        g.drawLine(ghost, oY - 16, act, oY, 1.0f);
        juce::Path d; d.addRectangle(-3.4f, -3.4f, 6.8f, 6.8f);
        d.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4)
                             .translated(ghost, oY - 16));
        g.setColour(theme::cyan); g.strokePath(d, juce::PathStrokeType(1.1f));
        const float r = 3.5f + 3.0f * snap_.energy[i];
        g.setColour(theme::amber);
        g.fillEllipse(act - r, oY - r, r * 2, r * 2);
    }

    // ── Playhead sweep (the scrubber) ─────────────────────────────────────────
    if (snap_.isPlaying) {
        const double qPerBar = 4.0 * snap_.timeSigNum / std::max(1, snap_.timeSigDen);
        const float x = L.x0 + W * (float)wrap01(snap_.ppq / qPerBar);
        g.setColour(theme::cyan.withAlpha(0.65f));
        g.drawLine(x, L.top - 6, x, gBot, 1.4f);
        juce::Path tri;
        tri.addTriangle(x - 4, L.top - 6, x + 4, L.top - 6, x, L.top + 1);
        g.setColour(theme::cyan); g.fillPath(tri);
    }

    // Readout.
    g.setFont(theme::mono(11.0f));
    g.setColour(theme::dim);
    g.drawText("gen " + juce::String(snap_.gen).paddedLeft('0', 3)
                   + "    k " + juce::String(k) + " · n " + juce::String(n)
                   + " · draw the curve above",
               theme::pad, getHeight() - 24, getWidth() - 2 * theme::pad, 14,
               juce::Justification::left);
    g.setFont(theme::mono(9.5f));
    g.drawText("density", theme::pad, (int)L.top - 14, 60, 12, juce::Justification::left);
}
