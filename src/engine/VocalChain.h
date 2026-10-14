#pragma once
// Real-time vocal processing chain (no JUCE dependency, no allocation while processing).
//
// Signal flow (zero latency, so bypass / A-B need no delay compensation):
//   input gain / auto-level -> noise reduction -> plosive control -> pitch stage (correction + harmonies)
//   -> subtractive EQ -> dynamic EQ -> FET -> opto -> resonance -> de-esser -> breath control -> additive EQ
//   -> sidechain unmasking -> 2x-oversampled saturation -> band-limit -> parallel comp + exciter -> harmonies
//   -> reverb/delay/doubler/chorus sends -> K-weighted level match -> output -> width -> true-peak limiter.
// Every stage is always in the signal path, so the total latency is CONSTANT (latency() samples, reported to the host).
// Bypass is a 10 ms crossfade to the input delayed by the same amount, so A/B is sample-aligned.
#include "VocalMixEngine.h"
#include "../dsp/Effects.h"
#include "../dsp/PitchProcessor.h"
#include "../dsp/NoiseReducer.h"
#include "../dsp/Restoration.h"
#include "../dsp/Limiter2.h"
#include "../dsp/Oversampler.h"
#include <atomic>
#include <vector>
#include <array>

namespace vonox::engine {

struct Meters {
    std::atomic<float> inPeakDb{-120.f}, inRmsDb{-120.f}, outPeakDb{-120.f}, outRmsDb{-120.f}, gainReductionDb{0.f};
    std::atomic<bool> overload{false};
    std::atomic<float> truePeakDb{-120.f}, nrReductionDb{0.f}, autoLevelDb{0.f}, unmaskDb{0.f}, voicedMidi{0.f};
    std::array<std::atomic<float>, 12> keyHist{};          // confidence-weighted pitch-class counts (C = 0)
};

// Voice features that are NOT scaled by MIX AMOUNT (they are tools, not a "style").
struct VoiceSettings {
    double inputGainDb = 0.0; bool autoLevel = false; double autoLevelDb = -20.0;
    double nr = 0.0, plosive = 0.0, breath = 0.0;
    dsp::PitchParams pitch;
    double dynEqHz = 3000.0, dynEqAmount = 0.0, dynEqThreshDb = -30.0;
    double unmask = 0.0;               // 0..1 sidechain unmasking depth (needs the aux input)
    bool adlib = false;                // ad-lib / backing role: more space and width, lighter low end
    double bandLimit = 0.0;            // 0..1 telephone / radio band-limit (450 Hz - 3.2 kHz)
};

struct ChainTarget {
    MixSettings base;          // AUTO MIX / preset values (full strength)
    double mix = 1.0;          // 0..1 MIX AMOUNT (see VocalMixEngine::scale)
    double compress = 0.5;     // 0..1 compression macro: 0.5 = as set, 0 = off, 1 = double (ratio-1)
    double outputDb = 0.0;     // -12..+12
    double space = 0.5;        // 0..1 macro on reverb + delay amounts: 0.5 = as set, 0 = dry, 1 = double
    double character = 0.5;    // 0..1 macro on exciter + saturation drive + parallel compression (same law)
    double bpm = 120.0;        // host tempo for the synchronised delay
    VoiceSettings voice;
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
        for (auto& c : ch) { c.res.prepare(sr); c.ess.prepare(sr); c.dyn.prepare(sr); c.sat.prepare(sr * 2.0); }
        fet.prepare(sr); opto.prepare(sr); limiter.prepare(sr); parComp.prepare(sr);
        parComp.set(-32.0, 8.0, 3.0, 90.0, 3.0, 0.0);
        for (auto& e : exc) e.prepare(sr);
        rev.prepare(sr); dly.prepare(sr); chor.prepare(sr); dbl.prepare(sr); wid.prepare(sr);
        nr.prepare(sr); plosive.prepare(sr); pitch.prepare(sr); breath.prepare(sr); autoLvl.prepare(sr);
        duckAtk = 1.0 - std::exp(-1.0 / (sr * 0.005)); duckRel = 1.0 - std::exp(-1.0 / (sr * 0.150));
        umA = 1.0 - std::exp(-1.0 / (sr * 0.020)); umS = 1.0 - std::exp(-1.0 / (sr * 0.25));
        kwX.ready = kwD.ready = false; auxInit = false;
        total = nr.latency() + pitch.latency() + dsp::Oversampler2x::kLatency + limiter.latency();
        for (auto& b : dryBuf) b.assign(size_t(total + 1), 0.0);
        reset();
        snapToTarget = true;
    }

