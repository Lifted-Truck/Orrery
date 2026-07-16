// ElasticView.cpp — the ring, ported from elastic-euclid-2.html + the mockup.
#include "ElasticView.h"

#include <algorithm>
#include <cmath>

using namespace orrery;

namespace {
constexpr double kTwoPi = 6.28318530717958647692;
double wrap01(double x) { return std::fmod(std::fmod(x, 1.0) + 1.0, 1.0); }
double sdist(double a, double b) { return wrap01(b - a + 0.5) - 0.5; }
// Phase [0,1) → screen angle (12 o'clock = 0, clockwise).
float ang(double th) { return (float)(th * kTwoPi - kTwoPi / 4.0); }
} // namespace

ElasticView::ElasticView() { setOpaque(true); }

void ElasticView::updateSnapshot(const GuiSnapshot& s) {
    snap_ = s;
    computeGhosts();
    repaint();
}

// Nearest-rotation alignment of E(k,n) to the current particle set — the
// prototype's ghostPositions(): try every rotation, best cyclic alignment of
// the two sorted phase lists by summed squared sdist, keep the argmin.
void ElasticView::computeGhosts() {
    ghosts_.clear();
    rms_ = 0.0;
    const int k = snap_.k, n = snap_.n;
    if (k <= 0 || n <= 0 || n > 64) return;

    bool pat[64];
    ElasticEuclid::bjorklund(std::min(k, n), n, pat);
    std::vector<double> onsets;
    for (int i = 0; i < n; ++i) if (pat[i]) onsets.push_back((double)i / n);
    if (onsets.empty()) return;

    std::vector<double> th(snap_.theta, snap_.theta + k);
    std::sort(th.begin(), th.end());

    double bestErr = 1e18;
    std::vector<double> best = onsets;
    for (int r = 0; r < n; ++r) {
        std::vector<double> g;
        g.reserve(onsets.size());
        for (double o : onsets) g.push_back(wrap01(o + (double)r / n));
        std::sort(g.begin(), g.end());
        const int m = (int)std::min(g.size(), th.size());
        double bestLocal = 1e18;
        for (size_t off = 0; off < g.size(); ++off) {
            double e = 0.0;
            for (int i = 0; i < m; ++i) { const double d = sdist(th[(size_t)i], g[(off + (size_t)i) % g.size()]); e += d * d; }
            bestLocal = std::min(bestLocal, e);
        }
        if (bestLocal < bestErr) { bestErr = bestLocal; best = g; }
    }
    ghosts_ = best;
    rms_ = std::sqrt(bestErr / std::max<size_t>(1, std::min(best.size(), (size_t)k)));
}

