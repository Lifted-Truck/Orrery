// MeasuredView.cpp — density lane + onset lane (measured-euclid.html language).
#include "MeasuredView.h"

using namespace orrery;

void MeasuredView::paint(juce::Graphics& g) {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(theme::well);

    const float pad = (float)theme::pad;
    const float x0 = pad, x1 = b.getWidth() - pad;
    const float W = x1 - x0;
    const int   n = std::max(1, snap_.n), k = snap_.k;

    // ── Density lane (top ~56%) — the active measure, amber area fill ────────
    const float dTop = pad + 8, dBot = b.getHeight() * 0.54f;
    const float dH = dBot - dTop;
    juce::Path area;
    area.startNewSubPath(x0, dBot);
    for (int i = 0; i < 64; ++i) {
        const float x = x0 + W * (i / 63.0f);
        const float y = dBot - dH * juce::jlimit(0.0f, 1.0f, snap_.curve[i]);
        area.lineTo(x, y);
    }
    area.lineTo(x1, dBot);
    area.closeSubPath();
    g.setColour(theme::amber.withAlpha(0.12f));
    g.fillPath(area);
    g.setColour(theme::amber);
    juce::Path line;
    for (int i = 0; i < 64; ++i) {
        const float x = x0 + W * (i / 63.0f);
        const float y = dBot - dH * juce::jlimit(0.0f, 1.0f, snap_.curve[i]);
        if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
    }
    g.strokePath(line, juce::PathStrokeType(1.4f));

    // Beat gridlines through both lanes.
    const float gTop = dTop, gBot = b.getHeight() - pad;
    for (int i = 0; i <= n; ++i) {
        const float x = x0 + W * (i / (float)n);
        g.setColour(i % 4 == 0 ? theme::line2 : theme::line);
        g.drawVerticalLine((int)x, gTop, gBot);
    }

    // ── Onset lane (bottom) — ghosts (flat) → warped actual with links ───────
    const float oY = b.getHeight() * 0.78f;
    g.setColour(theme::line2);
    g.drawHorizontalLine((int)oY, x0, x1);

    for (int i = 0; i < k; ++i) {
        const float ghost = x0 + W * ((i + 0.5f) / k);          // flat-measure position
        const float act   = x0 + W * (float)snap_.theta[i];      // warped onset
        // deformation link
        g.setColour(theme::cyan.withAlpha(0.35f));
        g.drawLine(ghost, oY - 16, act, oY, 1.0f);
        // ghost diamond
        juce::Path d; d.addRectangle(-3.4f, -3.4f, 6.8f, 6.8f);
        d.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4)
                             .translated(ghost, oY - 16));
        g.setColour(theme::cyan); g.strokePath(d, juce::PathStrokeType(1.1f));
        // actual onset dot, radius ∝ energy
        const float r = 3.5f + 3.0f * snap_.energy[i];
        g.setColour(theme::amber);
        g.fillEllipse(act - r, oY - r, r * 2, r * 2);
    }

    // Readout.
    g.setFont(theme::mono(11.0f));
    g.setColour(theme::dim);
    g.drawText("gen " + juce::String(snap_.gen).paddedLeft('0', 3)
                   + "    k " + juce::String(k) + " · n " + juce::String(n)
                   + " · measure-warped onsets",
               (int)pad, getHeight() - 24, getWidth() - 2 * (int)pad, 14,
               juce::Justification::left);
    g.setFont(theme::mono(9.5f));
    g.drawText("density", (int)pad, (int)dTop - 2, 80, 12, juce::Justification::left);
}
