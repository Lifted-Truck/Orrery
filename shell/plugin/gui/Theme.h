// Theme.h — the Orrery design system (single source of visual truth).
//
// Tokens lifted verbatim from the validated HTML prototypes (approved mockup:
// "match the prototypes"). EVERY widget and view pulls color/type/metric from
// here — no ad-hoc colors anywhere else. This file is the anti-mess keystone:
// changing the look means changing tokens, never hunting through components.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace orrery::theme {

// ── Color tokens (prototype :root palette) ───────────────────────────────────
inline const juce::Colour bg       { 0xff14181d };  // device ground
inline const juce::Colour panel    { 0xff1b2129 };  // rails / raised surfaces
inline const juce::Colour well     { 0xff0f1317 };  // canvas recess
inline const juce::Colour line     { 0xff2a323b };  // hairlines
inline const juce::Colour line2    { 0xff3a444f };  // stronger strokes / chips
inline const juce::Colour text     { 0xffc9d2da };  // primary text
inline const juce::Colour dim      { 0xff6b7784 };  // secondary text / labels
inline const juce::Colour amber    { 0xfff0a840 };  // live state / primary accent
inline const juce::Colour amberDim { 0xff8a6425 };  // locked / muted amber
inline const juce::Colour cyan     { 0xff58c7d4 };  // equilibrium ghosts / host sync

// ── Type tokens ──────────────────────────────────────────────────────────────
// Mono carries every numeric readout; sans carries labels/wordmark. System
// faces (SF Pro / SF Mono here) — no bundled webfonts in the plugin binary.
inline juce::Font mono(float h, bool bold = false) {
    auto o = juce::FontOptions().withName(juce::Font::getDefaultMonospacedFontName()).withHeight(h);
    return juce::Font(bold ? o.withStyle("Bold") : o);
}
inline juce::Font sans(float h, bool bold = false) {
    auto o = juce::FontOptions().withName(juce::Font::getDefaultSansSerifFontName()).withHeight(h);
    return juce::Font(bold ? o.withStyle("Bold") : o);
}
// Uppercase micro-label with tracking (the prototypes' .sect-title voice).
inline juce::Font label(float h = 10.0f) { return mono(h).withExtraKerningFactor(0.14f); }

// ── Metric tokens ────────────────────────────────────────────────────────────
inline constexpr int headerH  = 44;
inline constexpr int tabsH    = 36;
inline constexpr int railW    = 264;
inline constexpr int laneH    = 122;
inline constexpr int footH    = 40;
inline constexpr int pad      = 16;
inline constexpr float corner = 5.0f;

} // namespace orrery::theme
