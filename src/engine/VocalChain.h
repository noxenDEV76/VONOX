#pragma once
// Real-time vocal processing chain (no JUCE dependency, no allocation while processing).
//
// Signal flow (zero latency, so bypass / A-B need no delay compensation):
//   subtractive EQ -> FET peak compressor -> opto leveler -> dynamic resonance control
//   -> de-esser -> additive EQ (warmth / presence / air) -> saturation -> output trim
//   -> stereo-linked peak limiter.
// Bypass is a 10 ms crossfade to the untouched input. LEVEL MATCH gain-matches the
// processed path to the dry path (slow RMS) so loudness does not bias A/B listening.
#include "VocalMixEngine.h"
#include <atomic>
#include <vector>
#include <array>

namespace vonox::engine {

struct Meters {
    std::atomic<float> inPeakDb{-120.f}, inRmsDb{-120.f}, outPeakDb{-120.f}, outRmsDb{-120.f}, gainReductionDb{0.f};
    std::atomic<bool> overload{false};
};

struct ChainTarget {
    MixSettings base;          // AUTO MIX / preset values (full strength)
    double mix = 1.0;          // 0..1 MIX AMOUNT (see VocalMixEngine::scale)
    double compress = 0.5;     // 0..1 compression macro: 0.5 = as set, 0 = off, 1 = double (ratio-1)
    double outputDb = 0.0;     // -12..+12
    bool bypass = false;
    bool levelMatch = false;
};

class VocalChain {
public:
    static constexpr int kControlBlock = 32;
    static constexpr int kMaxChannels = 2;

    void prepare(double sr, int maxBlock) {
        sampleRate = sr;
        (void) maxBlock;   // processing works on any block length; nothing is sized by it
        ctlAlpha = 1.0 - std::exp(-double(kControlBlock) / (sr * 0.040));
        rampAlpha = 1.0 - std::exp(-1.0 / (sr * 0.010));
        rampStep = 1.0 / (sr * 0.010);
        rmsAlpha = 1.0 - std::exp(-1.0 / (sr * 1.5));
        for (auto& c : ch) { c.res.prepare(sr); c.ess.prepare(sr); c.sat.prepare(sr); }
        fet.prepare(sr); opto.prepare(sr); limiter.prepare(sr);
        reset();
        snapToTarget = true;
    }

    void reset() {
        for (auto& c : ch) {
            c.hpf.reset(); c.lowMid.reset(); c.box.reset(); c.harsh.reset(); c.warm.reset(); c.pres.reset(); c.air.reset();
            c.res.reset(); c.ess.reset(); c.sat.reset();
        }
        fet.reset(); opto.reset(); limiter.reset();
        bypassRamp = tgtBypass ? 1.0 : 0.0; lmGain = 1.0; eDry = eWet = 1e-6; ctlCounter = kControlBlock;
    }

    // Called once per block from the audio thread (plain values, no allocation).
    void setTarget(const ChainTarget& t) {
        MixSettings s = VocalMixEngine::scale(t.base, t.mix, -3.0);
        const double k = std::clamp(t.compress, 0.0, 1.0) * 2.0;
        s.fetRatio = std::clamp(1.0 + (s.fetRatio - 1.0) * k, 1.0, 20.0);
        s.optoRatio = std::clamp(1.0 + (s.optoRatio - 1.0) * k, 1.0, 20.0);
        tgt = s; tgtOutDb = t.outputDb; tgtBypass = t.bypass; tgtLevelMatch = t.levelMatch;
        if (snapToTarget) { cur = tgt; curOutDb = tgtOutDb; applyControl(); snapToTarget = false; }
    }

