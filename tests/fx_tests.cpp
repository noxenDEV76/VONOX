#include "dsp/Effects.h"
#include <cstdio>
#include <random>
using namespace vonox::dsp;
static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++failures; } else std::printf("ok:   %s\n", m); } while (0)

static double goertzel(const std::vector<double>& x, double f, double sr) {
    double c = 2 * std::cos(2 * kPi * f / sr), s1 = 0, s2 = 0;
    for (double v : x) { double s = v + c * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / x.size();
}

int main() {
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        // ---- reverb: tail decays, RT60 in a believable range, stable at extremes
        for (auto mode : { Reverb::Mode::Room, Reverb::Mode::Plate, Reverb::Mode::Hall, Reverb::Mode::Chamber }) {
            Reverb rv; Reverb::Params p; p.mode = mode; p.hpHz = 20; p.lpHz = 20000; p.preDelayMs = 0; p.damping = 0.3; rv.set(p); rv.prepare(sr);
            const int N = int(sr * 8); std::vector<double> e(N);
            double l, r; for (int i = 0; i < N; ++i) { rv.process(i == 0 ? 1.0 : 0.0, i == 0 ? 1.0 : 0.0, l, r); e[i] = l * l + r * r; }
            // energy decay curve (Schroeder backward integration)
            std::vector<double> edc(N); double acc = 0; for (int i = N - 1; i >= 0; --i) { acc += e[i]; edc[i] = acc; }
            int t30 = -1, t5 = -1; for (int i = 0; i < N; ++i) { const double db = 10 * std::log10(edc[i] / edc[0] + 1e-30); if (t5 < 0 && db < -5) t5 = i; if (db < -35) { t30 = i; break; } }
            const double rt = (t30 > 0 && t5 > 0) ? 2.0 * (t30 - t5) / sr * (60.0 / 60.0) : -1; // slope over 30 dB -> RT60 extrapolation
            char m[128]; std::snprintf(m, sizeof m, "Reverb mode %d @%.0f Hz: RT60 estimate %.2f s is finite and 0.2..6 s", int(mode), sr, rt);
            CHECK(rt > 0.2 && rt < 6.0, m);
        }
        {   Reverb rv; Reverb::Params p; p.mode = Reverb::Mode::Hall; p.decay = 4.0; p.damping = 0.0; p.diffusion = 1.0; rv.set(p); rv.prepare(sr);
            std::mt19937 g(1); std::uniform_real_distribution<double> u(-1, 1); bool ok = true; double mx = 0, l, r;
            for (int i = 0; i < int(sr * 20); ++i) { rv.process(u(g), u(g), l, r); ok &= std::isfinite(l) && std::isfinite(r); mx = std::max(mx, std::max(std::abs(l), std::abs(r))); }
            CHECK(ok && mx < 60.0, "Reverb stays finite and bounded with max decay and full-scale noise for 20 s"); }
        {   Reverb a, b; Reverb::Params p; a.set(p); b.set(p); a.prepare(sr); b.prepare(sr);
            std::mt19937 g(2); std::uniform_real_distribution<double> u(-1, 1); double l, r, l2, r2, err = 0;
            for (int i = 0; i < 4000; ++i) { const double x = u(g); a.process(x, x, l, r); b.process(x, x, l2, r2); err += std::abs(l - l2) + std::abs(r - r2); }
            CHECK(err == 0.0, "Reverb is deterministic"); }
        // ---- delay timing + feedback
        {   StereoDelay d; StereoDelay::Params p; p.mode = StereoDelay::Mode::Stereo; p.timeMs = 100; p.feedback = 0.5; p.hpHz = 20; p.lpHz = 20000; d.set(p); d.prepare(sr);
            const int N = int(sr * 0.5); std::vector<double> o(N); double l, r;
            for (int i = 0; i < N; ++i) { d.process(i == 0 ? 1.0 : 0.0, i == 0 ? 1.0 : 0.0, l, r); o[i] = l; }
            const int t1 = int(0.1 * sr); int pk = t1; for (int k = t1 - 20; k < t1 + 20; ++k) if (std::abs(o[k]) > std::abs(o[pk])) pk = k;
            char m[128]; std::snprintf(m, sizeof m, "Delay first echo at 100 ms (found %.1f ms) @%.0f", pk * 1000.0 / sr, sr);
            CHECK(std::abs(pk * 1000.0 / sr - 100.0) < 1.0 && std::abs(o[pk]) > 0.2, m);
            const int t2 = 2 * t1; int pk2 = t2; for (int k = t2 - 30; k < t2 + 30; ++k) if (std::abs(o[k]) > std::abs(o[pk2])) pk2 = k;
            CHECK(std::abs(o[pk2]) < std::abs(o[pk]) && std::abs(o[pk2]) > 0.02, "Delay repeats decay (feedback < 1)"); }
        {   StereoDelay d; StereoDelay::Params p; p.mode = StereoDelay::Mode::PingPong; p.timeMs = 80; p.feedback = 0.6; p.hpHz = 20; p.lpHz = 20000; d.set(p); d.prepare(sr);
            const int N = int(sr * 0.5); double el[4] = {}, er[4] = {}, l, r;
            for (int i = 0; i < N; ++i) { d.process(i == 0 ? 1.0 : 0.0, i == 0 ? 1.0 : 0.0, l, r); const int slot = i * 1000 / int(sr) / 80; if (slot < 4) { el[slot] += l * l; er[slot] += r * r; } }
            CHECK(el[1] > er[1] * 4 && er[2] > el[2] * 2, "Ping-pong alternates left/right"); }
        {   StereoDelay d; StereoDelay::Params p; p.mode = StereoDelay::Mode::Stereo; p.feedback = 0.92; d.set(p); d.prepare(sr);
            std::mt19937 g(3); std::uniform_real_distribution<double> u(-1, 1); bool ok = true; double l, r;
            for (int i = 0; i < int(sr * 20); ++i) { d.process(u(g), u(g), l, r); ok &= std::isfinite(l) && std::abs(l) < 20; }
            CHECK(ok, "Delay stable at 92% feedback with noise"); }
        // ---- chorus / doubler decorrelation
        {   Doubler db; db.prepare(sr); std::mt19937 g(4); std::uniform_real_distribution<double> u(-1, 1);
            double sl = 0, sr_ = 0, sd = 0, l, r;
            for (int i = 0; i < int(sr); ++i) { const double x = std::sin(2 * kPi * 220 * i / sr) * 0.5 + 0.1 * u(g); db.process(x, l, r); if (i > 4000) { sl += l * l; sr_ += r * r; sd += l * r; } }
            const double corr = sd / std::sqrt(sl * sr_);
            CHECK(corr < 0.97 && sl > 0.1, "Doubler outputs two decorrelated voices"); }
        // ---- width
        {   Width w; w.prepare(sr); w.set(0.0); double sumSide = 0, l, r;
            for (int i = 0; i < 20000; ++i) { l = std::sin(i * 0.05) * 0.5; r = std::sin(i * 0.05 + 1.0) * 0.5; w.process(l, r); if (i > 5000) sumSide += std::abs(l - r); }
            CHECK(sumSide < 1e-6, "Width 0 collapses to mono");
            Width u; u.prepare(sr); u.set(1.0); double err = 0, l2, r2;
            for (int i = 0; i < 5000; ++i) { l = std::sin(i * 0.05) * 0.5; r = std::sin(i * 0.07) * 0.5; l2 = l; r2 = r; u.process(l2, r2); err += std::abs(l - l2) + std::abs(r - r2); }
            CHECK(err < 1e-9, "Width 1 is transparent");
            Width wd; wd.prepare(sr); wd.set(2.0, 150.0); std::vector<double> sl, sh; double a, b;
            for (int i = 0; i < int(sr); ++i) { a = std::sin(2 * kPi * 60 * i / sr); b = -a; wd.process(a, b); sl.push_back(a - b); }
            for (int i = 0; i < int(sr); ++i) { a = std::sin(2 * kPi * 3000 * i / sr); b = -a; wd.process(a, b); sh.push_back(a - b); }
            const double lowGain = goertzel(sl, 60, sr) / 2.0, highGain = goertzel(sh, 3000, sr) / 2.0;
            CHECK(lowGain < 1.4 && highGain > 1.8, "Width widens highs but keeps lows near-unity (mono-safe bass)"); }
        // ---- exciter makes harmonics, level independent
        for (double lvl : { 0.3, 0.03 }) {
            Exciter ex; ex.prepare(sr); ex.set(0.8, 2500); std::vector<double> y, ref; const double f = 4000;
            for (int i = 0; i < int(sr); ++i) { const double x = lvl * std::sin(2 * kPi * f * i / sr); y.push_back(ex.process(x)); ref.push_back(x); }
            std::vector<double> y2(y.begin() + 4000, y.end()), r2(ref.begin() + 4000, ref.end());
            const double h3 = goertzel(y2, 3 * f, sr) / lvl, h3r = goertzel(r2, 3 * f, sr) / lvl;
            const double h2 = goertzel(y2, 2 * f, sr) / lvl;
            char m[128]; std::snprintf(m, sizeof m, "Exciter adds 2nd/3rd harmonics at level %.2f (h2 %.3f h3 %.3f)", lvl, h2, h3);
            CHECK(h3 > h3r + 0.01 && h2 > 0.01, m);
        }
        std::printf("--- end @%.0f ---\n", sr);
    }
    // saturator new modes unity small-signal + finite
    for (auto m : { Saturator::Mode::Tube, Saturator::Mode::Tape, Saturator::Mode::SoftClip }) {
        Saturator s; s.prepare(48000); s.set(m, 6.0); bool ok = true; double pk = 0;
        for (int i = 0; i < 48000; ++i) { const double y = s.process(0.8 * std::sin(i * 0.05)); ok &= std::isfinite(y); pk = std::max(pk, std::abs(y)); }
        CHECK(ok && pk < 2.0 && pk > 0.2, "Tube/Tape/SoftClip saturators are finite and bounded");
    }
    std::printf(failures ? "FX TESTS FAILED (%d)\n" : "FX TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
