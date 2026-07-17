// MeasuredView.cpp — density lane (3 draw modes) + onset lane + playhead.
#include "MeasuredView.h"

#include <algorithm>
#include <cmath>

using namespace orrery;

namespace {
const char* kPresetNames[6] = { "FLAT", "RISE", "FALL", "WAVES", "BEATS", "RAND" };
const char* kModeNames[3]   = { "DRAW", "BEZIER", "STEPS" };
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
    for (int i = 0; i < 3; ++i) {
        modes_[i].setButtonText(kModeNames[i]);
        modes_[i].setComponentID("chip-cyan");
        modes_[i].setClickingTogglesState(false);
        modes_[i].onClick = [this, i] { setMode((Mode)i); };
        addAndMakeVisible(modes_[i]);
    }
    modes_[0].setToggleState(true, juce::dontSendNotification);
}

void MeasuredView::setMode(Mode m) {
    mode_ = m;
    for (int i = 0; i < 3; ++i)
        modes_[i].setToggleState((Mode)i == m, juce::dontSendNotification);
    if (m == Mode::Bezier && pts_.empty()) {
        // Seed control points from the current drawn curve so entering bezier
        // starts from what's there, not from scratch.
        for (float t : { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f }) {
            const int i = juce::jlimit(0, 63, (int)std::lround(t * 63));
            pts_.push_back({ t, juce::jlimit(0.0f, 1.0f, snap_.curveDrawn[i]) });
        }
    }
    repaint();
}

MeasuredView::Lane MeasuredView::densityLane() const {
    const auto b = getLocalBounds().toFloat();
    const float pad = (float)theme::pad;
    return { pad, b.getWidth() - pad, pad + 34.0f, b.getHeight() * 0.54f };
}

juce::Point<float> MeasuredView::laneToNorm(juce::Point<float> p) const {
    const Lane L = densityLane();
    return { juce::jlimit(0.0f, 1.0f, (p.x - L.x0) / (L.x1 - L.x0)),
             juce::jlimit(0.0f, 1.0f, (L.bot - p.y) / (L.bot - L.top)) };
}
juce::Point<float> MeasuredView::normToLane(juce::Point<float> n) const {
    const Lane L = densityLane();
    return { L.x0 + (L.x1 - L.x0) * n.x, L.bot - (L.bot - L.top) * n.y };
}

void MeasuredView::resized() {
    const int h = 20, gap = 5;
    // Mode chips, top-left; preset chips, top-right.
    int x = theme::pad;
    for (int i = 0; i < 3; ++i) {
        const int w = i == 1 ? 56 : 48;
        modes_[i].setBounds(x, theme::pad - 6, w, h);
        x += w + gap;
    }
    const int pw = 50;
    x = getWidth() - theme::pad - 6 * pw - 5 * gap;
    for (int i = 0; i < 6; ++i) {
        presets_[i].setBounds(x, theme::pad - 6, pw, h);
        x += pw + gap;
    }
}

// ── DRAW + STEPS painting ─────────────────────────────────────────────────────
void MeasuredView::paintAt(const juce::MouseEvent& e) {
    if (!onGesture) return;
    const auto nrm = laneToNorm(e.position);
    const int n = std::max(1, snap_.n);

    if (mode_ == Mode::Steps) {
        // Fill the whole grid cell (snap to the selected n-grid).
        const int cell = juce::jlimit(0, n - 1, (int)(nrm.x * n));
        const int b0 = cell * MeasuredEuclid::kM / n;
        const int b1 = (cell + 1) * MeasuredEuclid::kM / n;
        for (int bi = b0; bi < b1; ++bi)
            onGesture({ GestureEvent::Type::CurveEdit, bi, nrm.y });
        return;
    }

    // Free draw: interpolate from the previous paint point so fast swipes
    // leave no gaps, one CurveEdit per touched bin (prototype behavior).
    const int bin = (int)std::lround(nrm.x * (MeasuredEuclid::kM - 1));
    if (lastBin_ < 0) { lastBin_ = bin; lastVal_ = nrm.y; }
    const int from = lastBin_, step = bin >= from ? 1 : -1;
    const int nBins = std::abs(bin - from);
    for (int i = 0; i <= nBins; ++i) {
        const int bi = from + i * step;
        const float frac = nBins > 0 ? (float)i / nBins : 1.0f;
        onGesture({ GestureEvent::Type::CurveEdit, bi, lastVal_ + (nrm.y - lastVal_) * frac });
    }
    lastBin_ = bin; lastVal_ = nrm.y;
}