    // Total constant latency in samples (what the plugin reports to the host).
    int latency() const { return total; }

    void reset() {
        for (auto& c : ch) {
            c.hpf.reset(); c.lowMid.reset(); c.box.reset(); c.harsh.reset(); c.warm.reset(); c.pres.reset(); c.air.reset();
            c.res.reset(); c.ess.reset(); c.dyn.reset(); c.sat.reset(); c.umBell.reset(); c.umLow.reset();
            c.blHp.reset(); c.blLp.reset(); c.blHp2.reset(); c.blLp2.reset();
        }
        for (auto& o : os) o.reset();
        fet.reset(); opto.reset(); limiter.reset(); parComp.reset();
        for (auto& e : exc) e.reset();
        rev.reset(); dly.reset(); chor.reset(); dbl.reset(); wid.reset(); duckEnv = 0.0;
        nr.reset(); plosive.reset(); pitch.reset(); breath.reset(); autoLvl.reset();
        for (auto& b : dryBuf) std::fill(b.begin(), b.end(), 0.0);
        dryPos = 0; kwX.reset(); kwD.reset(); for (auto& t : tpm) t.reset();
        auxHp.reset(); auxLp.reset(); vocHp.reset(); vocLp.reset(); auxEnv = vocEnv = umDb = 0.0; histCounter = 0;
        bypassRamp = tgtBypass ? 1.0 : 0.0; lmGain = 1.0; eDry = eWet = 1e-6; ctlCounter = kControlBlock;
    }

    void clearKeyHistogram() { for (auto& h : meters.keyHist) h.store(0.f); }

    // Called once per block from the audio thread (plain values, no allocation).
    void setTarget(const ChainTarget& t) {
        MixSettings base = t.base;
        if (t.voice.adlib) {                 // ad-lib / backing role: wider, wetter, lighter low end
            base.reverbAmount = std::min(1.0, base.reverbAmount * 1.5 + 0.04); base.delayAmount = std::min(1.0, base.delayAmount * 1.3 + 0.03);
            base.width = std::min(2.0, base.width * 1.25); base.hpfHz = std::max(base.hpfHz, 140.0); base.doublerAmount = std::min(1.0, base.doublerAmount + 0.1);
            base.duck = std::min(1.0, base.duck + 0.2);
        }
        MixSettings s = VocalMixEngine::scale(base, t.mix, -3.0);
        const double ks = std::clamp(t.space, 0.0, 1.0) * 2.0, kc = std::clamp(t.character, 0.0, 1.0) * 2.0;
        s.reverbAmount = std::clamp(s.reverbAmount * ks, 0.0, 1.0); s.delayAmount = std::clamp(s.delayAmount * ks, 0.0, 1.0);
        s.exciterAmount = std::clamp(s.exciterAmount * kc, 0.0, 1.0); s.parCompAmount = std::clamp(s.parCompAmount * kc, 0.0, 1.0);
        s.satDriveDb = std::clamp(s.satDriveDb * kc, 0.0, 18.0);
        bpm = std::clamp(t.bpm, 30.0, 300.0);
        const double k = std::clamp(t.compress, 0.0, 1.0) * 2.0;
        s.fetRatio = std::clamp(1.0 + (s.fetRatio - 1.0) * k, 1.0, 20.0);
        s.optoRatio = std::clamp(1.0 + (s.optoRatio - 1.0) * k, 1.0, 20.0);
        tgt = s; tgtOutDb = t.outputDb; tgtBypass = t.bypass; tgtLevelMatch = t.levelMatch;
        voice = t.voice;
        nr.set(voice.nr); plosive.set(voice.plosive); breath.set(voice.breath); autoLvl.set(voice.autoLevel, voice.autoLevelDb); pitch.set(voice.pitch);
        inGain = dbToLin(voice.inputGainDb);
        if (snapToTarget) { cur = tgt; curOutDb = tgtOutDb; applyControl(); snapToTarget = false; }
    }

