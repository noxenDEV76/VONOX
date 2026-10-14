#pragma once
// Pitch stage: tracker + PSOLA engine + correction/harmony control.
//   lead  : automatic pitch correction (key/scale quantiser, retune speed, humanize, amount)
//   extras: two scale-aware harmony voices and an octave generator (down/up), wet only
// The lead path ALWAYS goes through the engine's fixed delay (or its direct delayed copy when no
// correction is active), so the stage has a constant latency and bypass / A-B stay aligned.
#include "PitchTracker.h"
#include "Psola.h"
#include "Biquad.h"
#include "../engine/Scale.h"
namespace vonox::dsp {

struct PitchParams {
    double amount = 0.0;          // 0 = correction off
    double retuneMs = 40.0;       // 0 = hard (robotic), 200+ = natural
    double humanize = 0.3;        // 0..1 amount of natural pitch movement kept
    int key = 0; int scale = 0;   // 0..11, ScaleType index (0 = chromatic)
    int harm1 = 0, harm2 = 0;     // 0 off, 1 3rd up, 2 3rd down, 3 5th up, 4 5th down, 5 6th up, 6 octave up, 7 octave down
    double harm1Gain = 0.0, harm2Gain = 0.0, octDown = 0.0, octUp = 0.0;   // linear 0..1
};

class PitchProcessor {
public:
    void prepare(double sr) {
        sampleRate = sr; trk.prepare(sr); eng.prepare(sr);
        ctlA = 1.0 - std::exp(-1.0 / (sr * 0.040)); xfA = 1.0 / (sr * 0.012);
        reset();
    }
    void reset() { trk.reset(); eng.reset(); subPh = 0; subF = 150.0; subLp[0].reset(); subLp[1].reset(); subCut = 0; cents = goalCents = 0; wet = 0; slow = 0; haveSlow = false; prevNote = -1;
        for (auto& r : hr) r = 1.0; for (auto& g : hg) g = 0.0; voiced = false; f0 = 0; lastMidi = 0; }
    int latency() const { return eng.latency(); }
    void set(const PitchParams& np) { p = np; }
    bool voicedNow() const { return voiced; }
    double f0Hz() const { return f0; }
    double detectedMidi() const { return voiced ? lastMidi : 0.0; }

