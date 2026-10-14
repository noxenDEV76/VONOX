#pragma once
// Small in-place radix-2 complex FFT (power-of-two sizes), double precision, no allocation after construction.
#include <vector>
#include <complex>
#include <cmath>
namespace vonox::dsp {
class FFT {
public:
    explicit FFT(int n = 512) { init(n); }
    void init(int n) {
        N = n; tw.resize(n / 2); rev.resize(n);
        for (int i = 0; i < n / 2; ++i) tw[i] = std::polar(1.0, -2.0 * 3.14159265358979323846 * i / n);
        int bits = 0; while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[i] = r; }
    }
    int size() const { return N; }
    void forward(std::complex<double>* x) const { run(x, false); }
    void inverse(std::complex<double>* x) const { run(x, true); for (int i = 0; i < N; ++i) x[i] /= double(N); }
private:
    void run(std::complex<double>* x, bool inv) const {
        for (int i = 0; i < N; ++i) if (i < rev[i]) std::swap(x[i], x[rev[i]]);
        for (int len = 2; len <= N; len <<= 1) {
            const int half = len / 2, step = N / len;
            for (int i = 0; i < N; i += len)
                for (int j = 0; j < half; ++j) {
                    std::complex<double> w = tw[j * step]; if (inv) w = std::conj(w);
                    const auto u = x[i + j], v = x[i + j + half] * w;
                    x[i + j] = u + v; x[i + j + half] = u - v;
                }
        }
    }
    int N = 0; std::vector<std::complex<double>> tw; std::vector<int> rev;
};
} // namespace vonox::dsp