// ── BEZIER editing ────────────────────────────────────────────────────────────
// Catmull-Rom through the control points (clamped ends): smooth curve that
// passes THROUGH every handle — the intuitive "bezier-like" behavior.
float MeasuredView::bezierValueAt(float t) const {
    if (pts_.empty()) return 0.5f;
    if (t <= pts_.front().x) return pts_.front().y;
    if (t >= pts_.back().x) return pts_.back().y;
    size_t s = 0;
    while (s + 1 < pts_.size() && pts_[s + 1].x < t) ++s;
    const auto p1 = pts_[s], p2 = pts_[s + 1];
    const auto p0 = s > 0 ? pts_[s - 1] : p1;
    const auto p3 = s + 2 < pts_.size() ? pts_[s + 2] : p2;
    const float span = std::max(1e-6f, p2.x - p1.x);
    const float u = (t - p1.x) / span, u2 = u * u, u3 = u2 * u;
    const float v = 0.5f * ((2 * p1.y) + (-p0.y + p2.y) * u
                            + (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * u2
                            + (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * u3);
    return juce::jlimit(0.0f, 1.0f, v);
}

void MeasuredView::bezierDown(const juce::MouseEvent& e) {
    const auto nrm = laneToNorm(e.position);
    dragPt_ = -1;
    for (int i = 0; i < (int)pts_.size(); ++i)
        if (normToLane(pts_[(size_t)i]).getDistanceFrom(e.position) < 12.0f) { dragPt_ = i; break; }
    if (dragPt_ < 0) {
        // Add a new point, keeping pts_ sorted by t.
        auto it = std::lower_bound(pts_.begin(), pts_.end(), nrm,
                                   [](const auto& a, const auto& b) { return a.x < b.x; });
        dragPt_ = (int)std::distance(pts_.begin(), pts_.insert(it, nrm));
    }
    repaint();
}

void MeasuredView::bezierDrag(const juce::MouseEvent& e) {
    if (dragPt_ < 0) return;
    auto nrm = laneToNorm(e.position);
    // Keep t strictly between the neighbors (no crossings).
    const float lo = dragPt_ > 0 ? pts_[(size_t)dragPt_ - 1].x + 0.01f : 0.0f;
    const float hi = dragPt_ + 1 < (int)pts_.size() ? pts_[(size_t)dragPt_ + 1].x - 0.01f : 1.0f;
    nrm.x = juce::jlimit(lo, hi, nrm.x);
    pts_[(size_t)dragPt_] = nrm;
    repaint();   // live preview; the curve streams on release
}

void MeasuredView::bezierCommit() {
    if (!onGesture || pts_.empty()) return;
    for (int bi = 0; bi < MeasuredEuclid::kM; ++bi)
        onGesture({ GestureEvent::Type::CurveEdit, bi,
                    bezierValueAt((float)bi / (MeasuredEuclid::kM - 1)) });
}

// ── Mouse dispatch ────────────────────────────────────────────────────────────
void MeasuredView::mouseDown(const juce::MouseEvent& e) {
    lastBin_ = -1;
    const Lane L = densityLane();
    if (e.position.y < L.top - 8 || e.position.y > L.bot + 8) return;
    if (mode_ == Mode::Bezier) bezierDown(e);
    else paintAt(e);
}

void MeasuredView::mouseDrag(const juce::MouseEvent& e) {
    if (mode_ == Mode::Bezier) bezierDrag(e);
    else if (lastBin_ >= 0 || mode_ == Mode::Steps) {
        const Lane L = densityLane();
        if (e.position.y >= L.top - 30 && e.position.y <= L.bot + 30) paintAt(e);
    }
}

void MeasuredView::mouseUp(const juce::MouseEvent&) {
    if (mode_ == Mode::Bezier && dragPt_ >= 0) { bezierCommit(); dragPt_ = -1; }
    lastBin_ = -1;
}

void MeasuredView::mouseDoubleClick(const juce::MouseEvent& e) {
    if (mode_ != Mode::Bezier || pts_.size() <= 2) return;
    for (size_t i = 0; i < pts_.size(); ++i)
        if (normToLane(pts_[i]).getDistanceFrom(e.position) < 12.0f) {
            pts_.erase(pts_.begin() + (long)i);
            bezierCommit();
            repaint();
            return;
        }
}

// ── Painting ──────────────────────────────────────────────────────────────────
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

    // Sounding curve (amber fill).
    {
        juce::Path line; curvePath(snap_.curve, line);
        juce::Path area = line;
        area.lineTo(L.x1, L.bot); area.lineTo(L.x0, L.bot); area.closeSubPath();
        g.setColour(theme::amber.withAlpha(0.12f)); g.fillPath(area);
        g.setColour(theme::amber);                  g.strokePath(line, juce::PathStrokeType(1.4f));
    }
    // Pending drawn curve (dimmed) when it differs.
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
            g.drawText("edits pending — next bar", (int)L.x0 + 66, (int)L.top - 14, 200, 12,
                       juce::Justification::left);
        }
    }
    // Bezier preview: the spline + handles, live while editing.
    if (mode_ == Mode::Bezier && !pts_.empty()) {
        juce::Path bez;
        for (int i = 0; i < 96; ++i) {
            const float t = i / 95.0f;
            const auto p = normToLane({ t, bezierValueAt(t) });
            if (i == 0) bez.startNewSubPath(p); else bez.lineTo(p);
        }
        g.setColour(theme::cyan.withAlpha(0.8f));
        g.strokePath(bez, juce::PathStrokeType(1.2f));
        for (const auto& pt : pts_) {
            const auto p = normToLane(pt);
            g.setColour(theme::well);
            g.fillEllipse(p.x - 5, p.y - 5, 10, 10);
            g.setColour(theme::cyan);
            g.drawEllipse(p.x - 5, p.y - 5, 10, 10, 1.4f);
        }
    }

    // Beat gridlines through both lanes.
    const float gBot = b.getHeight() - (float)theme::pad;
    for (int i = 0; i <= n; ++i) {
        const float x = L.x0 + W * (i / (float)n);
        g.setColour(i % 4 == 0 ? theme::line2 : theme::line);
        g.drawVerticalLine((int)x, L.top, gBot);
    }

    // Onset lane: flat ghosts → warped onsets with deformation links.
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

    // Playhead.
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
    juce::String hint = mode_ == Mode::Bezier ? "drag points · dbl-click removes"
                       : mode_ == Mode::Steps ? "paint fills grid cells"
                                              : "draw the curve above";
    g.drawText("gen " + juce::String(snap_.gen).paddedLeft('0', 3)
                   + "    k " + juce::String(k) + " · n " + juce::String(n) + " · " + hint,
               theme::pad, getHeight() - 24, getWidth() - 2 * theme::pad, 14,
               juce::Justification::left);
    g.setFont(theme::mono(9.5f));
    g.drawText("density", theme::pad, (int)L.top - 14, 60, 12, juce::Justification::left);
}
