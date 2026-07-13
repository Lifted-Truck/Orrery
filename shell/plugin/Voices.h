// Voices.h — internal fallback drum voices (contract §4).
//
// A fixed-capacity percussive synth so the plugin is a self-contained
// instrument (MIDI out is the primary path; these give it sound standalone and
// satisfy instrument validation). RT-safe: fixed voice array, no allocation, no
// wall-clock — per-voice PCG noise for the transient. Framework-free.
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
    }

    // Trigger a percussive voice for a MIDI note. Steals the quietest voice.
    void noteOn(int note, int velocity) {
        int idx = 0;
        float lowest = 1e9f;
        for (int i = 0; i < kMaxVoices; ++i) {
            const float lvl = voices_[i].active ? voices_[i].env : -1.0f;
            if (!voices_[i].active) { idx = i; break; }
            if (lvl < lowest) { lowest = lvl; idx = i; }
        }
        Voice& v = voices_[idx];
        v.active   = true;
        v.phase    = 0.0;
        const double freq = 440.0 * std::pow(2.0, (note - 69) / 12.0);
        v.phaseInc = 2.0 * 3.14159265358979323846 * freq / sr_;
        v.env      = static_cast<float>(velocity) / 127.0f;
        // Higher notes = shorter, brighter; lower = longer body.
        v.decay    = std::exp(-1.0 / (sr_ * (0.04 + 0.30 * std::exp(-(note - 36) / 18.0))));
        v.noiseMix = note >= 60 ? 0.5f : 0.15f;
        v.seed     = rng_.nextU32() | 1u;
    }

    // Mix all active voices into a stereo (or mono) buffer, adding in place.
    void render(float* const* out, int numCh, int numSamples, float gain) {
        for (int i = 0; i < kMaxVoices; ++i) {
            Voice& v = voices_[i];
            if (!v.active) continue;
            for (int s = 0; s < numSamples; ++s) {
                // Cheap xorshift noise from the per-voice seed (deterministic).
                v.seed ^= v.seed << 13; v.seed ^= v.seed >> 17; v.seed ^= v.seed << 5;
                const float noise = (static_cast<int32_t>(v.seed) * (1.0f / 2147483648.0f));
                const float tone  = static_cast<float>(std::sin(v.phase));
                const float smp   = ((1.0f - v.noiseMix) * tone + v.noiseMix * noise) * v.env * gain;
                for (int ch = 0; ch < numCh; ++ch) out[ch][s] += smp;
                v.phase += v.phaseInc;
                v.env   *= v.decay;
            }
            if (v.env < 1e-4f) v.active = false;
        }
    }

private:
    static constexpr int kMaxVoices = 32;
    struct Voice {
        bool     active   = false;
        double   phase    = 0.0;
        double   phaseInc = 0.0;
        float    env      = 0.0f;
        float    decay    = 0.0f;
        float    noiseMix = 0.0f;
        uint32_t seed     = 1u;
    };
    Voice  voices_[kMaxVoices];
    double sr_ = 48000.0;
    Pcg32  rng_;
};

} // namespace orrery
