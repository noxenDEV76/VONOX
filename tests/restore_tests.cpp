#include "dsp/Restoration.h"
#include "dsp/NoiseReducer.h"
#include "dsp/Limiter2.h"
#include "dsp/Oversampler.h"
#include "dsp/PitchTracker.h"
#include <cstdio>
#include <random>
using namespace vonox::dsp;
static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++failures; } else std::printf("ok:   %s\n", m); } while (0)
static double rmsDb(const std::vector<double>& x, size_t a, size_t b) { double e = 0; for (size_t i = a; i < b; ++i) e += x[i] * x[i]; return 10 * std::log10(e / double(b - a) + 1e-20); }
static double goertzelMag(const std::vector<double>& x, size_t a, size_t b, double f, double sr) { double c = 2 * std::cos(2 * kPi * f / sr), s1 = 0, s2 = 0; for (size_t i = a; i < b; ++i) { const double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; } return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / double(b - a); }

int main() {
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        { // ---- noise reducer: identity at amount 0, exact latency
            NoiseReducer nr; nr.prepare(sr); nr.set(0.0); std::mt19937 g(1); std::uniform_real_distribution<double> u(-0.5, 0.5);
            const int n = int(sr * 0.5); std::vector<double> in(static_cast<size_t>(n)), out(static_cast<size_t>(n)); double l, r;
            for (int i = 0; i < n; ++i) { in[size_t(i)] = u(g); nr.process(in[size_t(i)], in[size_t(i)], l, r); out[size_t(i)] = l; }
            double e = 0; const int L = nr.latency(); for (int i = L + 600; i < n; ++i) e = std::max(e, std::abs(out[size_t(i)] - in[size_t(i - L)]));
            char m[100]; std::snprintf(m, sizeof m, "Noise reducer @%.0f: amount 0 is an exact delay of %d samples (err %.1e)", sr, L, e); CHECK(e < 1e-9, m);
        }
        { // ---- noise reduction: tone + steady noise -> noise floor drops, tone survives
            NoiseReducer nr; nr.prepare(sr); nr.set(1.0); std::mt19937 g(2); std::normal_distribution<double> nz(0, 0.02);
            const size_t n = size_t(sr * 6); std::vector<double> in(n), out(n); double l, r;
            for (size_t i = 0; i < n; ++i) { const bool tone = i > size_t(sr * 3) && i < size_t(sr * 5); in[i] = nz(g) + (tone ? 0.2 * std::sin(2 * kPi * 440 * i / sr) + 0.1 * std::sin(2 * kPi * 880 * i / sr) : 0.0); nr.process(in[i], in[i], l, r); out[i] = l; }
            const size_t L = size_t(nr.latency());
            const double noiseIn = rmsDb(in, size_t(sr * 2), size_t(sr * 2.8)), noiseOut = rmsDb(out, size_t(sr * 2) + L, size_t(sr * 2.8) + L);
            const double tIn = goertzelMag(in, size_t(sr * 3.5), size_t(sr * 4.5), 440, sr), tOut = goertzelMag(out, size_t(sr * 3.5) + L, size_t(sr * 4.5) + L, 440, sr);
            char m[140]; std::snprintf(m, sizeof m, "Noise reduction @%.0f: noise floor %.1f -> %.1f dB (>= 8 dB lower), 440 Hz tone kept within %.2f dB", sr, noiseIn, noiseOut, 20 * std::log10(tOut / tIn));
            CHECK(noiseIn - noiseOut > 8.0 && std::abs(20 * std::log10(tOut / tIn)) < 1.5, m);
        }
    }
    { const double sr = 48000; // ---- plosive control
        PlosiveControl pc; pc.prepare(sr); pc.set(1.0); std::vector<double> in, out;
        for (size_t i = 0; i < size_t(sr * 2); ++i) { const double t = i / sr; double x = 0.1 * std::sin(2 * kPi * 300 * t) + 0.05 * std::sin(2 * kPi * 3000 * t);
            if (t > 1.0 && t < 1.06) x += 0.6 * std::sin(2 * kPi * 70 * t) * std::sin(kPi * (t - 1.0) / 0.06); in.push_back(x); double l = x, r = x; pc.process(l, r); out.push_back(l); }
        std::vector<double> inLow, outLow; auto lowBand = [&](const std::vector<double>& v) { std::vector<double> o; Biquad b; b.set(Biquad::Type::LowPass, sr, 120.0, 0.7071); Biquad c; c.set(Biquad::Type::LowPass, sr, 120.0, 0.7071); for (double s : v) o.push_back(c.process(b.process(s))); return o; };
        inLow = lowBand(in); outLow = lowBand(out);
        const double red = rmsDb(inLow, size_t(sr * 1.01), size_t(sr * 1.06)) - rmsDb(outLow, size_t(sr * 1.01), size_t(sr * 1.06));
        const double toneErr = std::abs(20 * std::log10(goertzelMag(out, size_t(sr * 1.5), size_t(sr * 2.0), 300, sr) / goertzelMag(in, size_t(sr * 1.5), size_t(sr * 2.0), 300, sr)));
        char m[140]; std::snprintf(m, sizeof m, "Plosive control: pop reduced by %.1f dB (>= 8), voice tone changes %.2f dB (< 0.5)", red, toneErr); CHECK(red > 8.0 && toneErr < 0.5, m); }
    { const double sr = 48000; // ---- breath control: phrases (voiced) / quiet noisy breaths / loud sibilant
        BreathControl bc; bc.prepare(sr); bc.set(1.0); PitchTracker trk; trk.prepare(sr); std::mt19937 g(3); std::normal_distribution<double> nz(0, 1);
        const size_t N = size_t(sr * 8); std::vector<double> in(N), out(N); bool voiced = false; double hp = 0;
        for (size_t i = 0; i < N; ++i) { const double t = i / sr; double x = 0;
            const bool phrase = std::fmod(t, 2.0) < 1.0;                       // 0-1 voiced, 1-2 breath, ...
            if (phrase) { double v = 0; for (int k = 1; k <= 6; ++k) v += std::sin(2 * kPi * 180 * k * t) / k; x = 0.3 * v; }
            else { const double w = nz(g); hp = 0.9 * hp + w - 0.9 * 0; x = (t > 5.0 && t < 5.2 ? 0.25 : 0.012) * w; }   // 5.0-5.2 s: a loud sibilant-like burst; elsewhere quiet breath noise
            in[i] = x; if (trk.push(x)) voiced = trk.isVoiced(); double l = x, r = x; bc.process(l, r, voiced); out[i] = l; }
        const double breathRed = rmsDb(in, size_t(sr * 3.3), size_t(sr * 3.9)) - rmsDb(out, size_t(sr * 3.3), size_t(sr * 3.9));
        const double sibRed = rmsDb(in, size_t(sr * 5.05), size_t(sr * 5.15)) - rmsDb(out, size_t(sr * 5.05), size_t(sr * 5.15));
        const double voiceRed = rmsDb(in, size_t(sr * 4.2), size_t(sr * 4.8)) - rmsDb(out, size_t(sr * 4.2), size_t(sr * 4.8));
        char m[160]; std::snprintf(m, sizeof m, "Breath control: breath -%.1f dB (>= 6), loud sibilant -%.1f dB (< 3), voiced -%.2f dB (< 0.5)", breathRed, sibRed, voiceRed);
        CHECK(breathRed > 6.0 && sibRed < 3.0 && voiceRed < 0.5, m); }
    { const double sr = 48000; // ---- auto level
        AutoLevel al; al.prepare(sr); al.set(true, -20.0); const size_t N = size_t(sr * 12); std::vector<double> out(N);
        for (size_t i = 0; i < N; ++i) { double x = 0.0447 * std::sin(2 * kPi * 220 * i / sr); double l = x, r = x; al.process(l, r); out[i] = l; }   // about -30 dBFS RMS
        const double o = rmsDb(out, size_t(sr * 9), N);
        char m[120]; std::snprintf(m, sizeof m, "Auto-level: -30 dBFS input settles at %.1f dBFS (target -20, within 3 dB)", o); CHECK(std::abs(o + 20.0) < 3.0, m);
        AutoLevel q; q.prepare(sr); q.set(true, -20.0); std::mt19937 g(4); std::normal_distribution<double> nz(0, 0.0005); double l = 0, r = 0;
        for (size_t i = 0; i < N; ++i) { l = r = nz(g); q.process(l, r); }
        CHECK(std::abs(q.gainDb()) < 0.5, "Auto-level does not boost a -66 dBFS noise floor (gate)"); }
    for (double sr : { 44100.0, 48000.0, 96000.0 }) { // ---- true-peak limiter
        TruePeakLimiter lim; lim.prepare(sr); lim.set(-1.0, 100.0);
        const size_t N = size_t(sr * 2); std::vector<double> o(N); std::mt19937 g(5); std::uniform_real_distribution<double> u(-1, 1);
        for (size_t i = 0; i < N; ++i) { double a = 1.5 * std::sin(2 * kPi * (sr / 4.0) * i / sr + 0.785398) + (i > N / 2 ? 0.8 * u(g) : 0.0), b = 0.9 * std::sin(2 * kPi * 1000 * i / sr); lim.process(a, b); o[i] = a; }
        // independent reference true-peak: 8x polyphase with a long windowed-sinc
        double tp = 0; const int T = 64; for (size_t i = 200; i + 80 < N; ++i) for (int ph = 0; ph < 8; ++ph) { double s = 0; for (int k = -T / 2; k < T / 2; ++k) { const double x = k - ph / 8.0, w = 0.5 * (1 + std::cos(kPi * x / (T / 2 + 1))); s += o[i + size_t(k + 0)] * ((std::abs(x) < 1e-9) ? 1.0 : std::sin(kPi * x) / (kPi * x)) * w; } tp = std::max(tp, std::abs(s)); }
        const double tpDb = 20 * std::log10(tp);
        char m[140]; std::snprintf(m, sizeof m, "True-peak limiter @%.0f: independent 8x true-peak %.2f dBTP vs ceiling -1.0 (<= -0.7)", sr, tpDb); CHECK(tpDb < -0.7, m);
        TruePeakLimiter q; q.prepare(sr); q.set(-1.0, 100.0); double e = 0, a, b; std::vector<double> in(static_cast<size_t>(sr)), out(static_cast<size_t>(sr));
        for (size_t i = 0; i < in.size(); ++i) { in[i] = 0.2 * std::sin(2 * kPi * 440 * i / sr); a = in[i]; b = in[i]; q.process(a, b); out[i] = a; }
        const size_t L = size_t(q.latency()); for (size_t i = L + 100; i < in.size(); ++i) e = std::max(e, std::abs(out[i] - in[i - L]));
        CHECK(e < 1e-9, "Limiter below ceiling is a pure delay of latency() samples"); }
    { // ---- oversampler: transparent for a clean tone, and reduces aliasing of a non-linearity
        const double sr = 48000; Oversampler2x os; std::vector<double> y, ref;
        for (size_t i = 0; i < size_t(sr); ++i) { const double x = 0.5 * std::sin(2 * kPi * 1000 * i / sr); ref.push_back(x); y.push_back(os.process(x, [](double v) { return v; })); }
        const double err = std::abs(20 * std::log10(goertzelMag(y, 20000, 40000, 1000, sr) / goertzelMag(ref, 20000, 40000, 1000, sr)));
        CHECK(err < 0.1, "Oversampler passes a 1 kHz tone within 0.1 dB");
        double e2 = 0; for (size_t i = 100; i < 5000; ++i) e2 = std::max(e2, std::abs(y[i] - ref[i - Oversampler2x::kLatency])); char m[100]; std::snprintf(m, sizeof m, "Oversampler latency is exactly %d samples (max error %.2e)", Oversampler2x::kLatency, e2); CHECK(e2 < 2e-3, m);
        Oversampler2x o2; std::vector<double> a1, a0; const double f = 13000; double ph = 0;
        for (size_t i = 0; i < size_t(sr); ++i) { const double x = 0.8 * std::sin(2 * kPi * f * i / sr); a1.push_back(o2.process(x, [](double v) { return std::tanh(3 * v); })); a0.push_back(std::tanh(3 * x)); }
        (void) ph; const double alias1 = goertzelMag(a1, 20000, 40000, 9000, sr), alias0 = goertzelMag(a0, 20000, 40000, 9000, sr);   // 3*13k = 39k aliases to 9k
        char m2[140]; std::snprintf(m2, sizeof m2, "Oversampling lowers the 3rd-harmonic alias of a 13 kHz tone by %.1f dB (>= 12)", 20 * std::log10(alias0 / alias1)); CHECK(20 * std::log10(alias0 / alias1) > 12.0, m2); }
    std::printf(failures ? "RESTORE TESTS FAILED (%d)\n" : "RESTORE TESTS PASSED\n", failures); return failures ? 1 : 0;
}
