#pragma once
#include "dsp/Dynamics.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace vonox::engine {

using namespace vonox::dsp;

// Result of a real measurement pass. Nothing here is invented: every field
// comes from the audio that was analysed.
struct VocalProfile {
    bool valid = false;
    std::string note;             // why analysis is unreliable, if it is

    double peakDb = -120, rmsDb = -120, crestDb = 0;
    double dynamicRangeDb = 0;    // 95th - 10th percentile of short-term RMS
    double noiseFloorDb = -120;   // 10th percentile of active-frame levels
    double clippedFraction = 0;   // samples at/over 0.999 in runs >= 3
    // Band energy relative to total (dB, relative to full-band RMS)
    double rumbleDb = -120;       // < 80 Hz
    double lowMidDb = -120;       // 150-450 Hz
    double presenceDb = -120;     // 2-5 kHz
    double sibilanceDb = -120;    // 5-10 kHz
    double airDb = -120;          // 10-16 kHz
    // Derived indicators (0..1)
    double lowMidBuildup = 0, sibilance = 0, brightness = 0, rumble = 0;
    double stereoCorrelation = 1.0;
    bool clipped = false, noisy = false;

    std::string summary() const;
};

class VocalAnalyzer {
public:
    // mono analysis (stereo callers pass mid; correlation passed separately)
    static VocalProfile analyze(const std::vector<float>& x, double sr,
                                const std::vector<float>* right = nullptr) {
        VocalProfile p;
        const size_t minSamples = static_cast<size_t>(sr * 1.0);
        if (x.size() < minSamples) {
            p.note = "Audio too short (need at least 1 second)";
            return p;
        }

        // Level, peak, clipping
        double peak = 0, sum2 = 0;
        size_t clipRun = 0, clipped = 0;
        for (float s : x) {
            const double a = std::abs(static_cast<double>(s));
            peak = std::max(peak, a); sum2 += a * a;
            if (a >= 0.999) { if (++clipRun >= 3) ++clipped; } else clipRun = 0;
        }
        const double rms = std::sqrt(sum2 / x.size());
        p.peakDb = linToDb(peak); p.rmsDb = linToDb(rms);
        if (p.rmsDb < -70.0) { p.note = "Audio is silent"; return p; }
        p.crestDb = p.peakDb - p.rmsDb;
        p.clippedFraction = double(clipped) / x.size();
        p.clipped = p.clippedFraction > 0.0005;

        // Short-term (50 ms) levels -> dynamics + noise floor
        const size_t win = static_cast<size_t>(sr * 0.05);
        std::vector<double> frames;
        for (size_t i = 0; i + win <= x.size(); i += win) {
            double s2 = 0; for (size_t k = 0; k < win; ++k) s2 += double(x[i + k]) * x[i + k];
            frames.push_back(linToDb(std::sqrt(s2 / win)));
        }
        std::vector<double> active;
        for (double f : frames) if (f > p.rmsDb - 40.0) active.push_back(f);
        if (active.size() < 10) { p.note = "Not enough vocal activity to analyse reliably"; return p; }
        std::sort(active.begin(), active.end());
        auto pct = [&](double q) { return active[static_cast<size_t>(q * (active.size() - 1))]; };
        p.dynamicRangeDb = pct(0.95) - pct(0.10);
        std::sort(frames.begin(), frames.end());
        p.noiseFloorDb = frames[static_cast<size_t>(0.10 * (frames.size() - 1))];
        p.noisy = (p.rmsDb - p.noiseFloorDb) < 25.0;

        // Band energies via cascaded biquad band-pass (HP + LP)
        auto bandDb = [&](double lo, double hi) {
            Biquad h1, h2, l1, l2;
            const double nyq = sr * 0.45; hi = std::min(hi, nyq);
            if (lo <= 20.0) { h1.set(Biquad::Type::HighPass, sr, 20.0, 0.7071); h2 = h1; }
            else { h1.set(Biquad::Type::HighPass, sr, lo, 0.7071); h2 = h1; }
            l1.set(Biquad::Type::LowPass, sr, hi, 0.7071); l2 = l1;
            double e = 0;
            for (float s : x) { double y = l2.process(l1.process(h2.process(h1.process(s)))); e += y * y; }
            return linToDb(std::sqrt(e / x.size())) - p.rmsDb;
        };
        p.rumbleDb    = bandDb(20, 80);
        p.lowMidDb    = bandDb(150, 450);
        p.presenceDb  = bandDb(2000, 5000);
        p.sibilanceDb = bandDb(5000, 10000);
        p.airDb       = bandDb(10000, 16000);

        // Derived 0..1 indicators from measured numbers
        p.lowMidBuildup = clamp01((p.lowMidDb + 9.0) / 9.0);   // -9 dB or lower -> 0, 0 dB -> 1
        p.sibilance     = clamp01((p.sibilanceDb + 24.0) / 12.0);
        p.brightness    = clamp01((p.presenceDb + 18.0) / 10.0 * 0.6 + (p.airDb + 40.0) / 20.0 * 0.4);
        p.rumble        = clamp01((p.rumbleDb + 30.0) / 18.0);

        if (right && right->size() == x.size()) { // stereo correlation
            double lr = 0, ll = 0, rr = 0;
            for (size_t i = 0; i < x.size(); ++i) { lr += double(x[i]) * (*right)[i];
                ll += double(x[i]) * x[i]; rr += double((*right)[i]) * (*right)[i]; }
            p.stereoCorrelation = (ll > 0 && rr > 0) ? lr / std::sqrt(ll * rr) : 1.0;
        }
        p.valid = true;
        return p;
    }
private:
    static double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }
};

inline std::string VocalProfile::summary() const {
    if (!valid) return "Analysis failed: " + note + ". Using safe defaults.";
    auto lvl = [](double v, const char* lo, const char* mid, const char* hi) {
        return v < 0.33 ? lo : (v < 0.66 ? mid : hi); };
    std::string s = "Analysis complete: ";
    s += std::string(lvl(lowMidBuildup, "low low-mid", "moderate low-mid", "strong low-mid buildup")) + ", ";
    s += std::string(lvl(sibilance, "low sibilance", "moderate sibilance", "strong sibilance")) + ", ";
    s += dynamicRangeDb > 12 ? "variable dynamics" : (dynamicRangeDb > 7 ? "moderate dynamics" : "consistent dynamics");
    if (clipped) s += ", CLIPPING detected";
    if (noisy) s += ", noisy recording";
    return s;
}

} // namespace vonox::engine
