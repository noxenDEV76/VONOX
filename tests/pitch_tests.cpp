#include "dsp/PitchProcessor.h"
#include <cstdio>
#include <random>
#include <vector>
using namespace vonox::dsp; using namespace vonox::engine;
static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++failures; } else std::printf("ok:   %s\n", m); } while (0)
static double cents(double a, double b) { return 1200.0 * std::log2(a / b); }

struct Tone { double ph = 0; double sr; explicit Tone(double s) : sr(s) {} double next(double f) { ph += 2 * kPi * f / sr; double v = 0; for (int k = 1; k <= 6; ++k) v += std::sin(k * ph) / k; return 0.25 * v; } };

// median pitch of a signal region measured with the tracker
static double measure(const std::vector<double>& x, double sr, size_t from, size_t to, double* spread = nullptr) {
    PitchTracker t; t.prepare(sr); std::vector<double> f;
    for (size_t i = 0; i < to && i < x.size(); ++i) if (t.push(x[i]) && i >= from && t.isVoiced()) f.push_back(t.frequency());
    if (f.size() < 5) return 0; std::sort(f.begin(), f.end());
    if (spread) *spread = 1200.0 * std::log2(f[f.size() * 9 / 10] / f[f.size() / 10]);
    return f[f.size() / 2];
}

