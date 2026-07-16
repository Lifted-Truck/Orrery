// Voices.h — internal fallback drum voices (contract §4).
//
// The prototype's validated percussion voice (elastic-euclid-2.html trigger()),
// ported and made user-controllable: a PITCHED, percussive hit — sine body
// whose frequency drops from `dropSemis` above the target down to the target
// over 50 ms (tom/membrane character) with an exponential amplitude decay, plus
// a short cubic-faded noise CLICK as the attack transient only. No sustained
// noise. Onboard pitch = MIDI note + `tuneSemis` (voice-only; the MIDI OUT
// note is unchanged, so this is a monitoring tuning, not a re-pitch of the
// sequence). RT-safe: fixed voice array, no allocation, no wall-clock — per-
// voice xorshift noise for the click. Framework-free.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "orrery/Pcg32.h"
#include "orrery/Types.h"

namespace orrery {

// User-facing voice controls (the "VOICE" rail section).
struct VoiceParams {
    int   tuneSemis = 12;     // onboard-only pitch shift (+12 = an octave up)
    float decaySec  = 0.22f;  // body amplitude decay
    float transient = 0.45f;  // click amount relative to body (0 = pure tone)
    float dropSemis = 14.0f;  // attack pitch sweep (0 = static pitch; 14 ≈ prototype 2.2×)
};

class VoiceBank {
public:
    void prepare(double sampleRate) {
        sr_ = sampleRate;
        for (auto& v : voices_) v = Voice{};
        rng_.seed(0x00FA11BAC, 0xD00D);
        clickLen_ = (int)(0.017 * sr_);   // ≈800 smp @48k transient
        recompute();
    }

    void setParams(const VoiceParams& p) { params_ = p; recompute(); }

    // Trigger a voice for a MIDI note. Steals the quietest active voice.
    void noteOn(int note, int velocity) {
        int idx = 0;
        float lowest = 1e9f;
        for (int i = 0; i < kMaxVoices; ++i) {
            if (!voices_[i].active) { idx = i; break; }
            if (voices_[i].env < lowest) { lowest = voices_[i].env; idx = i; }
        }
        Voice& v = voices_[idx];
        const double freq = 440.0 * std::pow(2.0, (note + params_.tuneSemis - 69) / 12.0);
        v.active    = true;
        v.phase     = 0.0;
        v.targetInc = 2.0 * 3.14159265358979323846 * freq / sr_;
        v.phaseInc  = v.targetInc * dropMul_;   // start above target, glide down
        v.env       = static_cast<float>(velocity) / 127.0f;
        v.clickAmp  = params_.transient * v.env;
        v.clickPos  = 0;
        v.seed      = rng_.nextU32() | 1u;
    }

    // Mix all active voices into the buffer, adding in place.
    void render(float* const* out, int numCh, int numSamples, float gain) {
        for (int i = 0; i < kMaxVoices; ++i) {
            Voice& v = voices_[i];
            if (!v.active) continue;
            for (int s = 0; s < numSamples; ++s) {
                float smp = static_cast<float>(std::sin(v.phase)) * v.env;
                if (v.clickPos < clickLen_) {
                    // Cubic-faded noise transient (prototype: (1 - i/N)^3).
                    v.seed ^= v.seed << 13; v.seed ^= v.seed >> 17; v.seed ^= v.seed << 5;
                    const float noise = static_cast<int32_t>(v.seed) * (1.0f / 2147483648.0f);
                    const float fade = 1.0f - static_cast<float>(v.clickPos) / clickLen_;
                    smp += noise * fade * fade * fade * v.clickAmp;
                    ++v.clickPos;
                }
                smp *= gain;
                for (int ch = 0; ch < numCh; ++ch) out[ch][s] += smp;

                v.phase += v.phaseInc;
                if (v.phaseInc > v.targetInc)
                    v.phaseInc = std::max(v.targetInc, v.phaseInc * glide_);
                v.env *= decay_;
            }
            if (v.env < 1e-4f && v.clickPos >= clickLen_) v.active = false;
        }
    }

private:
    void recompute() {
        dropMul_ = std::pow(2.0, params_.dropSemis / 12.0);
        glide_   = dropMul_ > 1.0 ? std::pow(1.0 / dropMul_, 1.0 / (0.050 * sr_)) : 1.0;
        const double d = std::max(0.02f, params_.decaySec);
        decay_   = static_cast<float>(std::exp(std::log(0.0025) / (d * sr_)));
    }

    static constexpr int kMaxVoices = 32;
    struct Voice {
        bool     active    = false;
        double   phase     = 0.0;
        double   phaseInc  = 0.0;   // current (glides down to targetInc)
        double   targetInc = 0.0;
        float    env       = 0.0f;
        float    clickAmp  = 0.0f;
        int      clickPos  = 0;
        uint32_t seed      = 1u;
    };
    Voice       voices_[kMaxVoices];
    VoiceParams params_;
    double      sr_       = 48000.0;
    double      glide_    = 1.0;
    double      dropMul_  = 1.0;
    float       decay_    = 0.999f;
    int         clickLen_ = 800;
    Pcg32       rng_;
};

} // namespace orrery