    // Process in place. channels: 1 or 2 used; extra channels are left untouched.
    void process(float* const* io, int numCh, int n) {
        const int nc = std::min(numCh, kMaxChannels);
        if (nc <= 0 || n <= 0) return;

        double inPk = 0, inE = 0, outPk = 0, outE = 0;
        for (int i = 0; i < n; ++i) {
            if (ctlCounter >= kControlBlock) { smoothControl(); applyControl(); ctlCounter = 0; }
            ++ctlCounter;

            double x[kMaxChannels] = {0, 0}, d[kMaxChannels] = {0, 0};
            for (int c = 0; c < nc; ++c) { d[c] = io[c][i]; x[c] = d[c]; inPk = std::max(inPk, std::abs(d[c])); inE += d[c] * d[c]; }

            // 1. subtractive EQ
            for (int c = 0; c < nc; ++c) {
                auto& s = ch[c];
                x[c] = s.harsh.process(s.box.process(s.lowMid.process(s.hpf.process(x[c]))));
            }
            // 2. FET peak compression + 3. opto leveler (linked detectors)
            double det = std::abs(x[0]); if (nc > 1) det = std::max(det, std::abs(x[1]));
            const double gF = fet.gainFor(det);
            for (int c = 0; c < nc; ++c) x[c] *= gF;
            det = std::abs(x[0]); if (nc > 1) det = std::max(det, std::abs(x[1]));
            const double gO = opto.gainFor(det);
            for (int c = 0; c < nc; ++c) x[c] *= gO;
            // 4. dynamic resonance control, 5. de-esser, 6. additive EQ, 7. saturation
            for (int c = 0; c < nc; ++c) {
                auto& s = ch[c];
                if (cur.resAmount > 0.005) x[c] = s.res.process(x[c]);
                if (cur.deEssAmount > 0.005) x[c] = s.ess.process(x[c]);
                x[c] = s.air.process(s.pres.process(s.warm.process(x[c])));
                if (cur.satDriveDb > 0.05) x[c] = s.sat.process(x[c]);
            }
            // loudness match (processed -> dry), slow RMS so it never pumps
            double ew = 0, ed = 0;
            for (int c = 0; c < nc; ++c) { ew += x[c] * x[c]; ed += d[c] * d[c]; }
            eWet += rmsAlpha * (ew - eWet); eDry += rmsAlpha * (ed - eDry);
            const double lmTarget = tgtLevelMatch ? std::clamp(std::sqrt((eDry + 1e-9) / (eWet + 1e-9)), 0.25, 4.0) : 1.0;
            lmGain += rampAlpha * 0.02 * (lmTarget - lmGain);
            const double outLin = dbToLin(curOutDb) * lmGain;
            for (int c = 0; c < nc; ++c) x[c] *= outLin;

            // 8. peak limiter (linked)
            if (nc > 1) limiter.processLinked(x[0], x[1]); else x[0] = limiter.process(x[0]);

            // bypass crossfade (10 ms) to the untouched input
            const double tb = tgtBypass ? 1.0 : 0.0;                       // linear 10 ms crossfade, lands exactly
            if (bypassRamp < tb) bypassRamp = std::min(tb, bypassRamp + rampStep);
            else if (bypassRamp > tb) bypassRamp = std::max(tb, bypassRamp - rampStep);
            for (int c = 0; c < nc; ++c) {
                const double y = (bypassRamp >= 1.0) ? d[c] : (bypassRamp <= 0.0 ? x[c] : d[c] * bypassRamp + x[c] * (1.0 - bypassRamp));
                io[c][i] = static_cast<float>(y);
                outPk = std::max(outPk, std::abs(y)); outE += y * y;
            }
        }
        const double inN = double(n) * nc;
        meters.inPeakDb.store(float(linToDb(inPk))); meters.outPeakDb.store(float(linToDb(outPk)));
        meters.inRmsDb.store(float(linToDb(std::sqrt(inE / inN)))); meters.outRmsDb.store(float(linToDb(std::sqrt(outE / inN))));
        meters.gainReductionDb.store(float(fet.gainReductionDb() + opto.gainReductionDb() + ch[0].ess.gainReductionDb()));
        if (outPk >= 0.9999) meters.overload.store(true, std::memory_order_relaxed);
    }

