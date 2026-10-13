#include "dsp/Dynamics.h"
#include <cstdio>
#include <vector>
#include <random>

using namespace vonox::dsp;
static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } else std::printf("ok:   %s\n", msg); } while (0)

static bool isFiniteVal(double v) { return std::isfinite(v); }

int main() {
    const double sr = 48000.0;

    { // High-pass: -3 dB near cutoff, strong cut far below, flat far above
        Biquad b; b.set(Biquad::Type::HighPass, sr, 100.0, 0.7071);
        CHECK(std::abs(b.magnitudeDb(100.0, sr) + 3.0) < 0.5, "HPF is about -3 dB at cutoff");
        CHECK(b.magnitudeDb(20.0, sr) < -20.0, "HPF attenuates 20 Hz by > 20 dB");
        CHECK(std::abs(b.magnitudeDb(5000.0, sr)) < 0.1, "HPF is flat at 5 kHz");
    }
    { // Bell gain accuracy
        Biquad b; b.set(Biquad::Type::Bell, sr, 3000.0, 1.0, -4.0);
        CHECK(std::abs(b.magnitudeDb(3000.0, sr) + 4.0) < 0.1, "Bell -4 dB gain exact at center");
    }
    { // Compressor reduces a loud sine, leaves quiet one untouched
        Compressor c; c.prepare(sr); c.set(-20.0, 4.0, 5.0, 80.0, 6.0, 0.0);
        double g = 1.0;
        for (int i = 0; i < 48000; ++i) g = c.gainFor(0.5); // ~ -6 dBFS
        CHECK(c.gainReductionDb() > 5.0, "Compressor applies gain reduction on loud signal");
        Compressor q; q.prepare(sr); q.set(-20.0, 4.0, 5.0, 80.0, 6.0, 0.0);
        for (int i = 0; i < 48000; ++i) g = q.gainFor(0.01); // -40 dBFS
        CHECK(q.gainReductionDb() < 0.01, "Compressor leaves quiet signal untouched");
        (void)g;
    }
    { // Limiter never exceeds ceiling, even on extreme input
        Limiter l; l.prepare(sr); l.set(-1.0, 100.0);
        const double ceil = dbToLin(-1.0);
        double peak = 0.0; std::mt19937 rng(1); std::uniform_real_distribution<double> d(-4.0, 4.0);
        for (int i = 0; i < 100000; ++i) peak = std::max(peak, std::abs(l.process(d(rng))));
        CHECK(peak <= ceil + 1e-12, "Limiter output never exceeds -1 dBFS ceiling");
    }
    { // Saturator: no NaN/inf, no DC offset on a symmetric signal
        Saturator s; s.prepare(sr);
        const Saturator::Mode modes[] = { Saturator::Mode::Clean, Saturator::Mode::Warm,
                                          Saturator::Mode::Analog, Saturator::Mode::Aggressive };
        bool ok = true; double dc = 0.0;
        for (auto m : modes) {
            s.set(m, 12.0); s.reset(); dc = 0.0;
            for (int i = 0; i < 96000; ++i) {
                const double y = s.process(0.8 * std::sin(2 * kPi * 220.0 * i / sr));
                if (!isFiniteVal(y)) ok = false;
                if (i >= 48000) dc += y;
            }
            if (std::abs(dc / 48000.0) > 0.01) ok = false;
        }
        CHECK(ok, "Saturator (all modes): finite output, DC offset < 0.01");
    }
    { // De-esser reduces 7 kHz tone more than 200 Hz tone
        auto run = [&](double f) {
            DeEsser d; d.prepare(sr); d.set(6500.0, 1.0, -30.0, 0.8);
            double peak = 0.0;
            for (int i = 0; i < 48000; ++i) {
                const double y = d.process(0.3 * std::sin(2 * kPi * f * i / sr));
                if (i > 24000) peak = std::max(peak, std::abs(y));
            }
            return peak;
        };
        CHECK(run(7000.0) < run(200.0) * 0.8, "De-esser attenuates sibilant band more than low band");
    }
    { // Silence in -> silence out through the whole chain, no NaN
        Compressor c; c.prepare(sr); c.set(-18, 3, 5, 80, 6, 0);
        bool ok = true;
        for (int i = 0; i < 10000; ++i) if (!isFiniteVal(c.gainFor(0.0))) ok = false;
        CHECK(ok, "Silence produces finite gain (no NaN/inf)");
    }

    std::printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
