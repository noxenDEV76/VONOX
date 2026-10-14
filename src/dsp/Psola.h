#pragma once
// Pitch-synchronous overlap-add (time-domain PSOLA style) multi-voice engine.
// One analysis (input ring + pitch-period grid) feeds up to 5 independent output voices, each with its own
// pitch ratio. Voice 0 is stereo (the corrected lead), voices 1..4 are mono-in / panned-stereo-out (harmonies, octaves).
// Grains of 2 periods are copied from the input at the analysis marks and overlap-added at the NEW period
// spacing, so formants stay where they are (the vocal tract is not scaled) and only the pitch moves.
// Fixed latency: latency() samples (the engine always delays by exactly that much).
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
namespace vonox::dsp {

class PsolaEngine {
public:
    static constexpr int kVoices = 5;
    void prepare(double sampleRate) {
        sr = sampleRate; D0 = int(std::ceil(0.030 * sr));
        N = 1; while (N < int(sr * 0.5)) N <<= 1; mask = N - 1;
        for (auto& c : in) c.assign(size_t(N), 0.0);
        for (auto& v : voice) { v.accL.assign(size_t(N), 0.0); v.accR.assign(size_t(N), 0.0); v.w.assign(size_t(N), 0.0); }
        reset();
    }
    void reset() {
        for (auto& c : in) std::fill(c.begin(), c.end(), 0.0);
        for (auto& v : voice) { std::fill(v.accL.begin(), v.accL.end(), 0.0); std::fill(v.accR.begin(), v.accR.end(), 0.0); std::fill(v.w.begin(), v.w.end(), 0.0); v.active = false; v.ratio = 1.0; v.gain = 0.0; v.next = 0.0; }
        n = 0; T0 = 0.008 * sr; anchor = 0.0;
    }
    int latency() const { return D0; }
    // Called whenever the analysis changes. f0 <= 0 means unvoiced (a neutral 8 ms grid is used).
    void setPeriodFromPitch(double f0) {
        const double target = double(n - D0);
        const double newT = (f0 > 0.0) ? std::clamp(sr / f0, sr / 1000.0, sr / 70.0) : 0.008 * sr;
        // keep the mark grid continuous: re-anchor on the mark that precedes "now" in the old grid
        anchor = anchor + std::floor((target - anchor) / T0) * T0; T0 = newT;
    }
    // voice 0: stereo lead; 1..4: harmonies/octaves
    void setVoice(int v, bool active, double ratio, double gain, double panL = 1.0, double panR = 1.0) {
        auto& vo = voice[size_t(v)];
        if (active && !vo.active) vo.next = double(n);
        vo.active = active; vo.ratio = std::clamp(ratio, 0.25, 4.0); vo.gain = gain; vo.pl = panL; vo.pr = panR;
    }
    // Feed one stereo sample. Returns the lead (voice 0) and the sum of the other voices.
    void process(double l, double r, double& leadL, double& leadR, double& harmL, double& harmR, double& delayedL, double& delayedR) {
        in[0][size_t(n & mask)] = l; in[1][size_t(n & mask)] = r;
        for (int vi = 0; vi < kVoices; ++vi) {
            auto& vo = voice[size_t(vi)]; if (!vo.active) continue;
            int guard = 0;
            while (vo.next - std::min(T0, 0.5 * D0 - 2.0) <= double(n) && guard++ < 4) {
                // grain half-length = the longer of input / output period, so the overlapped windows always sum to ~1
                const int h = std::max(8, int(std::min(std::lround(std::max(T0, T0 / vo.ratio)), std::lround(0.5 * D0 - 2.0))));
                const double target = vo.next - D0;
                const double m = anchor + std::floor((target - anchor) / T0) * T0;
                const int64_t mi = int64_t(std::llround(m)), ci = int64_t(std::llround(vo.next));
                for (int k = -h; k <= h; ++k) {
                    const double w = 0.5 * (1.0 + std::cos(3.14159265358979323846 * k / double(h)));
                    int64_t src = mi + k; if (src > n) src = n; if (src < n - N + 2) continue;
                    const size_t di = size_t((ci + k) & mask);
                    if (vi == 0) { vo.accL[di] += w * in[0][size_t(src & mask)]; vo.accR[di] += w * in[1][size_t(src & mask)]; }
                    else { const double mono = 0.5 * (in[0][size_t(src & mask)] + in[1][size_t(src & mask)]); vo.accL[di] += w * mono; }
                    vo.w[di] += w;
                }
                vo.next += T0 / vo.ratio;
            }
        }
        leadL = leadR = harmL = harmR = 0.0;
        const size_t o = size_t(n & mask);
        for (int vi = 0; vi < kVoices; ++vi) {
            auto& vo = voice[size_t(vi)];
            if (vo.active) {
                const double norm = 1.0 / std::max(vo.w[o], 0.25);
                if (vi == 0) { leadL = vo.accL[o] * norm; leadR = vo.accR[o] * norm; }
                else { const double s = vo.accL[o] * norm * vo.gain; harmL += s * vo.pl; harmR += s * vo.pr; }
            }
            vo.accL[o] = 0.0; vo.accR[o] = 0.0; vo.w[o] = 0.0;
        }
        const size_t di = size_t((n - D0) & mask);
        delayedL = in[0][di]; delayedR = in[1][di];
        ++n;
    }
private:
    struct Voice { std::vector<double> accL, accR, w; bool active = false; double ratio = 1, gain = 0, next = 0, pl = 1, pr = 1; };
    double sr = 48000, T0 = 384, anchor = 0; int D0 = 1440, N = 32768, mask = 32767; int64_t n = 0;
    std::array<std::vector<double>, 2> in; std::array<Voice, kVoices> voice;
};

} // namespace vonox::dsp