    Meters meters;

private:
    struct Channel {
        dsp::Biquad hpf, lowMid, box, harsh, warm, pres, air;
        dsp::DeEsser res, ess;
        dsp::Saturator sat;
    };

    static void lerp(MixSettings& a, const MixSettings& b, double t) {
        auto l = [t](double& x, double y) { x += (y - x) * t; };
        l(a.hpfHz, b.hpfHz); l(a.lowMidCutDb, b.lowMidCutDb); l(a.lowMidHz, b.lowMidHz); l(a.boxCutDb, b.boxCutDb);
        l(a.harshCutDb, b.harshCutDb); l(a.harshHz, b.harshHz);
        l(a.fetThreshDb, b.fetThreshDb); l(a.fetRatio, b.fetRatio); l(a.fetAttackMs, b.fetAttackMs); l(a.fetReleaseMs, b.fetReleaseMs);
        l(a.optoThreshDb, b.optoThreshDb); l(a.optoRatio, b.optoRatio); l(a.optoAttackMs, b.optoAttackMs); l(a.optoReleaseMs, b.optoReleaseMs);
        l(a.deEssHz, b.deEssHz); l(a.deEssThreshDb, b.deEssThreshDb); l(a.deEssAmount, b.deEssAmount);
        l(a.resAmount, b.resAmount); l(a.resThreshDb, b.resThreshDb);
        l(a.presenceDb, b.presenceDb); l(a.presenceHz, b.presenceHz); l(a.airDb, b.airDb); l(a.warmthDb, b.warmthDb);
        l(a.satDriveDb, b.satDriveDb); l(a.ceilingDb, b.ceilingDb); l(a.outputTrimDb, b.outputTrimDb);
        a.satMode = b.satMode;
    }

    void smoothControl() {
        lerp(cur, tgt, ctlAlpha);
        curOutDb += (tgtOutDb - curOutDb) * ctlAlpha;
    }

    void applyControl() {
        using T = dsp::Biquad::Type;
        for (auto& s : ch) {
            s.hpf.set(T::HighPass, sampleRate, cur.hpfHz, 0.7071);
            s.lowMid.set(T::Bell, sampleRate, cur.lowMidHz, 1.0, cur.lowMidCutDb);
            s.box.set(T::Bell, sampleRate, 500.0, 1.2, cur.boxCutDb);
            s.harsh.set(T::Bell, sampleRate, cur.harshHz, 1.4, cur.harshCutDb);
            s.warm.set(T::LowShelf, sampleRate, 180.0, 0.7071, cur.warmthDb);
            s.pres.set(T::Bell, sampleRate, cur.presenceHz, 0.9, cur.presenceDb);
            s.air.set(T::HighShelf, sampleRate, 10000.0, 0.7071, cur.airDb);
            s.res.set(cur.harshHz, 1.0, cur.resThreshDb, cur.resAmount);
            s.ess.set(cur.deEssHz, 1.2, cur.deEssThreshDb, cur.deEssAmount);
            s.sat.set(cur.satMode, cur.satDriveDb);
        }
        fet.set(cur.fetThreshDb, cur.fetRatio, cur.fetAttackMs, cur.fetReleaseMs, 6.0, 0.0);
        opto.set(cur.optoThreshDb, cur.optoRatio, cur.optoAttackMs, cur.optoReleaseMs, 10.0, 0.0);
        limiter.set(cur.ceilingDb, 80.0);
    }

    double sampleRate = 44100.0, ctlAlpha = 0.01, rampAlpha = 0.01, rampStep = 0.002, rmsAlpha = 1e-5;
    int ctlCounter = kControlBlock;
    std::array<Channel, kMaxChannels> ch;
    dsp::Compressor fet, opto;
    dsp::Limiter limiter;
    MixSettings cur, tgt;
    double curOutDb = 0, tgtOutDb = 0, bypassRamp = 0, lmGain = 1.0, eDry = 1e-6, eWet = 1e-6;
    bool tgtBypass = false, tgtLevelMatch = false, snapToTarget = true;
};

} // namespace vonox::engine