int main() {
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        for (double f : { 90.0, 130.0, 220.0, 440.0, 880.0 }) {
            Tone t(sr); std::vector<double> x(static_cast<size_t>(sr * 1.5)); for (auto& v : x) v = t.next(f);
            const double e = cents(measure(x, sr, size_t(sr * 0.4), x.size()), f);
            char m[120]; std::snprintf(m, sizeof m, "Tracker %.0f Hz @%.0f: error %.1f cents (< 8)", f, sr, e); CHECK(std::abs(e) < 8.0, m);
        }
    }
    { const double sr = 48000; std::mt19937 g(1); std::normal_distribution<double> n(0, 0.1); std::vector<double> x(static_cast<size_t>(sr)); for (auto& v : x) v = n(g);
      PitchTracker t; t.prepare(sr); int voiced = 0, tot = 0; for (size_t i = 0; i < x.size(); ++i) if (t.push(x[i])) { ++tot; if (t.isVoiced()) ++voiced; }
      CHECK(voiced < tot * 0.1, "Tracker does not call white noise voiced (< 10 %)"); }
    { const double sr = 48000; Tone t(sr); std::vector<double> x(size_t(sr * 2)); double worst = 0;
      for (size_t i = 0; i < x.size(); ++i) x[i] = t.next(300.0 * std::pow(2.0, 40.0 * std::sin(2 * kPi * 5.5 * i / sr) / 1200.0));
      PitchTracker tr; tr.prepare(sr); int cnt = 0; double acc = 0;
      for (size_t i = 0; i < x.size(); ++i) if (tr.push(x[i]) && i > sr * 0.5 && tr.isVoiced()) { acc += std::abs(cents(tr.frequency(), 300.0)); ++cnt; worst = std::max(worst, std::abs(cents(tr.frequency(), 300.0))); }
      char m[120]; std::snprintf(m, sizeof m, "Tracker follows +-40 cent vibrato (mean |dev| %.1f cents, worst %.1f)", acc / cnt, worst); CHECK(cnt > 50 && worst < 70, m); }

    { std::array<double, 12> h{}; const int cmaj[] = { 0, 2, 4, 5, 7, 9, 11 }; for (int n : cmaj) h[size_t(n)] = 30; h[0] = 80; h[7] = 60; h[4] = 50;
      auto k = estimateKey(h); CHECK(k.valid && k.key == 0 && !k.minor && k.confidence > 0.2, "Key estimation: C major profile -> C major");
      std::array<double, 12> m{}; const int amin[] = { 9, 11, 0, 2, 4, 5, 7 }; for (int n : amin) m[size_t(n)] = 30; m[9] = 90; m[4] = 60; m[0] = 45;
      auto k2 = estimateKey(m); CHECK(k2.valid && k2.key == 9 && k2.minor, "Key estimation: A minor profile -> A minor");
      std::array<double, 12> flat{}; for (auto& v : flat) v = 10; auto k3 = estimateKey(flat); CHECK(!k3.valid || k3.confidence < 0.15, "Key estimation: flat histogram -> low confidence"); }
    { auto mk = scaleMask(0, ScaleType::Major); CHECK(quantizeNote(61.3, mk) == 60 || quantizeNote(61.3, mk) == 62, "Quantiser stays inside the scale (C# -> C or D)");
      CHECK(stepInScale(69, 2, mk) == 72, "Diatonic 3rd above A in C major is C");
      CHECK(stepInScale(64, 2, mk) == 67, "Diatonic 3rd above E in C major is G"); }

    for (double sr : { 44100.0, 48000.0 }) {
        // ---- correction: 40 cents sharp A4 -> A4
        PitchProcessor pp; pp.prepare(sr); PitchParams p; p.amount = 1.0; p.retuneMs = 15; p.humanize = 0.0; p.key = 0; p.scale = 0; pp.set(p);
        Tone t(sr); const size_t N = size_t(sr * 2.5); std::vector<double> out(N); double l, r, hl, hr;
        for (size_t i = 0; i < N; ++i) { const double x = t.next(440.0 * std::pow(2.0, 40.0 / 1200.0)); pp.process(x, x, l, r, hl, hr); out[i] = l; }
        const double e = cents(measure(out, sr, size_t(sr * 1.2), N), 440.0);
        char m[160]; std::snprintf(m, sizeof m, "Pitch correction @%.0f: +40 cents sharp -> %.1f cents from A440 (|e| < 10)", sr, e); CHECK(std::abs(e) < 10.0, m);
        CHECK(pp.latency() > 0 && pp.latency() < int(sr * 0.04), "Pitch stage reports a fixed latency under 40 ms");
        // ---- amount 0: pure delayed copy
        PitchProcessor q; q.prepare(sr); PitchParams z; z.amount = 0; q.set(z); Tone t2(sr); std::vector<double> in(static_cast<size_t>(sr)), o2(static_cast<size_t>(sr));
        for (size_t i = 0; i < in.size(); ++i) { in[i] = t2.next(200.0); q.process(in[i], in[i], l, r, hl, hr); o2[i] = l; }
        double err = 0; for (size_t i = size_t(q.latency()) + 10; i < in.size(); ++i) err = std::max(err, std::abs(o2[i] - in[i - size_t(q.latency())]));
        CHECK(err < 1e-9, "Amount 0: output is the input delayed by exactly latency()");
    }
    { const double sr = 48000; // humanize keeps vibrato, 0 flattens it
        auto run = [&](double hum) { PitchProcessor pp; pp.prepare(sr); PitchParams p; p.amount = 1; p.retuneMs = 0; p.humanize = hum; pp.set(p); Tone t(sr); const size_t N = size_t(sr * 3); std::vector<double> o(N); double l, r, a, b;
            for (size_t i = 0; i < N; ++i) { const double x = t.next(440.0 * std::pow(2.0, 35.0 * std::sin(2 * kPi * 5.0 * i / sr) / 1200.0)); pp.process(x, x, l, r, a, b); o[i] = l; }
            double sp = 0; measure(o, sr, size_t(sr * 1.0), N, &sp); return sp; };
        const double flat = run(0.0), keep = run(1.0); char m[160]; std::snprintf(m, sizeof m, "Humanize: pitch spread %.1f cents at 0 vs %.1f cents at 1", flat, keep);
        CHECK(flat < keep * 0.6 && keep > 25, m); }
    { const double sr = 48000; PitchProcessor pp; pp.prepare(sr); PitchParams p; p.amount = 0; p.octUp = 1.0; p.octDown = 1.0; p.key = 0; p.scale = 1; pp.set(p);
      Tone t(sr); const size_t N = size_t(sr * 2.5); std::vector<double> up(N), dn(N); double l, r, a, b;
      std::vector<double> hl(N), hr2(N);
      for (size_t i = 0; i < N; ++i) { const double x = t.next(220.0); pp.process(x, x, l, r, a, b); hl[i] = a; hr2[i] = b; }
      // octave voices are summed: measure the dominant pitch by tracking the sum
      std::vector<double> sum(N); for (size_t i = 0; i < N; ++i) sum[i] = hl[i];
      double e440 = 0, e110 = 0; { const double w = 2 * kPi; for (size_t i = size_t(sr * 1.2); i < N; ++i) { e440 += sum[i] * std::sin(w * 440.0 * i / sr); e110 += sum[i] * std::sin(w * 110.0 * i / sr); } }
      // use goertzel magnitude instead of a single phase
      auto gz = [&](double f) { double c = 2 * std::cos(2 * kPi * f / sr), s1 = 0, s2 = 0; for (size_t i = size_t(sr * 1.2); i < N; ++i) { const double s = sum[i] + c * s1 - s2; s2 = s1; s1 = s; } return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2); };
      (void) e440; (void) e110;
      const double g110 = gz(110.0), g220 = gz(220.0), g440 = gz(440.0), g330 = gz(330.0);
      char m[160]; std::snprintf(m, sizeof m, "Octave generator adds 110 Hz and 440 Hz (mag 110: %.1f, 440: %.1f, 330: %.1f)", g110, g440, g330);
      CHECK(g110 > 3 * g330 && g440 > 3 * g330, m); }
    { const double sr = 48000; PitchProcessor pp; pp.prepare(sr); PitchParams p; p.amount = 0; p.key = 0; p.scale = 1; p.harm1 = 1; p.harm1Gain = 1.0; pp.set(p);
      Tone t(sr); const size_t N = size_t(sr * 2.5); std::vector<double> hl(N); double l, r, a, b;
      for (size_t i = 0; i < N; ++i) { const double x = t.next(220.0); pp.process(x, x, l, r, a, b); hl[i] = a; }
      const double f = measure(hl, sr, size_t(sr * 1.2), N); const double target = 261.6256;
      char m[160]; std::snprintf(m, sizeof m, "Harmony 3rd up from A3 in C major -> %.1f Hz (C4 = 261.6), %.1f cents off", f, f > 0 ? cents(f, target) : 999.0);
      CHECK(f > 0 && std::abs(cents(f, target)) < 25.0, m); }
    { // formants stay put when the pitch is shifted (the point of PSOLA vs. resampling): source 200 Hz through a 900 Hz resonator
      const double sr = 48000; auto band = [&](const std::vector<double>& x, double f0, double f1) { double e = 0; int cnt = 0; for (double f = f0; f <= f1; f += 15.0) { double c = 2 * std::cos(2 * kPi * f / sr), s1 = 0, s2 = 0; for (size_t i = size_t(sr * 1.2); i < x.size(); ++i) { const double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; } e += s1 * s1 + s2 * s2 - c * s1 * s2; ++cnt; } return e / cnt; };
      auto synth = [&](double f0, size_t N) { std::vector<double> x(N); Biquad a, b; a.set(Biquad::Type::Bell, sr, 900.0, 4.0, 24.0); b.set(Biquad::Type::Bell, sr, 900.0, 4.0, 0.0); double ph = 0; for (size_t i = 0; i < N; ++i) { ph += f0 / sr; double imp = 0; if (ph >= 1.0) { ph -= 1.0; imp = 1.0; } x[i] = a.process(imp) * 0.2; } return x; };
      const size_t N = size_t(sr * 2.5); auto x = synth(200.0, N); PitchProcessor pp; pp.prepare(sr); PitchParams p; p.amount = 1; p.retuneMs = 0; p.humanize = 0; p.key = 3; p.scale = 1; pp.set(p);
      // Force a large correction by choosing a key where 200 Hz (G3-ish, ~55 cents sharp of G3 196) moves down: expect ~ -55 cents only, so also run +4 semitones through a harmony voice
      PitchParams ph; ph.amount = 0; ph.scale = 0; ph.harm1 = 6; ph.harm1Gain = 1.0; PitchProcessor hp; hp.prepare(sr); hp.set(ph); std::vector<double> o(N); double l, r, a, b;
      for (size_t i = 0; i < N; ++i) { hp.process(x[i], x[i], l, r, a, b); o[i] = a; }       // octave up voice: pitch x2, formant should not move to 1800
      double worst = 0; { auto line = [&](const std::vector<double>& v, double f) { double c = 2 * std::cos(2 * kPi * f / sr), s1 = 0, s2 = 0; for (size_t i = size_t(sr * 1.2); i < v.size(); ++i) { const double s = v[i] + c * s1 - s2; s2 = s1; s1 = s; } return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2); };
        for (double f : { 800.0, 1200.0, 1600.0, 2000.0, 2400.0 }) worst = std::max(worst, std::abs(20 * std::log10(line(o, f) / line(x, f)))); }
      char m[160]; std::snprintf(m, sizeof m, "Formants preserved on octave-up: harmonics keep the source spectral envelope (worst deviation %.2f dB)", worst);
      CHECK(worst < 2.0, m); }    { const double sr = 48000; PitchProcessor pp; pp.prepare(sr); PitchParams p; p.amount = 1; p.retuneMs = 5; p.harm1 = 1; p.harm1Gain = 1; p.octUp = 1; pp.set(p);
      std::mt19937 g(9); std::uniform_real_distribution<double> u(-1, 1); bool ok = true; double mx = 0, l, r, a, b;
      for (size_t i = 0; i < size_t(sr * 6); ++i) { const double x = 0.3 * u(g); pp.process(x, x, l, r, a, b); ok &= std::isfinite(l) && std::isfinite(a); mx = std::max({ mx, std::abs(l), std::abs(a) }); }
      CHECK(ok && mx < 2.0, "Pitch stage is finite and bounded on noise with every voice enabled"); }
    std::printf(failures ? "PITCH TESTS FAILED (%d)\n" : "PITCH TESTS PASSED\n", failures); return failures ? 1 : 0;
}
