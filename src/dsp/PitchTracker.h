#pragma once
// Monophonic pitch tracker (YIN: de Cheveigne & Kawahara 2002) on a decimated copy of the signal.
// Feed one sample at a time; every hop it publishes f0 (Hz), a confidence in 0..1 and a voiced flag.
// The estimate describes the window centre, i.e. roughly kWindow/2 decimated samples in the past.
#include "Biquad.h"
#include <vector>
#include <cmath>
#include <algorithm>
namespace vonox::dsp {
class PitchTracker {
public:
    static constexpr double kMinHz = 70.0, kMaxHz = 1000.0;
    void prepare(double sampleRate) {
        sr = sampleRate; dec = std::max(1, int(std::lround(sr / 16000.0))); dsr = sr / dec;
        win = 1024; hop = std::max(32, int(dsr * 0.008));
        tauMin = std::max(2, int(dsr / kMaxHz)); tauMax = std::min(win / 2 - 2, int(dsr / kMinHz) + 2);
        buf.assign(size_t(win), 0.0); d.assign(size_t(tauMax + 2), 0.0); dn.assign(size_t(tauMax + 2), 0.0);
        for (auto& f : lp) f.set(Biquad::Type::LowPass, sr, std::min(0.45 * dsr, 3000.0), 0.7071);
        reset();
    }
    void reset() { std::fill(buf.begin(), buf.end(), 0.0); for (auto& f : lp) f.reset(); wpos = 0; cnt = 0; decCnt = 0; f0 = 0; conf = 0; voiced = false; level = 0; }
    // returns true when a new estimate was published
    bool push(double x) {
        const double y = lp[1].process(lp[0].process(x));          // anti-alias + remove >3 kHz (helps YIN on vowels)
        if (++decCnt < dec) return false; decCnt = 0;
        buf[size_t(wpos)] = y; wpos = (wpos + 1) % win;
        if (++cnt < hop) return false; cnt = 0;
        estimate(); return true;
    }
    double frequency() const { return f0; }
    double confidence() const { return conf; }
    bool isVoiced() const { return voiced; }
    double levelDb() const { return level; }   // window RMS in dBFS (of the filtered signal)
private:
    void estimate() {
        std::vector<double>& w = tmp; w.resize(size_t(win));
        double e = 0; for (int i = 0; i < win; ++i) { w[size_t(i)] = buf[size_t((wpos + i) % win)]; e += w[size_t(i)] * w[size_t(i)]; }
        level = 10.0 * std::log10(e / win + 1e-12);
        const int W = win - tauMax - 1;
        if (level < -60.0) { voiced = false; conf = 0; return; }
        for (int t = 1; t <= tauMax; ++t) { double s = 0; for (int j = 0; j < W; ++j) { const double a = w[size_t(j)] - w[size_t(j + t)]; s += a * a; } d[size_t(t)] = s; }
        double run = 0; dn[0] = 1.0;
        for (int t = 1; t <= tauMax; ++t) { run += d[size_t(t)]; dn[size_t(t)] = d[size_t(t)] * t / (run + 1e-30); }
        int best = -1; const double thr = 0.15;
        for (int t = tauMin; t <= tauMax; ++t) {
            if (dn[size_t(t)] < thr) { while (t + 1 <= tauMax && dn[size_t(t + 1)] < dn[size_t(t)]) ++t; best = t; break; }
        }
        if (best < 0) { // no dip under the threshold: take the global minimum, flag low confidence
            double m = 1e9; for (int t = tauMin; t <= tauMax; ++t) if (dn[size_t(t)] < m) { m = dn[size_t(t)]; best = t; }
            if (m > 0.4) { voiced = false; conf = std::max(0.0, 1.0 - m); return; }
        }
        double tau = best;                                              // parabolic interpolation
        if (best > tauMin && best < tauMax) {
            const double a = dn[size_t(best - 1)], b = dn[size_t(best)], c = dn[size_t(best + 1)], den = a - 2 * b + c;
            if (std::abs(den) > 1e-12) tau = best + 0.5 * (a - c) / den;
        }
        const double hz = dsr / tau;
        conf = std::clamp(1.0 - dn[size_t(best)], 0.0, 1.0);
        if (hz >= kMinHz && hz <= kMaxHz && conf > 0.7) { f0 = hz; voiced = true; } else voiced = false;
    }
    double sr = 48000, dsr = 16000, f0 = 0, conf = 0, level = 0; bool voiced = false;
    int dec = 3, win = 1024, hop = 128, tauMin = 16, tauMax = 230, wpos = 0, cnt = 0, decCnt = 0;
    std::vector<double> buf, d, dn, tmp; Biquad lp[2];
};
} // namespace vonox::dsp