    // returns lead (delayed/corrected), harm (wet-only extras) for one stereo sample
    inline void process(double l, double r, double& leadL, double& leadR, double& harmL, double& harmR) {
        if (trk.push(0.5 * (l + r))) analyse();
        // per-sample smoothing of the correction and of the harmony ratios/gains
        const double retA = (p.retuneMs <= 1.0) ? 1.0 : 1.0 - std::exp(-1.0 / (sampleRate * p.retuneMs * 1e-3));
        cents += ((voiced ? retA : ctlA) * (goalCents - cents));
        const double mainRatio = std::pow(2.0, cents / 1200.0);
        const bool leadOn = p.amount > 0.001 || std::abs(cents) > 0.2;
        eng.setVoice(0, true, mainRatio, 1.0);
        for (int v = 0; v < 4; ++v) {
            hr[v] += ctlA * 2.0 * (hrGoal[v] - hr[v]); hg[v] += ctlA * (hgGoal[v] - hg[v]);
            if (v == 2) continue;                                  // octave-down is a tracked sub-octave generator (below)
            eng.setVoice(v + 1, hg[v] > 1e-4 || hgGoal[v] > 1e-4, hr[v], hg[v], pl[v], pr[v]);
        }
        double eL, eR, dL, dR; eng.process(l, r, eL, eR, harmL, harmR, dL, dR);
        // Octave down: the (delayed) vocal is ring-modulated by a sine at f0/2 and low-passed below 0.8*f0, which keeps
        // only the f0/2 component (a pitch-following sub-octave; PSOLA cannot make a clean one, it re-creates the original period)
        subPh += 2.0 * 3.14159265358979323846 * (subF * 0.5) / sampleRate; if (subPh > 6.283185307179586) subPh -= 6.283185307179586;
        if (hg[2] > 1e-4 || hgGoal[2] > 1e-4) {
            if (std::abs(subCut - 0.8 * subF) > 1.0) { subCut = 0.8 * subF; subLp[0].set(Biquad::Type::LowPass, sampleRate, subCut, 0.7071); subLp[1].set(Biquad::Type::LowPass, sampleRate, subCut, 0.7071); }
            const double o = 2.0 * subLp[1].process(subLp[0].process(0.5 * (dL + dR) * std::sin(subPh))) * hg[2] * 0.9; harmL += o; harmR += o;
        }
        const double tw = leadOn ? 1.0 : 0.0;
        wet += std::clamp(tw - wet, -xfA, xfA);
        leadL = dL + (eL - dL) * wet; leadR = dR + (eR - dR) * wet;
    }
private:
    void analyse() {
        voiced = trk.isVoiced(); f0 = trk.frequency(); if (voiced) subF = f0;
        eng.setPeriodFromPitch(voiced ? f0 : 0.0);
        const bool wantHarm = p.harm1Gain > 0 || p.harm2Gain > 0 || p.octDown > 0 || p.octUp > 0;
        if (!voiced) { goalCents = 0; for (int v = 0; v < 4; ++v) hgGoal[v] = 0; haveSlow = false; return; }
        const double midi = 69.0 + 12.0 * std::log2(f0 / 440.0); lastMidi = midi;
        if (!haveSlow) { slow = midi; haveSlow = true; prevNote = -1; }
        const double hopSec = 0.008, a = 1.0 - std::exp(-hopSec / 0.15); slow += a * (midi - slow);
        const auto mask = engine::scaleMask(p.key, static_cast<engine::ScaleType>(std::clamp(p.scale, 0, engine::kNumScales - 1)));
        const double deciding = midi + p.humanize * (slow - midi);               // humanize: decide from the smoothed pitch
        const int note = engine::quantizeNote(deciding, mask, prevNote); prevNote = note;
        const double desired = note + p.humanize * (midi - slow);                 // keep part of the natural movement
        goalCents = (desired - midi) * 100.0 * std::clamp(p.amount, 0.0, 1.0);
        if (wantHarm) {
            const auto hm = (p.scale == 0) ? engine::scaleMask(p.key, engine::ScaleType::Major) : mask;
            auto voiceFor = [&](int mode, int slot, double gain) {
                static const int steps[8] = { 0, 2, -2, 4, -4, 5, 0, 0 };
                if (mode == 0 || gain <= 0) { hgGoal[slot] = 0; return; }
                double target;
                if (mode == 6) target = midi + 12.0; else if (mode == 7) target = midi - 12.0;
                else target = engine::stepInScale(engine::quantizeNote(deciding, hm), steps[mode], hm) + p.humanize * (midi - slow);
                hrGoal[slot] = std::pow(2.0, (target - midi) / 12.0); hgGoal[slot] = gain; pl[slot] = slot == 0 ? 1.0 : 0.45; pr[slot] = slot == 0 ? 0.45 : 1.0;
            };
            voiceFor(p.harm1, 0, p.harm1Gain); voiceFor(p.harm2, 1, p.harm2Gain);
            hgGoal[2] = p.octDown; hrGoal[2] = 0.5; pl[2] = pr[2] = 0.7;
            hgGoal[3] = p.octUp; hrGoal[3] = 2.0; pl[3] = pr[3] = 0.7;
        } else for (int v = 0; v < 4; ++v) hgGoal[v] = 0;
    }
    double sampleRate = 48000, ctlA = 0.001, xfA = 0.002, cents = 0, goalCents = 0, wet = 0, slow = 0, f0 = 0, lastMidi = 0;
    bool haveSlow = false, voiced = false; int prevNote = -1;
    double hr[4] = { 1, 1, 0.5, 2 }, hg[4] = {}, hrGoal[4] = { 1, 1, 0.5, 2 }, hgGoal[4] = {}, pl[4] = { 1, 0.45, 0.7, 0.7 }, pr[4] = { 0.45, 1, 0.7, 0.7 };
    PitchTracker trk; PsolaEngine eng; PitchParams p; double subPh = 0, subF = 150.0, subCut = 0; Biquad subLp[2];
};
} // namespace vonox::dsp
