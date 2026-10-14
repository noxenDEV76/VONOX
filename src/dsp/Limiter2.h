#pragma once
// Look-ahead true-peak limiter + true-peak meter.
// Peaks are measured on a 4x band-limited interpolation of the signal (so inter-sample peaks that would clip a
// DAC or a lossy codec are caught), gain reduction is applied `lookahead` samples ahead and smoothed with a
// moving average (no overshoot, no distortion from fast gain jumps), followed by a hard safety clamp.
#include "Dynamics.h"
#include <deque>
#include <vector>
#include <array>
namespace vonox::dsp {

// 4x interpolation FIR (windowed sinc, 24 taps per phase)
class TruePeakDetector {
public:
    static constexpr int kTaps = 48;
    TruePeakDetector() {
        for (int ph = 1; ph < 4; ++ph) {
            double sum = 0;
            for (int k = 0; k < kTaps; ++k) {
                const double x = (k - kTaps / 2 + 1) - ph / 4.0;       // position of the interpolated point relative to tap k
                const double sinc = std::abs(x) < 1e-9 ? 1.0 : std::sin(3.14159265358979323846 * x) / (3.14159265358979323846 * x);
                const double w = 0.5 * (1.0 + std::cos(3.14159265358979323846 * x / (kTaps / 2.0 + 0.5)));
                coef[ph][k] = sinc * w; sum += coef[ph][k];
            }
            for (int k = 0; k < kTaps; ++k) coef[ph][k] /= sum;       // unity DC gain
        }
    }
    void reset() { hist.fill(0.0); pos = 0; }
    // push a sample, return the largest absolute value among the sample and its 3 interpolated neighbours (centered kTaps/2-1 samples back)
    inline double push(double x) {
        hist[size_t(pos)] = x; pos = (pos + 1) % kTaps;
        double pk = std::abs(hist[size_t((pos + kTaps / 2 - 1) % kTaps)]);
        for (int ph = 1; ph < 4; ++ph) { double s = 0; for (int k = 0; k < kTaps; ++k) s += coef[ph][k] * hist[size_t((pos + k) % kTaps)]; pk = std::max(pk, std::abs(s)); }
        return pk;
    }
    static constexpr int delaySamples() { return kTaps / 2; }    // the peak refers to a point this many samples in the past
private:
    double coef[4][kTaps] = {}; std::array<double, kTaps> hist{}; int pos = 0;
};

class TruePeakLimiter {
public:
    void prepare(double sr, int maxChannels = 2) {
        sampleRate = sr; look = std::max(16, int(std::ceil(0.0015 * sr))); (void) maxChannels;
        delayBuf[0].assign(size_t(look + TruePeakDetector::delaySamples() + 8), 0.0); delayBuf[1] = delayBuf[0];
        reset(); set(-1.0, 100.0);
    }
    void reset() { for (auto& d : det) d.reset(); std::fill(delayBuf[0].begin(), delayBuf[0].end(), 0.0); std::fill(delayBuf[1].begin(), delayBuf[1].end(), 0.0); dpos = 0; state = 1.0; ma.assign(size_t(look), 1.0); maSum = double(look); mpos = 0; peakQ.clear(); idx = 0; }
    void set(double ceilingDb, double releaseMs) { ceil = dbToLin(ceilingDb); rel = std::exp(-1.0 / (sampleRate * releaseMs * 1e-3)); }
    int latency() const { return look + TruePeakDetector::delaySamples(); }
    double gainReductionDb() const { return -linToDb(std::max(1e-6, curGain)); }
    double truePeakOutDb() const { return linToDb(outPeak); }
    void resetPeak() { outPeak = 0; }
    // in-place stereo (or mono: pass the same variable twice is NOT supported; use b = dummy)
    inline void process(double& a, double& b) {
        const int D = TruePeakDetector::delaySamples();
        const double tp = std::max(det[0].push(a), det[1].push(b));            // peak of the sample D back
        // sliding maximum over the last `look` detector outputs (monotonic deque)
        while (!peakQ.empty() && peakQ.back().second <= tp) peakQ.pop_back();
        peakQ.push_back({ idx, tp }); while (peakQ.front().first <= idx - look) peakQ.pop_front();
        const double pkMax = peakQ.front().second;
        const double target = ceil * 0.97;                                     // 0.26 dB margin for FIR interpolation error
        const double need = (pkMax > target) ? target / pkMax : 1.0;
        state = (need < state) ? need : rel * state + (1.0 - rel) * need;       // instant attack (we are `look` early), smooth release
        maSum += state - ma[size_t(mpos)]; ma[size_t(mpos)] = state; mpos = (mpos + 1) % look;
        curGain = maSum / look;
        // audio delayed by look + D so the reduction arrives exactly when the peak does
        const size_t n = delayBuf[0].size();
        delayBuf[0][size_t(dpos)] = a; delayBuf[1][size_t(dpos)] = b;
        const size_t rp = (size_t(dpos) + n - size_t(look + D)) % n; dpos = int((size_t(dpos) + 1) % n);
        a = std::clamp(delayBuf[0][rp] * curGain, -ceil, ceil); b = std::clamp(delayBuf[1][rp] * curGain, -ceil, ceil);
        outPeak = std::max(outPeak, std::max(std::abs(a), std::abs(b)));
        ++idx;
    }
private:
    double sampleRate = 48000, ceil = 0.89, rel = 0.999, state = 1.0, curGain = 1.0, maSum = 0, outPeak = 0; int look = 72, dpos = 0, mpos = 0; long long idx = 0;
    TruePeakDetector det[2]; std::vector<double> delayBuf[2], ma; std::deque<std::pair<long long, double>> peakQ;
};
} // namespace vonox::dsp
