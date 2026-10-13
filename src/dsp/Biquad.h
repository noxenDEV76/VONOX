#pragma once
#include <cmath>
#include <algorithm>

namespace vonox::dsp {

constexpr double kPi = 3.14159265358979323846;

// Robust Direct Form II Transposed biquad (RBJ cookbook coefficients).
class Biquad {
public:
    enum class Type { HighPass, LowShelf, Bell, HighShelf, LowPass };

    void set(Type t, double sampleRate, double freq, double q, double gainDb = 0.0) {
        freq = std::clamp(freq, 10.0, sampleRate * 0.45);
        q = std::clamp(q, 0.1, 10.0);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * freq / sampleRate;
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double alpha = sw / (2.0 * q);
        double b0, b1, b2, a0, a1, a2;
        switch (t) {
        case Type::HighPass:
            b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case Type::LowPass:
            b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case Type::Bell:
            b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
        case Type::LowShelf: {
            const double s = 2.0 * std::sqrt(A) * alpha;
            b0 = A * ((A + 1) - (A - 1) * cw + s);
            b1 = 2 * A * ((A - 1) - (A + 1) * cw);
            b2 = A * ((A + 1) - (A - 1) * cw - s);
            a0 = (A + 1) + (A - 1) * cw + s;
            a1 = -2 * ((A - 1) + (A + 1) * cw);
            a2 = (A + 1) + (A - 1) * cw - s; break; }
        case Type::HighShelf: default: {
            const double s = 2.0 * std::sqrt(A) * alpha;
            b0 = A * ((A + 1) + (A - 1) * cw + s);
            b1 = -2 * A * ((A - 1) + (A + 1) * cw);
            b2 = A * ((A + 1) + (A - 1) * cw - s);
            a0 = (A + 1) - (A - 1) * cw + s;
            a1 = 2 * ((A - 1) - (A + 1) * cw);
            a2 = (A + 1) - (A - 1) * cw - s; break; }
        }
        c0 = b0 / a0; c1 = b1 / a0; c2 = b2 / a0; d1 = a1 / a0; d2 = a2 / a0;
    }

    void reset() { z1 = z2 = 0.0; }

    inline double process(double x) {
        const double y = c0 * x + z1;
        z1 = c1 * x - d1 * y + z2;
        z2 = c2 * x - d2 * y;
        // flush denormals
        if (std::abs(z1) < 1e-30) z1 = 0.0;
        if (std::abs(z2) < 1e-30) z2 = 0.0;
        return y;
    }

    // Magnitude response in dB at frequency f (for tests / UI curve)
    double magnitudeDb(double f, double sampleRate) const {
        const double w = 2.0 * kPi * f / sampleRate;
        const double cw = std::cos(w), sw = std::sin(w);
        const double c2w = std::cos(2 * w), s2w = std::sin(2 * w);
        const double nr = c0 + c1 * cw + c2 * c2w, ni = -(c1 * sw + c2 * s2w);
        const double dr = 1 + d1 * cw + d2 * c2w, di = -(d1 * sw + d2 * s2w);
        return 10.0 * std::log10((nr * nr + ni * ni) / (dr * dr + di * di));
    }

private:
    double c0 = 1, c1 = 0, c2 = 0, d1 = 0, d2 = 0, z1 = 0, z2 = 0;
};

} // namespace vonox::dsp
