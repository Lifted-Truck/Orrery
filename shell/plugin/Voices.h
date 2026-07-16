// Voices.h — internal fallback drum voices (contract §4).
//
// The prototype's validated percussion voice (elastic-euclid-2.html trigger()),
// ported: a PITCHED, percussive hit — sine body whose frequency drops from
// 2.2× the target to the target over 50 ms (tom/membrane character) with a
// 220 ms exponential amplitude decay, plus a short cubic-faded noise CLICK
// (~800 samples at 0.625× body gain) as the attack transient only. No
// sustained noise. Pitch derives from the MIDI note (note map + offset-layer
// transposes stay meaningful). RT-safe: fixed voice array, no allocation, no
// wall-clock — per-voice xorshift noise for the click. Framework-free.
#pragma once

#include <cmath>
#include <cstdint>

#include "orrery/Pcg32.h"
#include "orrery/Types.h"

namespace orrery {

class VoiceBank {
public:
    void prepare(double sampleRate) {
        sr_ = sampleRate;
        for (auto& v : voices_) v = Voice{};
        rng_.seed(0x00FA11BAC, 0xD00D);
        // Prototype constants, converted to per-sample factors at this rate.
        glide_    = std::pow(1.0 / 2.2, 1.0 / (0.050 * sr_));   // 2.2× → 1× in 50 ms
        decay_    = (float)std::exp(std::log(0.0025) / (0.22 * sr_)); // →0.25% in 220 ms
        clickLen_ = (int)(0.017 * sr_);                          // ≈800 smp @48k
    }

    // Trigger a voice for a MIDI note. Steals the quietest active voice.
    void noteOn(int note, int velocity) {
        int idx = 0;
        float lowest = 1e9f;
        for (int i = 0; i < kMaxVoices; ++i) {
            if (!voices_[i].active) { idx = i; break; }
            if (voices_[i].env < lowest) { lowest = voices_[i].env; idx = i; }
        }
        Voice& v = voices_[idx];
        const double freq = 440.0 * std::pow(2.0, (note - 69) / 12.0);
        v.active    = true;
        v.phase     = 0.0;
        v.targetInc = 2.0 * 3.14159265358979323846 * freq / sr_;
        v.phaseInc  = v.targetInc * 2.2;      // start high, glide down (the "drop")
        v.env       = static_cast<float>(velocity) / 127.0f;
        v.clickAmp  = 0.625f * v.env;         // prototype: click 0.25 vs body 0.4
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
    Voice  voices_[kMaxVoices];
    double sr_       = 48000.0;
    double glide_    = 1.0;
    float  decay_    = 0.999f;
    int    clickLen_ = 800;
    Pcg32  rng_;
};

} // namespace orrery
