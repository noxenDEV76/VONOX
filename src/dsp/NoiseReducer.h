#pragma once
// STFT spectral noise reduction (512-point sqrt-Hann, 75 % overlap) with a per-bin minimum-statistics noise
// estimate that adapts continuously (no "learn" step needed), over-subtraction controlled by `amount`,
// and time/frequency smoothing of the gain to avoid musical noise. Stereo-linked gain.
// Fixed algorithmic latency: latency() samples. With amount = 0 the stage is an exact delay.
#include "FFT.h"
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>
namespace vonox::dsp {
class NoiseReducer {
public:
    static constexpr int N = 512, HOP = 128, BINS = N / 2 + 1;
    NoiseReducer() : fft(N) { for (int i = 0; i < N; ++i) win[size_t(i)] = std::sqrt(0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * (i + 0.5) / N))); }
    void prepare(double sr) { sampleRate = sr; relA = std::exp(-double(HOP) / (sr * 0.060)); reset(); }
    void reset() {
        for (auto& c : inBuf) c.assign(size_t(N), 0.0); for (auto& c : outBuf) c.assign(size_t(N * 2), 0.0);
        ps.assign(BINS, 1e-10); nz.assign(BINS, 1e-6); gain.assign(BINS, 1.0); first = true; pos = 0; sinceHop = 0; outRead = 0; wr = 0; grDb = 0; tcount = 0;
    }
    int latency() const { return N - 1; }
    void set(double amount) { amt = std::clamp(amount, 0.0, 1.0); }
    double gainReductionDb() const { return grDb; }
    void process(double l, double r, double& ol, double& orr) {
        inBuf[0][size_t(pos)] = l; inBuf[1][size_t(pos)] = r; pos = (pos + 1) % N; ++tcount;
        if (++sinceHop >= HOP) { sinceHop = 0; frame(); }
        // overlap-add output stream, read with a fixed delay of N-HOP samples relative to the input
        const int64_t tau = tcount - 1 - (N - 1);                    // stream time of the sample that is complete now
        if (tau >= 0) { const size_t ri = size_t(tau % (N * 2)); ol = outBuf[0][ri]; orr = outBuf[1][ri]; outBuf[0][ri] = 0.0; outBuf[1][ri] = 0.0; }
        else ol = orr = 0.0;
    }
private:
    void frame() {
        std::complex<double> X[2][N];
        for (int c = 0; c < 2; ++c) { for (int i = 0; i < N; ++i) X[c][i] = inBuf[c][size_t((pos + i) % N)] * win[size_t(i)]; fft.forward(X[c]); }
        // noise tracking on the mean power of both channels
        std::vector<double>& p = tmpP; p.resize(BINS);
        for (int k = 0; k < BINS; ++k) {
            const double pw = 0.5 * (std::norm(X[0][k]) + std::norm(X[1][k])) / (N * 0.5) + 1e-12;
            ps[size_t(k)] = first ? pw : 0.8 * ps[size_t(k)] + 0.2 * pw; p[size_t(k)] = pw;
            double& n = nz[size_t(k)];
            if (first || ps[size_t(k)] < n) n = ps[size_t(k)]; else n *= 1.0 + 0.004;     // slow upward leak (~ seconds)
        }
        first = false;
        const double gmin = std::pow(10.0, -amt * 18.0 / 20.0), over = 1.0 + 2.0 * amt;
        double sumG = 0; std::vector<double>& g = tmpG; g.resize(BINS);
        for (int k = 0; k < BINS; ++k) {
            const double noise = 1.6 * nz[size_t(k)] * over;                              // 1.6: min-statistics bias compensation
            g[size_t(k)] = (amt <= 0.0) ? 1.0 : std::max(gmin, 1.0 - noise / (ps[size_t(k)] + 1e-18));
        }
        for (int k = 0; k < BINS; ++k) {                                                   // 3-bin smoothing across frequency
            const double a = g[size_t(std::max(0, k - 1))], b = g[size_t(k)], c = g[size_t(std::min(BINS - 1, k + 1))];
            const double s = (amt <= 0.0) ? 1.0 : 0.25 * a + 0.5 * b + 0.25 * c;
            gain[size_t(k)] = (s < gain[size_t(k)]) ? s : relA * gain[size_t(k)] + (1.0 - relA) * s;   // fast attack of reduction, slow release
            sumG += gain[size_t(k)];
        }
        grDb = -20.0 * std::log10(std::max(1e-6, sumG / BINS));
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < BINS; ++k) X[c][k] *= gain[size_t(k)];
            for (int k = 1; k < N / 2; ++k) X[c][N - k] = std::conj(X[c][k]);
            fft.inverse(X[c]);
            for (int i = 0; i < N; ++i) { const int64_t st = tcount - N + i; if (st >= 0) outBuf[c][size_t(st % (N * 2))] += X[c][i].real() * win[size_t(i)] * 0.5; }   // sqrt-Hann analysis x synthesis at 75 % overlap sums to 2
        }
    }
    FFT fft; double sampleRate = 48000, amt = 0, relA = 0.5, grDb = 0; std::array<double, N> win{};
    std::array<std::vector<double>, 2> inBuf, outBuf; std::vector<double> ps, nz, gain, tmpP, tmpG;
    bool first = true; int pos = 0, sinceHop = 0, outRead = 0, wr = 0, outBase = 0; int64_t tcount = 0;
};
} // namespace vonox::dsp