    // Process in place. channels: 1 or 2 used; extra channels are left untouched.
    // aux (optional): sidechain input used only by the unmasking stage (1 or 2 channels), same length n.
    void process(float* const* io, int numCh, int n, const float* const* aux = nullptr, int auxCh = 0) {
        const int nc = std::min(numCh, kMaxChannels);
        if (nc <= 0 || n <= 0) return;
        const bool useAux = aux != nullptr && auxCh > 0 && voice.unmask > 0.001;

        double inPk = 0, inE = 0, outPk = 0, outE = 0, tpk = 0;
        for (int i = 0; i < n; ++i) {
            if (ctlCounter >= kControlBlock) { smoothControl(); applyControl(); updateUnmask(); ctlCounter = 0; }
            ++ctlCounter;

            double x[kMaxChannels] = {0, 0}, d[kMaxChannels] = {0, 0}, dd[kMaxChannels] = {0, 0};
            for (int c = 0; c < nc; ++c) {
                d[c] = io[c][i]; inPk = std::max(inPk, std::abs(d[c])); inE += d[c] * d[c];
                auto& b = dryBuf[c]; b[size_t(dryPos)] = d[c]; dd[c] = b[size_t((dryPos + 1) % int(b.size()))];   // d delayed by `total`
                x[c] = d[c] * inGain;
            }
            dryPos = (dryPos + 1) % int(dryBuf[0].size());
            if (nc == 1) x[1] = x[0];

            // 0. input: auto-level, noise reduction, plosive control (stereo-linked stages work on the pair)
            if (voice.autoLevel || autoLvl.gainDb() != 0.0) autoLvl.process(x[0], x[1]);
            { double a, b; nr.process(x[0], x[1], a, b); x[0] = a; x[1] = b; }
            if (voice.plosive > 0.001) plosive.process(x[0], x[1]);
            // 1. pitch stage: lead (corrected or plain delayed copy) + wet-only harmonies / octaves
            double hL = 0, hR = 0;
            { double a, b; pitch.process(x[0], x[1], a, b, hL, hR); x[0] = a; x[1] = b; }
            if (nc == 1) x[1] = x[0];
            if ((++histCounter & 127) == 0 && pitch.voicedNow()) {
                const int pc = ((int(std::lround(pitch.detectedMidi())) % 12) + 12) % 12;
                meters.keyHist[size_t(pc)].store(meters.keyHist[size_t(pc)].load(std::memory_order_relaxed) + 1.f, std::memory_order_relaxed);
            }
            const bool voiced = pitch.voicedNow();

            // 2. subtractive EQ, dynamic EQ band
            for (int c = 0; c < nc; ++c) {
                auto& s = ch[c];
                x[c] = s.harsh.process(s.box.process(s.lowMid.process(s.hpf.process(x[c]))));
                if (voice.dynEqAmount > 0.005) x[c] = s.dyn.process(x[c]);
            }
            // 3. FET peak compression + opto leveler (linked detectors)
            double det = std::abs(x[0]); if (nc > 1) det = std::max(det, std::abs(x[1]));
            const double gF = fet.gainFor(det);
            for (int c = 0; c < nc; ++c) x[c] *= gF;
            det = std::abs(x[0]); if (nc > 1) det = std::max(det, std::abs(x[1]));
            const double gO = opto.gainFor(det);
            for (int c = 0; c < nc; ++c) x[c] *= gO;
            // 4. resonance, de-esser
            for (int c = 0; c < nc; ++c) {
                auto& s = ch[c];
                if (cur.resAmount > 0.005) x[c] = s.res.process(x[c]);
                if (cur.deEssAmount > 0.005) x[c] = s.ess.process(x[c]);
            }
            if (voice.breath > 0.001) { breath.process(x[0], x[1], voiced); if (nc == 1) x[1] = x[0]; }
            // 5. additive EQ, sidechain unmasking, band-limit, 2x-oversampled saturation
            if (useAux) feedUnmask(aux, auxCh, i, x[0], nc > 1 ? x[1] : x[0]); else if (umDb != 0.0 || auxEnv != 0.0) decayUnmask();
            for (int c = 0; c < nc; ++c) {
                auto& s = ch[c];
                x[c] = s.air.process(s.pres.process(s.warm.process(x[c])));
                if (std::abs(umDb) > 0.02) x[c] = s.umLow.process(s.umBell.process(x[c]));
                if (voice.bandLimit > 0.001) { const double f = s.blLp2.process(s.blLp.process(s.blHp2.process(s.blHp.process(x[c])))); x[c] += voice.bandLimit * (f - x[c]); }
                if (cur.satDriveDb > 0.05) x[c] = os[c].process(x[c], [&s](double v) { return s.sat.process(v); });
                else x[c] = os[c].process(x[c], [](double v) { return v; });      // identity, keeps the latency constant
            }
            // parallel compression (heavy compressed copy, linked detector) + harmonic exciter
            if (cur.parCompAmount > 0.005) {
                double dp = std::abs(x[0]); if (nc > 1) dp = std::max(dp, std::abs(x[1]));
                const double gp = parComp.gainFor(dp);
                for (int c = 0; c < nc; ++c) x[c] += cur.parCompAmount * 1.5 * x[c] * gp;
            }
            if (cur.exciterAmount > 0.005) for (int c = 0; c < nc; ++c) x[c] = exc[c].process(x[c]);
            // harmonies / octave voices (already pitch-shifted, wet only): joined before the sends
            x[0] += hL; if (nc > 1) x[1] += hR; else x[0] += 0.0;

            // dedicated wet paths: reverb / delay (ducked under the vocal), doubler / chorus (body of the vocal)
            {
                const double mono = (nc > 1) ? 0.5 * (x[0] + x[1]) : x[0];
                duckEnv += ((std::abs(mono) > duckEnv) ? duckAtk : duckRel) * (std::abs(mono) - duckEnv);
                const double duckGain = 1.0 - cur.duck * std::min(1.0, duckEnv / 0.12) * 0.75;
                double wl = 0, wr = 0, a, b;
                if (cur.reverbAmount > 0.003) { rev.process(mono, mono, a, b); wl += a * cur.reverbAmount * 5.0 * duckGain; wr += b * cur.reverbAmount * 5.0 * duckGain; }
                if (cur.delayAmount > 0.003) { dly.process(mono, mono, a, b); wl += a * cur.delayAmount * duckGain; wr += b * cur.delayAmount * duckGain; }
                if (cur.doublerAmount > 0.003) { dbl.process(mono, a, b); wl += a * cur.doublerAmount * 0.6; wr += b * cur.doublerAmount * 0.6; }
                if (cur.chorusAmount > 0.003) { chor.process(x[0], nc > 1 ? x[1] : x[0], a, b); wl += a * cur.chorusAmount * 0.6; wr += b * cur.chorusAmount * 0.6; }
                if (nc > 1) { x[0] += wl; x[1] += wr; } else x[0] += 0.5 * (wl + wr);
            }

            // loudness match (processed -> dry): K-weighted (BS.1770-style filters, not a certified meter), slow RMS
            const double kx = kwX.process(nc > 1 ? 0.5 * (x[0] + x[1]) : x[0]), kd = kwD.process(nc > 1 ? 0.5 * (dd[0] + dd[1]) : dd[0]);
            eWet += rmsAlpha * (kx * kx - eWet); eDry += rmsAlpha * (kd * kd - eDry);
            const double lmTarget = tgtLevelMatch ? std::clamp(std::sqrt((eDry + 1e-9) / (eWet + 1e-9)), 0.25, 4.0) : 1.0;
            lmGain += rampAlpha * 0.02 * (lmTarget - lmGain);
            const double outLin = dbToLin(curOutDb) * lmGain;
            for (int c = 0; c < nc; ++c) x[c] *= outLin;

            if (nc > 1 && std::abs(cur.width - 1.0) > 0.005) wid.process(x[0], x[1]);
            // 6. look-ahead true-peak limiter (linked)
            { double a = x[0], b = (nc > 1) ? x[1] : x[0]; limiter.process(a, b); x[0] = a; if (nc > 1) x[1] = b; }

            // bypass crossfade (10 ms) to the delayed input
            const double tb = tgtBypass ? 1.0 : 0.0;
            if (bypassRamp < tb) bypassRamp = std::min(tb, bypassRamp + rampStep);
            else if (bypassRamp > tb) bypassRamp = std::max(tb, bypassRamp - rampStep);
            for (int c = 0; c < nc; ++c) {
                const double y = (bypassRamp >= 1.0) ? dd[c] : (bypassRamp <= 0.0 ? x[c] : dd[c] * bypassRamp + x[c] * (1.0 - bypassRamp));
                io[c][i] = static_cast<float>(y);
                outPk = std::max(outPk, std::abs(y)); outE += y * y;
                tpk = std::max(tpk, tpm[c].push(y));
            }
        }
        const double inN = double(n) * nc;
        meters.inPeakDb.store(float(linToDb(inPk))); meters.outPeakDb.store(float(linToDb(outPk)));
        meters.inRmsDb.store(float(linToDb(std::sqrt(inE / inN)))); meters.outRmsDb.store(float(linToDb(std::sqrt(outE / inN))));
        meters.truePeakDb.store(float(linToDb(tpk)));
        meters.gainReductionDb.store(float(fet.gainReductionDb() + opto.gainReductionDb() + ch[0].ess.gainReductionDb()));
        meters.nrReductionDb.store(float(nr.gainReductionDb())); meters.autoLevelDb.store(float(autoLvl.gainDb())); meters.unmaskDb.store(float(umDb));
        meters.voicedMidi.store(float(pitch.detectedMidi()));
        if (tpk >= 1.0) meters.overload.store(true, std::memory_order_relaxed);
    }

