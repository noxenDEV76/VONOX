#include "engine/VocalChain.h"
#include <cstdio>
#include <random>

using namespace vonox::engine;
using vonox::dsp::kPi;
static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } else std::printf("ok:   %s\n", msg); } while (0)

// Deterministic vocal-like test signal: harmonic stack with syllable envelope + a little noise.
static std::vector<float> voice(double sr, double seconds, double gain) {
    std::mt19937 rng(5); std::uniform_real_distribution<double> u(-1, 1);
    std::vector<float> x(static_cast<size_t>(sr * seconds));
    for (size_t i = 0; i < x.size(); ++i) {
        const double t = i / sr, env = 0.35 + 0.65 * std::pow(0.5 + 0.5 * std::sin(2 * kPi * 2.0 * t), 2.0);
        double v = 0; for (int h = 1; h <= 25; ++h) v += std::sin(2 * kPi * 150.0 * h * t) / (h * 1.1);
        x[i] = float(gain * env * (v * 0.5 + 0.002 * u(rng)));
    }
    return x;
}

static ChainTarget settingsFor(Style style, const std::vector<float>& x, double sr) {
    ChainTarget t; auto prof = VocalAnalyzer::analyze(x, sr);
    t.base = VocalMixEngine::compute(prof, style); return t;
}

// Run `x` (mono) or stereo copy through the chain in blocks of `bs`.
static std::vector<float> run(VocalChain& ch, const ChainTarget& t, std::vector<float> x, int bs) {
    for (size_t i = 0; i < x.size(); i += bs) {
        const int n = int(std::min<size_t>(bs, x.size() - i)); float* p = x.data() + i;
        ch.setTarget(t); ch.process(&p, 1, n);
    }
    return x;
}
static double rmsDb(const std::vector<float>& x, size_t from = 0) {
    double e = 0; for (size_t i = from; i < x.size(); ++i) e += double(x[i]) * x[i];
    return 20 * std::log10(std::sqrt(e / (x.size() - from)) + 1e-12);
}

