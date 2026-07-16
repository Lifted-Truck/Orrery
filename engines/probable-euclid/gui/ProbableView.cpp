// ProbableView.cpp — radial probability field (probable-euclid.html language).
#include "ProbableView.h"

#include <cmath>

using namespace orrery;

namespace {
constexpr double kTwoPi = 6.28318530717958647692;
float ang(double th) { return (float)(th * kTwoPi - kTwoPi / 4.0); }
}

void ProbableView::paint(juce::Graphics& g) {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(theme::well);
    {
        juce::ColourGradient grad(juce::Colour(0xff12171c), b.getCentreX(), b.getHeight() * 0.42f,
                                  theme::well, b.getCentreX(), b.getBottom(), true);
        g.setGradientFill(grad); g.fillAll();
    }

    const float cx = b.getCentreX(), cy = b.getHeight() * 0.46f;
    const float R = std::min(b.getWidth(), b.getHeight()) * 0.30f;
    const int n = std::max(1, snap_.n);
    const float barMax = R * 0.7f;
    double expected = 0.0;

    // Ring.
    g.setColour(theme::line2);
    g.drawEllipse(cx - R, cy - R, R * 2, R * 2, 1.4f);

    for (int i = 0; i < n; ++i) {
        const float a = ang((double)i / n);
        const float p = juce::jlimit(0.0f, 1.0f, snap_.energy[i]);
        expected += p;
        const float ux = std::cos(a), uy = std::sin(a);

        // Probability bar outward from the ring (opacity + height ∝ p_i).
        g.setColour(theme::amber.withAlpha(0.20f + 0.6f * p));
        g.drawLine(cx + ux * R, cy + uy * R, cx + ux * (R + barMax * p), cy + uy * (R + barMax * p), 2.4f);

        // Backbone (high-p) as a cyan diamond just inside the ring.
        if (p > 0.6f) {
            juce::Path d; d.addRectangle(-3.0f, -3.0f, 6.0f, 6.0f);
            d.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4)
                                 .translated(cx + ux * (R - 12), cy + uy * (R - 12)));
            g.setColour(theme::cyan); g.strokePath(d, juce::PathStrokeType(1.2f));
        }
        // Realized onset: solid dot on the ring (radius ∝ p).
        if (snap_.realized[i]) {
            const float r = 3.5f + 3.0f * p;
            g.setColour(theme::amber);
            g.fillEllipse(cx + ux * R - r, cy + uy * R - r, r * 2, r * 2);
        }
    }

    // Playhead sweep (the scrubber).
    if (snap_.isPlaying) {
        const double qPerBar = 4.0 * snap_.timeSigNum / std::max(1, snap_.timeSigDen);
        const double ph = std::fmod(std::fmod(snap_.ppq / qPerBar, 1.0) + 1.0, 1.0);
        const float a = ang(ph);
        g.setColour(theme::cyan.withAlpha(0.55f));
        g.drawLine(cx + std::cos(a) * (R * 0.16f), cy + std::sin(a) * (R * 0.16f),
                   cx + std::cos(a) * (R + barMax + 6), cy + std::sin(a) * (R + barMax + 6), 1.4f);
    }

    // Center + readout: expected vs realized count, entropy-free honest numbers.
    int realized = 0;
    for (int i = 0; i < n; ++i) realized += snap_.realized[i] ? 1 : 0;
    g.setFont(theme::mono(9.0f));
    g.setColour(theme::dim.withAlpha(0.6f));
    g.drawText("n " + juce::String(n), (int)cx - 40, (int)cy - 6, 80, 12, juce::Justification::centred);

    g.setFont(theme::mono(11.0f));
    g.setColour(theme::dim);
    g.drawText("gen " + juce::String(snap_.gen).paddedLeft('0', 3)
                   + juce::String::formatted("    E[k] %.1f · realized %d", expected, realized),
               theme::pad, getHeight() - 26, 320, 14, juce::Justification::left);

    g.setFont(theme::mono(9.5f));
    const int lx = getWidth() - 118;
    g.setColour(theme::amber); g.fillEllipse((float)lx, 15.0f, 8.0f, 8.0f);
    g.setColour(theme::dim);   g.drawText("realized", lx + 14, 13, 90, 12, juce::Justification::left);
    g.setColour(theme::amber.withAlpha(0.5f)); g.fillRect((float)lx, 33.0f, 8.0f, 8.0f);
    g.setColour(theme::dim);   g.drawText("p field", lx + 14, 31, 90, 12, juce::Justification::left);
}