void ElasticView::paint(juce::Graphics& g) {
    const auto b = getLocalBounds().toFloat();
    // Recessed canvas: well ground with a soft center lift (mockup radial).
    g.fillAll(theme::well);
    {
        juce::ColourGradient grad(juce::Colour(0xff12171c), b.getCentreX(), b.getHeight() * 0.42f,
                                  theme::well, b.getCentreX(), b.getBottom(), true);
        g.setGradientFill(grad);
        g.fillAll();
    }

    const float cx = b.getCentreX(), cy = b.getHeight() * 0.46f;
    const float R = std::min(b.getWidth(), b.getHeight()) * 0.34f;
    const int k = snap_.k, n = snap_.n;

    // Ring.
    g.setColour(theme::line2);
    g.drawEllipse(cx - R, cy - R, R * 2, R * 2, 1.4f);

    // Lattice ticks — length ∝ lattice pull is an engine param the snapshot
    // doesn't carry; use the prototype's default visual weight.
    const float tick = 10.0f;
    for (int i = 0; i < n; ++i) {
        const float a = ang((double)i / n);
        const bool major = (n % 4 == 0) ? (i % (n / 4) == 0) : (i == 0);
        g.setColour(major ? theme::dim : theme::line);
        g.drawLine(cx + std::cos(a) * (R - tick), cy + std::sin(a) * (R - tick),
                   cx + std::cos(a) * (R + tick), cy + std::sin(a) * (R + tick),
                   major ? 1.4f : 1.0f);
    }

    // E(k,n) equilibrium ghosts: hollow cyan diamonds.
    g.setColour(theme::cyan);
    for (double o : ghosts_) {
        const float a = ang(o), x = cx + std::cos(a) * R, y = cy + std::sin(a) * R;
        juce::Path d;
        d.addRectangle(-4.5f, -4.5f, 9.0f, 9.0f);
        d.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4).translated(x, y));
        g.strokePath(d, juce::PathStrokeType(1.3f));
    }

    // Playhead sweep (lap = one bar at the default clock config).
    if (snap_.isPlaying) {
        const double qPerBar = 4.0 * snap_.timeSigNum / snap_.timeSigDen;
        const float a = ang(wrap01(snap_.ppq / qPerBar));
        g.setColour(theme::cyan.withAlpha(0.55f));
        g.drawLine(cx + std::cos(a) * (R * 0.16f), cy + std::sin(a) * (R * 0.16f),
                   cx + std::cos(a) * (R + 13.0f), cy + std::sin(a) * (R + 13.0f), 1.4f);
    }

    // Particles: amber dots + id, dashed energy whiskers (pending ω).
    for (int i = 0; i < k; ++i) {
        const float a = ang(snap_.theta[i]);
        const float x = cx + std::cos(a) * R, y = cy + std::sin(a) * R;
        const double om = snap_.omega[i];
        const float L = (float)(std::min(26.0, std::abs(om) * 60.0) * (om < 0 ? -1.0 : 1.0));
        if (std::abs(L) > 2.0f) {
            const float t = a + juce::MathConstants<float>::pi / 2;
            g.setColour(theme::amber.withAlpha(0.5f));
            const float dashes[] = { 2.0f, 3.0f };
            g.drawDashedLine(juce::Line<float>(x, y, x + std::cos(t) * L, y + std::sin(t) * L), dashes, 2, 1.3f);
        }
        g.setColour(theme::amber);
        g.fillEllipse(x - 5.4f, y - 5.4f, 10.8f, 10.8f);
        g.setColour(theme::bg);
        g.setFont(theme::mono(8.0f, true));
        g.drawText(juce::String(i), (int)x - 6, (int)y - 5, 12, 10, juce::Justification::centred);
    }

    // Center glyph + bottom readout + legend (all mono, prototype voice).
    g.setFont(theme::mono(9.0f));
    g.setColour(theme::dim.withAlpha(0.6f));
    g.drawText("k " + juce::String(k) + " · n " + juce::String(n),
               (int)cx - 40, (int)cy - 6, 80, 12, juce::Justification::centred);

    g.setFont(theme::mono(11.0f));
    const bool settled = rms_ < 0.004;
    juce::String left = "gen " + juce::String(snap_.gen).paddedLeft('0', 3)
                      + "    " + juce::String::formatted(u8"Δ %.1f%%", rms_ * 100.0);
    g.setColour(theme::dim);
    g.drawText(left, theme::pad, getHeight() - 26, 250, 14, juce::Justification::left);
    if (settled) {
        g.setColour(theme::cyan);
        g.drawText(u8"settled ✓", theme::pad + 160, getHeight() - 26, 90, 14, juce::Justification::left);
    }
    g.setColour(theme::dim);
    g.drawText("E(" + juce::String(std::min(k, n)) + "," + juce::String(n) + ")",
               getWidth() - 90, getHeight() - 26, 74, 14, juce::Justification::right);

    g.setFont(theme::mono(9.5f));
    const int lx = getWidth() - 118;
    g.setColour(theme::amber);  g.fillEllipse((float)lx, 15.0f, 8.0f, 8.0f);
    g.setColour(theme::dim);    g.drawText("particle", lx + 14, 13, 90, 12, juce::Justification::left);
    {
        juce::Path d; d.addRectangle(-3.4f, -3.4f, 6.8f, 6.8f);
        d.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4).translated((float)lx + 4, 35.0f));
        g.setColour(theme::cyan); g.strokePath(d, juce::PathStrokeType(1.2f));
        g.setColour(theme::dim);  g.drawText("E(k,n) ghost", lx + 14, 29, 96, 12, juce::Justification::left);
    }
}

// Click near a particle → kick that particle (spec §2.5 via the gesture path);
// click elsewhere → global kick.
void ElasticView::mouseUp(const juce::MouseEvent& e) {
    if (!onGesture) return;
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX(), cy = b.getHeight() * 0.46f;
    const float R = std::min(b.getWidth(), b.getHeight()) * 0.34f;
    for (int i = 0; i < snap_.k; ++i) {
        const float a = ang(snap_.theta[i]);
        const juce::Point<float> p(cx + std::cos(a) * R, cy + std::sin(a) * R);
        if (p.getDistanceFrom(e.position) < 14.0f) {
            onGesture({ GestureEvent::Type::Kick, i, 0.6f });
            return;
        }
    }
    onGesture({ GestureEvent::Type::Kick, 0, 1.0f });
}