    Meters meters;

private:
    struct Channel {
        dsp::Biquad hpf, lowMid, box, harsh, warm, pres, air, umBell, umLow, blHp, blHp2, blLp, blLp2;
        dsp::DeEsser res, ess, dyn;
        dsp::Saturator sat;
    };

    // K-weighting approximation (ITU-R BS.1770 style): high-shelf +4 dB @ ~1.7 kHz and a 38 Hz high-pass.
    struct KWeight {
        dsp::Biquad shelf, hp; bool ready = false; double sr = 0;
        void init(double s) { sr = s; shelf.set(dsp::Biquad::Type::HighShelf, s, 1681.0, 0.7071, 4.0); hp.set(dsp::Biquad::Type::HighPass, s, 38.0, 0.5); ready = true; }
        void reset() { shelf.reset(); hp.reset(); }
        double process(double v) { return hp.process(shelf.process(v)); }
    };

    // Sidechain unmasking: where the aux track has energy in the vocal's presence band (1.2-4.5 kHz) and masks the
    // vocal, the vocal gets a dynamic presence lift (+) and a low-mid trim; both follow the masking amount and release slowly.
    void feedUnmask(const float* const* aux, int auxCh, int i, double l, double r) {
        const double a = (auxCh > 1) ? 0.5 * (double(aux[0][i]) + double(aux[1][i])) : double(aux[0][i]);
        const double v = 0.5 * (l + r);
        const double ab = auxLp.process(auxHp.process(a)), vb = vocLp.process(vocHp.process(v));
        auxEnv += umA * (ab * ab - auxEnv); vocEnv += umA * (vb * vb - vocEnv);
        const double diffDb = 10.0 * std::log10((auxEnv + 1e-12) / (vocEnv + 1e-12));
        const bool active = vocEnv > 1e-8 && auxEnv > 1e-9;
        const double mask = active ? std::clamp((diffDb + 6.0) / 12.0, 0.0, 1.0) : 0.0;     // 0 at aux <= -6 dB under the vocal, 1 at +6 dB over it
        umTarget = mask * voice.unmask * 4.0;                                                // up to +4 dB presence lift
        umDb += umS * (umTarget - umDb);
    }
    void decayUnmask() { umTarget = 0.0; umDb += umS * (0.0 - umDb); auxEnv *= 0.999; if (std::abs(umDb) < 1e-4) umDb = 0.0; }
    void updateUnmask() {
        if (!(std::abs(umDb) > 0.02)) return;
        for (auto& s : ch) { s.umBell.set(dsp::Biquad::Type::Bell, sampleRate, 2800.0, 0.9, umDb); s.umLow.set(dsp::Biquad::Type::Bell, sampleRate, 320.0, 1.0, -0.5 * umDb); }
    }

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
        l(a.parCompAmount, b.parCompAmount); l(a.exciterAmount, b.exciterAmount);
        l(a.reverbAmount, b.reverbAmount); l(a.reverbDecay, b.reverbDecay); l(a.reverbPreMs, b.reverbPreMs); l(a.reverbDamp, b.reverbDamp);
        l(a.reverbHp, b.reverbHp); l(a.reverbLp, b.reverbLp);
        l(a.delayAmount, b.delayAmount); l(a.delayMs, b.delayMs); l(a.delayFeedback, b.delayFeedback); l(a.delayHp, b.delayHp); l(a.delayLp, b.delayLp);
        l(a.doublerAmount, b.doublerAmount); l(a.chorusAmount, b.chorusAmount); l(a.duck, b.duck); l(a.width, b.width);
        a.satMode = b.satMode; a.reverbMode = b.reverbMode; a.delayMode = b.delayMode; a.delayDiv = b.delayDiv;
    }

    void smoothControl() {
        lerp(cur, tgt, ctlAlpha);
        curOutDb += (tgtOutDb - curOutDb) * ctlAlpha;
    }

    void applyControl() {
        using T = dsp::Biquad::Type;
        if (!kwX.ready) { kwX.init(sampleRate); kwD.init(sampleRate); }
        if (!auxInit) {
            auxHp.set(T::HighPass, sampleRate, 1200.0, 0.7071); auxLp.set(T::LowPass, sampleRate, 4500.0, 0.7071);
            vocHp.set(T::HighPass, sampleRate, 1200.0, 0.7071); vocLp.set(T::LowPass, sampleRate, 4500.0, 0.7071); auxInit = true;
        }
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
            s.dyn.set(voice.dynEqHz, 1.0, voice.dynEqThreshDb, voice.dynEqAmount);
            s.sat.set(cur.satMode, cur.satDriveDb);
            s.blHp.set(T::HighPass, sampleRate, 450.0, 0.7071); s.blHp2.set(T::HighPass, sampleRate, 450.0, 0.7071);
            s.blLp.set(T::LowPass, sampleRate, 3200.0, 0.7071); s.blLp2.set(T::LowPass, sampleRate, 3200.0, 0.7071);
        }
        fet.set(cur.fetThreshDb, cur.fetRatio, cur.fetAttackMs, cur.fetReleaseMs, 6.0, 0.0);
        opto.set(cur.optoThreshDb, cur.optoRatio, cur.optoAttackMs, cur.optoReleaseMs, 10.0, 0.0);
        limiter.set(cur.ceilingDb, 80.0);
        for (auto& e : exc) e.set(cur.exciterAmount, 2800.0);
        {
            dsp::Reverb::Params rp; rp.mode = static_cast<dsp::Reverb::Mode>(std::clamp(cur.reverbMode, 0, 3));
            rp.decay = cur.reverbDecay; rp.preDelayMs = cur.reverbPreMs; rp.damping = cur.reverbDamp; rp.hpHz = cur.reverbHp; rp.lpHz = cur.reverbLp; rp.width = 1.0;
            rev.set(rp);
            static const double beats[7] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0 };
            dsp::StereoDelay::Params dp; dp.mode = static_cast<dsp::StereoDelay::Mode>(std::clamp(cur.delayMode, 0, 2));
            dp.timeMs = (cur.delayDiv >= 0 && cur.delayDiv < 7) ? 60000.0 / bpm * beats[cur.delayDiv] : cur.delayMs;
            if (dp.mode == dsp::StereoDelay::Mode::Slapback && cur.delayDiv == 7) dp.timeMs = cur.delayMs;
            dp.feedback = cur.delayFeedback; dp.hpHz = cur.delayHp; dp.lpHz = cur.delayLp; dp.width = 1.0;
            dly.set(dp);
            wid.set(cur.width);
        }
    }

    double sampleRate = 44100.0, ctlAlpha = 0.01, rampAlpha = 0.01, rampStep = 0.002, rmsAlpha = 1e-5;
    int ctlCounter = kControlBlock, total = 0, dryPos = 0, histCounter = 0;
    std::array<Channel, kMaxChannels> ch;
    dsp::Compressor fet, opto, parComp;
    std::array<dsp::Exciter, kMaxChannels> exc;
    dsp::Reverb rev; dsp::StereoDelay dly; dsp::Chorus chor; dsp::Doubler dbl; dsp::Width wid;
    double bpm = 120.0, duckEnv = 0.0, duckAtk = 0.01, duckRel = 0.0005;
    dsp::TruePeakLimiter limiter;
    dsp::NoiseReducer nr; dsp::PlosiveControl plosive; dsp::PitchProcessor pitch; dsp::BreathControl breath; dsp::AutoLevel autoLvl;
    std::array<dsp::Oversampler2x, kMaxChannels> os;
    std::array<dsp::TruePeakDetector, kMaxChannels> tpm;
    std::array<std::vector<double>, kMaxChannels> dryBuf;
    KWeight kwX, kwD;
    dsp::Biquad auxHp, auxLp, vocHp, vocLp; bool auxInit = false;
    double auxEnv = 0, vocEnv = 0, umDb = 0, umTarget = 0, umA = 0.01, umS = 0.0005, inGain = 1.0;
    VoiceSettings voice;
    MixSettings cur, tgt;
    double curOutDb = 0, tgtOutDb = 0, bypassRamp = 0, lmGain = 1.0, eDry = 1e-6, eWet = 1e-6;
    bool tgtBypass = false, tgtLevelMatch = false, snapToTarget = true;
};

} // namespace vonox::engine