int main() {
    const double sr = 48000.0;
    auto in = voice(sr, 4.0, 0.4);

    { // 1. Bypass is bit-exact after the 10 ms fade (input delayed by the reported latency, no coloration)
        VocalChain c; c.prepare(sr, 512); auto t = settingsFor(Style::AggressiveRap, in, sr); t.bypass = true;
        auto out = run(c, t, in, 256); bool exact = true;
        const size_t L = size_t(c.latency());
        for (size_t i = 4800 + L; i < in.size(); ++i) if (out[i] != in[i - L]) { exact = false; break; }
        CHECK(exact, "Bypass reproduces the input bit-exactly, delayed by exactly the reported latency (after 10 ms fade)");
    }
    { // 2. Heavy processing: finite, never above the ceiling, on a very loud input
        VocalChain c; c.prepare(sr, 512); auto loud = voice(sr, 4.0, 6.0);
        auto t = settingsFor(Style::AggressiveRap, in, sr); t.base.satDriveDb = 12; t.base.ceilingDb = -1.0; t.outputDb = 12;
        auto out = run(c, t, loud, 480); bool fin = true; double pk = 0;
        for (float v : out) { if (!std::isfinite(v)) fin = false; pk = std::max(pk, double(std::abs(v))); }
        CHECK(fin, "Extreme settings: no NaN / inf");
        CHECK(pk <= std::pow(10.0, -1.0 / 20.0) + 1e-6, "Output never exceeds the -1 dBFS ceiling (6x overdriven input, +12 dB trim)");
    }
    { // 3. Silence in -> silence out
        VocalChain c; c.prepare(sr, 512); std::vector<float> z(48000, 0.f); auto t = settingsFor(Style::Trap, in, sr);
        auto out = run(c, t, z, 512); double m = 0; for (float v : out) m = std::max(m, double(std::abs(v)));
        CHECK(m < 1e-6, "Silence stays silent (no noise or DC added)");
    }
    { // 4. Block size independence
        auto t = settingsFor(Style::ModernRap, in, sr); std::vector<std::vector<float>> outs;
        for (int bs : {32, 480, 1000}) { VocalChain c; c.prepare(sr, 1024); outs.push_back(run(c, t, in, bs)); }
        double d = 0; for (size_t i = 0; i < in.size(); ++i) d = std::max({d, double(std::abs(outs[0][i] - outs[1][i])), double(std::abs(outs[0][i] - outs[2][i]))});
        CHECK(d < 1e-6, "Output is identical for block sizes 32 / 480 / 1000");
    }
    { // 5. Stereo: identical channels in -> identical channels out (no widening / drift)
        VocalChain c; c.prepare(sr, 512); auto t = settingsFor(Style::MelodicRap, in, sr);
        t.base.reverbAmount = t.base.delayAmount = t.base.doublerAmount = t.base.chorusAmount = 0; t.base.width = 1.0;   // stereo effects off
        std::vector<float> L = in, R = in; double d = 0;
        for (size_t i = 0; i < L.size(); i += 256) {
            const int n = int(std::min<size_t>(256, L.size() - i)); float* p[2] = { L.data() + i, R.data() + i };
            c.setTarget(t); c.process(p, 2, n);
        }
        for (size_t i = 0; i < L.size(); ++i) d = std::max(d, double(std::abs(L[i] - R[i])));
        CHECK(d < 1e-6, "Dual-mono stereo input stays perfectly centred (stereo effects off)");
    }
    { // 6. Neutral settings (MIX 0 %, no trim) leave a 1 kHz tone essentially untouched
        VocalChain c; c.prepare(sr, 512); auto t = settingsFor(Style::ModernRap, in, sr); t.mix = 0.0;
        std::vector<float> tone(96000); for (size_t i = 0; i < tone.size(); ++i) tone[i] = float(0.3 * std::sin(2 * kPi * 1000.0 * i / sr));
        auto out = run(c, t, tone, 512);
        CHECK(std::abs(rmsDb(out, 48000) - rmsDb(tone, 48000)) < 0.15, "MIX 0%: 1 kHz level within 0.15 dB of input");
    }
    { // 7. Sample rates
        bool ok = true;
        for (double r : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) {
            VocalChain c; c.prepare(r, 1024); auto v = voice(r, 2.0, 0.5); auto t = settingsFor(Style::Drill, v, r);
            auto out = run(c, t, v, 777);
            for (float s : out) if (!std::isfinite(s) || std::abs(s) > 1.0f) ok = false;
        }
        CHECK(ok, "44.1 / 48 / 88.2 / 96 / 192 kHz: finite and within full scale");
    }
    { // 8. LEVEL MATCH brings the processed loudness close to the dry loudness
        auto longIn = voice(sr, 14.0, 0.4); auto t = settingsFor(Style::AggressiveRap, longIn, sr); t.outputDb = 0;
        VocalChain a; a.prepare(sr, 512); t.levelMatch = false; auto off = run(a, t, longIn, 512);
        VocalChain b; b.prepare(sr, 512); t.levelMatch = true;  auto on = run(b, t, longIn, 512);
        const size_t from = size_t(sr * 10);
        const double dry = rmsDb(longIn, from), eOff = std::abs(rmsDb(off, from) - dry), eOn = std::abs(rmsDb(on, from) - dry);
        std::printf("      level difference vs dry: off %.2f dB, on %.2f dB\n", eOff, eOn);
        CHECK(eOn < 1.5 && eOn <= eOff + 1e-9, "LEVEL MATCH keeps processed loudness within 1.5 dB of dry");
    }
    { // 9. Switching bypass during playback is click-free
        VocalChain c; c.prepare(sr, 512); auto t = settingsFor(Style::ModernRap, in, sr);
        std::vector<float> tone(96000); for (size_t i = 0; i < tone.size(); ++i) tone[i] = float(0.4 * std::sin(2 * kPi * 200.0 * i / sr));
        std::vector<float> out = tone; double worst = 0;
        for (size_t i = 0; i < out.size(); i += 128) {
            t.bypass = ((i / 12000) % 2) == 1; float* p = out.data() + i; c.setTarget(t); c.process(&p, 1, 128);
        }
        for (size_t i = 1; i < out.size(); ++i) worst = std::max(worst, double(std::abs(out[i] - out[i - 1])));
        std::printf("      largest sample step while toggling bypass: %.4f (a clean 200 Hz tone steps at most %.4f)\n", worst, 0.4 * 2 * kPi * 200.0 / sr);
        CHECK(worst < 0.05, "Bypass toggling: no clicks (largest step < 0.05)");
    }
    { // 10. Meters report real measurements
        VocalChain c; c.prepare(sr, 512); auto t = settingsFor(Style::ModernRap, in, sr); run(c, t, in, 512);
        CHECK(c.meters.inPeakDb.load() > -30.f && c.meters.inPeakDb.load() < 0.f && c.meters.outRmsDb.load() > -60.f, "Meters hold plausible measured values");
    }

    { // 11. Effects are audible and stable: reverb/delay tails continue after the input stops, and the dry centre is kept
        VocalChain c; c.prepare(sr, 512); auto t = settingsFor(Style::MelodicRap, in, sr);
        t.base.reverbAmount = 0.5; t.base.delayAmount = 0.4; t.space = 0.5;
        std::vector<float> L = in, R = in; L.resize(L.size() + size_t(sr * 2), 0.f); R = L;
        for (size_t i = 0; i < L.size(); i += 256) { const int n = int(std::min<size_t>(256, L.size() - i)); float* p[2] = { L.data() + i, R.data() + i }; c.setTarget(t); c.process(p, 2, n); }
        double tailE = 0, diff = 0, mid = 0; bool fin = true; const size_t t0 = in.size() + size_t(sr * 0.2);
        for (size_t i = t0; i < t0 + size_t(sr * 1.0); ++i) { tailE += double(L[i]) * L[i]; fin &= std::isfinite(L[i]); }
        for (size_t i = 0; i < in.size(); ++i) { diff += std::abs(double(L[i]) - R[i]); mid += std::abs(0.5 * (double(L[i]) + R[i])); }
        CHECK(fin && tailE > 1e-6, "Reverb + delay leave an audible tail after the input stops");
        CHECK(diff > 0.01 * mid, "Stereo effects create real stereo width (L != R)");
    }
    { // 12. Presets/styles differ measurably in the effects configuration
        auto prof = VocalAnalyzer::analyze(in, sr);
        auto a = VocalMixEngine::compute(prof, Style::CleanStudio), b = VocalMixEngine::compute(prof, Style::Trap);
        CHECK(b.reverbAmount > a.reverbAmount + 0.05 && b.delayAmount > a.delayAmount + 0.05 && b.width > a.width, "Trap has clearly more space than Clean Studio");
    }
    { // 13. SPACE = 0 and MIX = 0 give back the dry vocal (no tails)
        VocalChain c; c.prepare(sr, 512); auto t = settingsFor(Style::Trap, in, sr); t.space = 0.0; t.character = 0.5;
        t.base.doublerAmount = 0; t.base.chorusAmount = 0; t.base.width = 1.0;
        std::vector<float> L = in; L.resize(L.size() + size_t(sr), 0.f); std::vector<float> R = L;
        for (size_t i = 0; i < L.size(); i += 256) { const int n = int(std::min<size_t>(256, L.size() - i)); float* p[2] = { L.data() + i, R.data() + i }; c.setTarget(t); c.process(p, 2, n); }
        double tail = 0; for (size_t i = in.size() + size_t(sr * 0.3); i < L.size(); ++i) tail += double(L[i]) * L[i];
        CHECK(tail < 1e-8, "SPACE = 0 removes reverb and delay completely");
    }
    std::printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
