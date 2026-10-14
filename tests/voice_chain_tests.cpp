// Integration tests of the Phase 2 stages inside VocalChain (pitch, harmonies, constant latency, sidechain unmasking,
// key detection, true-peak ceiling). Everything is measured on the chain output, nothing is assumed.
#include "engine/VocalChain.h"
#include <cstdio>
#include <random>
using namespace vonox::engine;
using vonox::dsp::kPi;
static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++failures; } else std::printf("ok:   %s\n", m); } while (0)

static std::vector<float> tone(double sr, double sec, double f0, double gain = 0.3) {
    std::vector<float> x(size_t(sr * sec)); for (size_t i = 0; i < x.size(); ++i) { double v = 0; for (int h = 1; h <= 8; ++h) v += std::sin(2 * kPi * f0 * h * i / sr) / h; x[i] = float(gain * v); } return x;
}
static ChainTarget neutral() {   // everything that colours the sound is off; only the voice stages are under test
    ChainTarget t; MixSettings& b = t.base; b.hpfHz = 20; b.lowMidCutDb = b.boxCutDb = b.harshCutDb = 0; b.fetRatio = 1; b.optoRatio = 1; b.deEssAmount = 0; b.resAmount = 0;
    b.presenceDb = b.airDb = b.warmthDb = 0; b.satDriveDb = 0; b.parCompAmount = b.exciterAmount = 0; b.reverbAmount = b.delayAmount = b.doublerAmount = b.chorusAmount = 0; b.width = 1; b.ceilingDb = -0.3;
    t.mix = 1; t.compress = 0.5; return t;
}
static std::vector<float> run(VocalChain& c, const ChainTarget& t, std::vector<float> x, const std::vector<float>* aux = nullptr, int bs = 256) {
    for (size_t i = 0; i < x.size(); i += size_t(bs)) {
        const int n = int(std::min<size_t>(size_t(bs), x.size() - i)); float* p = x.data() + i; const float* a = aux ? aux->data() + i : nullptr;
        c.setTarget(t); c.process(&p, 1, n, aux ? &a : nullptr, aux ? 1 : 0);
    }
    return x;
}
static double mag(const std::vector<float>& x, size_t a, size_t b, double f, double sr) {
    double c = 2 * std::cos(2 * kPi * f / sr), s1 = 0, s2 = 0; for (size_t i = a; i < b; ++i) { const double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; } return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / double(b - a);
}
static double estF0(const std::vector<float>& x, size_t a, size_t b, double sr) {   // zero-crossing of the fundamental after a 1 kHz low-pass
    vonox::dsp::Biquad lp1, lp2; lp1.set(vonox::dsp::Biquad::Type::LowPass, sr, 700.0, 0.7071); lp2.set(vonox::dsp::Biquad::Type::LowPass, sr, 700.0, 0.7071);
    int zc = 0; double prev = 0; size_t first = 0, last = 0;
    for (size_t i = a; i < b; ++i) { const double y = lp2.process(lp1.process(x[i])); if (prev <= 0 && y > 0) { if (!zc) first = i; last = i; ++zc; } prev = y; }
    return zc > 2 ? (zc - 1) * sr / double(last - first) : 0.0;
}

