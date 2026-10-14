#pragma once
// Voice clean-up modules: plosive control, breath control, auto-level. All zero latency, stereo-linked, allocation-free.
#include "Biquad.h"
#include "Dynamics.h"
namespace vonox::dsp {

// ---- Plosive control: a dynamic high-pass that engages only on low-frequency bursts (p / b pops).
// A burst = energy below ~150 Hz that is much larger than the energy above 400 Hz and above an absolute floor.
class PlosiveControl {
public:
    void prepare(double sr) {
        sampleRate = sr; for (int c = 0; c < 2; ++c) { lp[c].reset(); hp[c].reset(); }
        for (int c = 0; c < 2; ++c) { lp[c].set(Biquad::Type::LowPass, sr, 150.0, 0.7071); lp2[c].set(Biquad::Type::LowPass, sr, 150.0, 0.7071); hp[c].set(Biquad::Type::HighPass, sr, 400.0, 0.7071); ho[c].set(Biquad::Type::HighPass, sr, 150.0, 0.7071); ho2[c].set(Biquad::Type::HighPass, sr, 150.0, 0.7071); }
        atk = 1.0 - std::exp(-1.0 / (sr * 0.002)); rel = 1.0 - std::exp(-1.0 / (sr * 0.060)); envA = 1.0 - std::exp(-1.0 / (sr * 0.004)); reset();
    }
    void reset() { for (int c = 0; c < 2; ++c) { lp[c].reset(); lp2[c].reset(); hp[c].reset(); ho[c].reset(); ho2[c].reset(); } eLow = eHigh = g = 0; }
    void set(double amount) { amt = std::clamp(amount, 0.0, 1.0); }
    // returns the plosive-reduced stereo pair in place
    inline void process(double& l, double& r) {
        if (amt <= 0.001) { g += rel * (0 - g); return; }
        const double lowL = lp2[0].process(lp[0].process(l)), lowR = lp2[1].process(lp[1].process(r));
        const double hi = 0.5 * (std::abs(hp[0].process(l)) + std::abs(hp[1].process(r)));
        const double lowA = 0.5 * (std::abs(lowL) + std::abs(lowR));
        eLow += envA * (lowA - eLow); eHigh += envA * (hi - eHigh);
        const double lowDb = linToDb(eLow + 1e-9), ratioDb = lowDb - linToDb(eHigh + 1e-9);
        const double burst = (lowDb > -38.0 && ratioDb > 8.0) ? std::clamp((ratioDb - 8.0) / 6.0, 0.0, 1.0) : 0.0;
        g += ((burst > g) ? atk : rel) * (burst - g);
        const double k = g * amt;                                      // crossfade to a 4th-order 150 Hz high-pass while a burst is detected
        const double hl = ho2[0].process(ho[0].process(l)), hr = ho2[1].process(ho[1].process(r));
        l += k * (hl - l); r += k * (hr - r);
    }
    double activity() const { return g; }
private:
    double sampleRate = 48000, amt = 0, atk = 0.1, rel = 0.001, envA = 0.01, eLow = 0, eHigh = 0, g = 0; Biquad lp[2], lp2[2], hp[2], ho[2], ho2[2];
};

// ---- Breath control: attenuates low-level, unvoiced, noisy passages between phrases (breaths) without touching
// voiced speech/singing or loud sibilants. `voiced` comes from the pitch tracker.
class BreathControl {
public:
    void prepare(double sr) {
        sampleRate = sr; for (int c = 0; c < 2; ++c) hp[c].set(Biquad::Type::HighPass, sr, 800.0, 0.7071);
        eA = 1.0 - std::exp(-1.0 / (sr * 0.010)); refA = 1.0 - std::exp(-1.0 / (sr * 2.0)); atk = 1.0 - std::exp(-1.0 / (sr * 0.012)); rel = 1.0 - std::exp(-1.0 / (sr * 0.120)); reset();
    }
    void reset() { for (auto& h : hp) h.reset(); env = 0; refLevel = 0; gr = 1.0; unvoicedCount = 0; }
    void set(double amount) { amt = std::clamp(amount, 0.0, 1.0); }
    inline void process(double& l, double& r, bool voiced) {
        const double a = 0.5 * (std::abs(l) + std::abs(r));
        env += eA * (a - env);
        if (voiced) { refLevel += refA * 4.0 * (env - refLevel); unvoicedCount = 0; } else if (unvoicedCount < 1000000) ++unvoicedCount;
        const double lvlDb = linToDb(env + 1e-9), refDb = linToDb(refLevel + 1e-9);
        // breath: unvoiced for a while, well below the typical voiced level, but not silence
        const bool breath = !voiced && unvoicedCount > int(sampleRate * 0.02) && refLevel > 1e-4 && lvlDb < refDb - 14.0 && lvlDb > refDb - 45.0;
        const double target = breath ? std::pow(10.0, -amt * 14.0 / 20.0) : 1.0;
        gr += ((target < gr) ? atk : rel) * (target - gr);
        l *= gr; r *= gr;
    }
    double gain() const { return gr; }
private:
    double sampleRate = 48000, amt = 0, env = 0, refLevel = 0, gr = 1, eA = 0.01, refA = 1e-5, atk = 0.01, rel = 0.001; int unvoicedCount = 0; Biquad hp[2];
};

// ---- Auto-level: slow RMS leveler toward a target, gated so silence / noise floor is never boosted.
class AutoLevel {
public:
    void prepare(double sr) { sampleRate = sr; eA = 1.0 - std::exp(-1.0 / (sr * 0.40)); up = 1.0 - std::exp(-1.0 / (sr * 1.5)); down = 1.0 - std::exp(-1.0 / (sr * 0.5)); reset(); }
    void reset() { e = 1e-6; gdb = 0.0; }
    void set(bool on, double targetDb) { enabled = on; target = targetDb; }
    inline double gainLin() const { return dbToLin(gdb); }
    inline void process(double& l, double& r) {
        const double p = 0.5 * (l * l + r * r); e += eA * (p - e);
        const double rmsDb = 10.0 * std::log10(e + 1e-12);
        double want = 0.0;
        if (enabled && rmsDb > -55.0) want = std::clamp(target - rmsDb, -12.0, 12.0);   // gate: below -55 dBFS RMS hold at 0 dB
        gdb += ((want > gdb) ? up : down) * (want - gdb);
        const double g = dbToLin(gdb); l *= g; r *= g;
    }
    double gainDb() const { return gdb; }
private:
    double sampleRate = 48000, eA = 0.001, up = 1e-5, down = 1e-5, e = 1e-6, gdb = 0.0, target = -20.0; bool enabled = false;
};
} // namespace vonox::dsp
