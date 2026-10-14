#pragma once
// 2x oversampler with a linear-phase 31-tap Kaiser half-band FIR. Fixed latency of kLatency samples at the base rate.
#include <array>
#include <cmath>
namespace vonox::dsp {
class Oversampler2x {
public:
    static constexpr int kTaps = 31, kLatency = 15;
    Oversampler2x() {
        auto i0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 25; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        const double beta = 7.0; double sum = 0;
        for (int j = 0; j < kTaps; ++j) {
            const double m = j - 15.0, sinc = (m == 0) ? 0.5 : std::sin(3.14159265358979323846 * m / 2.0) / (3.14159265358979323846 * m);
            const double r = m / 15.0, w = i0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0(beta);
            h[size_t(j)] = sinc * w; sum += h[size_t(j)];
        }
        for (auto& v : h) v /= sum;                           // unity DC gain at the 2x rate
    }
    void reset() { up.fill(0.0); dn.fill(0.0); upPos = dnPos = 0; }
    template <class F> inline double process(double x, F&& nonlinear) {
        up[size_t(upPos)] = x; upPos = (upPos + 1) % kTaps;
        double z[2];
        for (int ph = 0; ph < 2; ++ph) {                      // zero-stuffed input convolved with h (x2 gain restores level)
            double s = 0; for (int k = ph; k < kTaps; k += 2) s += h[size_t(k)] * up[size_t((upPos - 1 - (k - ph) / 2 + 2 * kTaps) % kTaps)];
            z[ph] = nonlinear(2.0 * s);
        }
        // decimate on the even phase so the total delay is exactly kLatency base-rate samples (2 x 15 at 2x rate)
        double y = 0;
        dn[size_t(dnPos)] = z[0]; dnPos = (dnPos + 1) % kTaps;
        { double s = 0; for (int k = 0; k < kTaps; ++k) s += h[size_t(k)] * dn[size_t((dnPos - 1 - k + 2 * kTaps) % kTaps)]; y = s; }
        dn[size_t(dnPos)] = z[1]; dnPos = (dnPos + 1) % kTaps;
        return y;
    }
private:
    std::array<double, kTaps> h{}, up{}, dn{}; int upPos = 0, dnPos = 0;
};
} // namespace vonox::dsp