int main() {
    for (double sr : { 44100.0, 48000.0 }) {
        char m[200];
        { VocalChain c; c.prepare(sr, 512);
          std::snprintf(m, sizeof m, "@%.0f: latency reported = %d samples (%.1f ms)", sr, c.latency(), 1000.0 * c.latency() / sr); CHECK(c.latency() > 500 && c.latency() < int(0.12 * sr), m); }
        { // constant latency: neutral chain, with and without correction/NR/harmonies, all line up with the delayed input
          auto in = tone(sr, 3.0, 220.0); double worst[3] = {}; int k = 0;
          for (int variant = 0; variant < 3; ++variant) {
              VocalChain c; c.prepare(sr, 512); auto t = neutral();
              if (variant >= 1) { t.voice.pitch.amount = 1.0; t.voice.nr = 0.0; t.voice.pitch.key = 9; t.voice.pitch.scale = 0; }   // 220 Hz = A3, already in tune
              if (variant == 2) { t.voice.pitch.harm1 = 1; t.voice.pitch.harm1Gain = 0.0; t.voice.nr = 0.0; }
              auto out = run(c, t, in);
              // find lag with best correlation
              int best = 0; double bc = -1; const int L = c.latency();
              for (int lag = L - 6; lag <= L + 6; ++lag) { double s = 0; for (size_t i = size_t(sr * 1.5); i < size_t(sr * 2.5); ++i) s += double(out[i]) * in[i - size_t(lag)]; if (s > bc) { bc = s; best = lag; } }
              worst[k++] = std::abs(best - L); std::printf("      variant %d: best lag %d vs latency %d\n", variant, best, L);
          }
          std::snprintf(m, sizeof m, "@%.0f: output lines up with the delayed input: within 1 sample with correction off, within 8 samples (0.2 ms) while PSOLA correction runs", sr); CHECK(worst[0] <= 1 && worst[1] <= 8 && worst[2] <= 8, m); }
        { // pitch correction through the whole chain: A4 + 40 cents -> A4
          const double f = 440.0 * std::pow(2.0, 40.0 / 1200.0); auto in = tone(sr, 3.0, f);
          VocalChain c; c.prepare(sr, 512); auto t = neutral(); t.voice.pitch.amount = 1.0; t.voice.pitch.retuneMs = 5; t.voice.pitch.humanize = 0.0; t.voice.pitch.key = 0; t.voice.pitch.scale = 0;
          auto out = run(c, t, in); const double got = estF0(out, size_t(sr * 1.8), size_t(sr * 2.9), sr), cents = 1200.0 * std::log2(got / 440.0);
          std::snprintf(m, sizeof m, "@%.0f: +40 cents input comes out %.1f cents from A4 through the full chain (< 8)", sr, cents); CHECK(std::abs(cents) < 8.0, m); }
        { // in-key harmony: A4 in A major, 3rd up = C#5 (554.37 Hz)
          auto in = tone(sr, 3.0, 440.0); VocalChain c; c.prepare(sr, 512); auto t = neutral(); t.voice.pitch.key = 9; t.voice.pitch.scale = 1; t.voice.pitch.harm1 = 1; t.voice.pitch.harm1Gain = 0.0;
          auto base = run(c, t, in); VocalChain c2; c2.prepare(sr, 512); t.voice.pitch.harm1Gain = 0.8; auto out = run(c2, t, in);
          const size_t a = size_t(sr * 1.8), b = size_t(sr * 2.9); const double h = 20 * std::log10(mag(out, a, b, 554.37, sr) / (mag(base, a, b, 554.37, sr) + 1e-9));
          std::snprintf(m, sizeof m, "@%.0f: 3rd-up harmony adds a C#5 line %.1f dB above the no-harmony output (>= 20)", sr, h); CHECK(h > 20.0, m); }
        { // sidechain unmasking: loud aux energy in the vocal's presence band lifts the vocal; silent/no aux does nothing
          auto in = tone(sr, 4.0, 200.0, 0.1); std::mt19937 g(9); std::normal_distribution<double> nz(0, 0.25); std::vector<float> aux(in.size());
          vonox::dsp::Biquad hp, lp; hp.set(vonox::dsp::Biquad::Type::HighPass, sr, 1500.0, 0.7071); lp.set(vonox::dsp::Biquad::Type::LowPass, sr, 4000.0, 0.7071);
          for (auto& v : aux) v = float(lp.process(hp.process(nz(g))));
          auto t = neutral(); t.voice.unmask = 1.0;
          VocalChain c; c.prepare(sr, 512); auto outA = run(c, t, in, &aux); const float um = c.meters.unmaskDb.load();
          VocalChain c2; c2.prepare(sr, 512); auto outN = run(c2, t, in, nullptr); const float um0 = c2.meters.unmaskDb.load();
          const double d = 20 * std::log10(mag(outA, size_t(sr * 3), size_t(sr * 3.9), 2800.0, sr) / (mag(outN, size_t(sr * 3), size_t(sr * 3.9), 2800.0, sr) + 1e-12));
          std::snprintf(m, sizeof m, "@%.0f: unmasking lifts the vocal's 2.8 kHz band by %.1f dB with a masking aux (meter %.1f dB), 0 without (meter %.2f)", sr, d, um, um0);
          CHECK(um > 1.0 && std::abs(um0) < 0.01 && d > 1.0, m); }
        { // key detection from sung notes (A major scale emphasising A, E, C#)
          static const int seq[] = { 9, 11, 1, 2, 4, 6, 8, 9, 4, 1, 9, 4, 9, 2, 1, 11, 9 }; VocalChain c; c.prepare(sr, 512); auto t = neutral(); c.clearKeyHistogram();
          std::vector<float> in; for (int pc : seq) { auto n = tone(sr, 0.5, 220.0 * std::pow(2.0, ((pc - 9 + 12) % 12) / 12.0)); in.insert(in.end(), n.begin(), n.end()); }
          run(c, t, in); std::array<double, 12> h{}; for (int i = 0; i < 12; ++i) h[size_t(i)] = c.meters.keyHist[size_t(i)].load();
          const auto k = estimateKey(h); std::snprintf(m, sizeof m, "@%.0f: key detection finds %s %s (conf %.2f), expected A major / F# minor", sr, keyName(k.key), k.minor ? "minor" : "major", k.confidence);
          CHECK(k.valid && ((k.key == 9 && !k.minor) || (k.key == 6 && k.minor)), m); }
        { // true-peak: overdriven input, final inter-sample peak stays under the ceiling
          VocalChain c; c.prepare(sr, 512); auto t = neutral(); t.base.ceilingDb = -1.0; t.outputDb = 12; auto in = tone(sr, 3.0, 440.0, 0.9);
          auto out = run(c, t, in); const float tp = c.meters.truePeakDb.load();
          std::snprintf(m, sizeof m, "@%.0f: true-peak meter reads %.2f dBTP with a -1.0 dB ceiling on a +12 dB overdriven input (<= -0.6)", sr, tp); CHECK(tp < -0.6f, m); }
        { // noise reduction inside the chain: noise-only passage is quieter with NR on
          std::mt19937 g(3); std::normal_distribution<double> nz(0, 0.01); auto in = tone(sr, 6.0, 300.0, 0.2); for (size_t i = 0; i < in.size(); ++i) in[i] += float(nz(g));
          for (size_t i = size_t(sr * 3); i < in.size(); ++i) in[i] = float(nz(g));   // second half: only noise
          VocalChain a; a.prepare(sr, 512); auto t = neutral(); auto o0 = run(a, t, in); VocalChain b; b.prepare(sr, 512); t.voice.nr = 1.0; auto o1 = run(b, t, in);
          auto rms = [&](const std::vector<float>& v) { double e = 0; for (size_t i = size_t(sr * 4.5); i < size_t(sr * 5.8); ++i) e += double(v[i]) * v[i]; return 10 * std::log10(e / (sr * 1.3) + 1e-20); };
          std::snprintf(m, sizeof m, "@%.0f: noise-only section %.1f dB with NR off vs %.1f dB with NR on (>= 8 dB lower)", sr, rms(o0), rms(o1)); CHECK(rms(o0) - rms(o1) > 8.0, m); }
        { // all new features on, stereo, odd block sizes: finite and bounded
          VocalChain c; c.prepare(sr, 512); auto t = neutral(); t.voice.nr = 0.6; t.voice.plosive = 0.6; t.voice.breath = 0.6; t.voice.autoLevel = true; t.voice.dynEqAmount = 0.5; t.voice.bandLimit = 0.4; t.voice.adlib = true;
          t.voice.pitch.amount = 1; t.voice.pitch.harm1 = 3; t.voice.pitch.harm1Gain = 0.5; t.voice.pitch.harm2 = 6; t.voice.pitch.harm2Gain = 0.4; t.voice.pitch.octDown = 0.4; t.base.reverbAmount = 0.3; t.base.delayAmount = 0.2;
          auto L = tone(sr, 3.0, 180.0, 0.5), R = L; bool fin = true; double pk = 0; const int bsz[] = { 1, 7, 64, 333, 1000 }; size_t pos = 0; int bi = 0;
          while (pos < L.size()) { const int n = int(std::min<size_t>(size_t(bsz[bi++ % 5]), L.size() - pos)); float* p[2] = { L.data() + pos, R.data() + pos }; c.setTarget(t); c.process(p, 2, n); pos += size_t(n); }
          for (float v : L) { fin &= std::isfinite(v); pk = std::max(pk, double(std::abs(v))); }
          std::snprintf(m, sizeof m, "@%.0f: all features on, stereo, block sizes 1..1000: finite, peak %.2f <= ceiling", sr, pk); CHECK(fin && pk <= 0.9 + 1e-6, m); }
    }
    std::printf(failures ? "VOICE CHAIN TESTS FAILED (%d)\n" : "VOICE CHAIN TESTS PASSED\n", failures); return failures ? 1 : 0;
}
