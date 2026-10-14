#include "engine/VocalMixEngine.h"
#include <cstdio>
#include <random>

using namespace vonox::engine;
using vonox::dsp::kPi;
static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } else std::printf("ok:   %s\n", msg); } while (0)

static const double SR = 48000.0;

// Deterministic synthetic "voice": harmonic stack with phrase envelope + optional extras.
static std::vector<float> makeVoice(double seconds, double lowMidExtra, double sibLevel,
                                    double gain, double noiseLevel, double dynamicDepth) {
    std::mt19937 rng(42); std::uniform_real_distribution<double> u(-1.0, 1.0);
    const size_t n = static_cast<size_t>(seconds * SR);
    std::vector<float> x(n);
    for (size_t i = 0; i < n; ++i) {
        const double t = i / SR;
        const double phrase = 0.5 + 0.5 * std::sin(2 * kPi * 1.5 * t);                 // syllables
        const double env = (1.0 - dynamicDepth) + dynamicDepth * phrase * phrase;
        double v = 0;
        for (int h = 1; h <= 20; ++h) v += std::sin(2 * kPi * 140.0 * h * t) / (h * 1.2);
        v += lowMidExtra * std::sin(2 * kPi * 300.0 * t);
        const double sibEnv = (std::fmod(t, 0.7) < 0.08) ? 1.0 : 0.0;                    // "s" bursts
        double hp = 0; static double prev = 0; hp = u(rng) - prev; prev = u(rng);          // crude HF noise
        v += sibLevel * sibEnv * hp * 3.0;
        v += noiseLevel * u(rng);
        x[i] = static_cast<float>(gain * env * v);
    }
    return x;
}

int main() {
    // 1. Silence and short audio are rejected with a reason, not guessed at
    {
        std::vector<float> silent(static_cast<size_t>(3 * SR), 0.0f);
        auto p = VocalAnalyzer::analyze(silent, SR);
        CHECK(!p.valid && !p.note.empty(), "Silence: analysis invalid with an explanation");
        std::vector<float> shortBuf(1000, 0.1f);
        CHECK(!VocalAnalyzer::analyze(shortBuf, SR).valid, "Too-short audio: analysis invalid");
        auto s = VocalMixEngine::compute(p, Style::ModernRap);
        CHECK(s.satDriveDb <= 1.0 && s.deEssAmount <= 0.3, "Invalid analysis => conservative defaults");
    }
    // 2. Real measurements: low-mid buildup raises the corrective cut
    auto normal = VocalAnalyzer::analyze(makeVoice(6, 0.0, 0.0, 0.2, 0.0005, 0.6), SR);
    auto muddy  = VocalAnalyzer::analyze(makeVoice(6, 6.0, 0.0, 0.2, 0.0005, 0.6), SR);
    CHECK(normal.valid && muddy.valid, "Synthetic voices analyse successfully");
    CHECK(muddy.lowMidBuildup > normal.lowMidBuildup, "Analyzer measures more low-mid buildup on muddy voice");
    auto sN = VocalMixEngine::compute(normal, Style::ModernRap);
    auto sM = VocalMixEngine::compute(muddy, Style::ModernRap);
    CHECK(sM.lowMidCutDb < sN.lowMidCutDb, "AUTO MIX cuts low-mids more on muddy voice");
    CHECK(sM.lowMidCutDb >= -4.5 && sM.boxCutDb >= -2.0, "Automatic cuts stay within safe limits");

    // 3. Sibilance drives the de-esser
    auto sibilant = VocalAnalyzer::analyze(makeVoice(6, 0.0, 0.5, 0.2, 0.0005, 0.6), SR);
    CHECK(sibilant.sibilance > normal.sibilance, "Analyzer measures more sibilance on sibilant voice");
    CHECK(VocalMixEngine::compute(sibilant, Style::Pop).deEssAmount >
          VocalMixEngine::compute(normal, Style::Pop).deEssAmount, "AUTO MIX raises de-essing on sibilant voice");

    // 4. Clipping is detected and reported, saturation is backed off
    auto clippedAudio = makeVoice(6, 0.0, 0.0, 6.0, 0.0005, 0.6);
    for (auto& v : clippedAudio) v = std::clamp(v, -1.0f, 1.0f);
    auto clipped = VocalAnalyzer::analyze(clippedAudio, SR);
    CHECK(clipped.clipped, "Clipping detected");
    CHECK(clipped.summary().find("CLIPPING") != std::string::npos, "Summary warns about clipping");
    CHECK(VocalMixEngine::compute(clipped, Style::Drill).satDriveDb <
          VocalMixEngine::compute(normal, Style::Drill).satDriveDb, "Saturation reduced on clipped source");

    // 5. Styles produce meaningfully different settings on the same voice
    auto clean = VocalMixEngine::compute(normal, Style::CleanStudio);
    auto aggr  = VocalMixEngine::compute(normal, Style::AggressiveRap);
    CHECK(aggr.fetRatio > clean.fetRatio && aggr.satDriveDb > clean.satDriveDb &&
          aggr.satMode != clean.satMode, "Aggressive Rap differs from Clean Studio (comp, drive, character)");

    // 6. Determinism
    auto a1 = VocalMixEngine::compute(normal, Style::Trap), a2 = VocalMixEngine::compute(normal, Style::Trap);
    CHECK(a1.fetThreshDb == a2.fetThreshDb && a1.presenceDb == a2.presenceDb, "Same input => identical settings");

    // 7. MIX AMOUNT: 0% is neutral, 100% is full, monotonic in between
    auto full = VocalMixEngine::compute(muddy, Style::ModernRap);
    auto zero = VocalMixEngine::scale(full, 0.0, muddy.peakDb);
    auto half = VocalMixEngine::scale(full, 0.5, muddy.peakDb);
    auto one  = VocalMixEngine::scale(full, 1.0, muddy.peakDb);
    CHECK(zero.lowMidCutDb == 0 && zero.fetRatio == 1.0 && zero.satDriveDb == 0, "MIX 0% is neutral (no EQ, ratio 1:1, no drive)");
    CHECK(one.fetRatio == full.fetRatio && one.lowMidCutDb == full.lowMidCutDb, "MIX 100% equals full AUTO MIX result");
    CHECK(half.fetRatio > zero.fetRatio && half.fetRatio < one.fetRatio &&
          half.lowMidCutDb < zero.lowMidCutDb && half.lowMidCutDb > one.lowMidCutDb, "MIX 50% sits between 0% and 100%");

    { // reference tonal match: brighter reference -> more presence/air, limited to +/-3 dB * 0.8; identical reference -> unchanged
        VocalProfile own, ref; own.valid = ref.valid = true; own.presenceDb = -22; own.airDb = -40; own.lowMidDb = -12; ref = own;
        MixSettings base = VocalMixEngine::compute(own, Style::ModernRap), same = base; VocalMixEngine::applyReference(same, own, ref);
        CHECK(same.presenceDb == base.presenceDb && same.airDb == base.airDb && same.lowMidCutDb == base.lowMidCutDb, "Reference identical to the input changes nothing");
        VocalProfile bright = own; bright.presenceDb += 10; bright.airDb += 10; MixSettings m = base; VocalMixEngine::applyReference(m, own, bright);
        CHECK(m.presenceDb >= base.presenceDb && m.presenceDb - base.presenceDb <= 2.41 && m.airDb - base.airDb <= 2.41 && m.presenceDb + m.airDb > base.presenceDb + base.airDb,
              "Brighter reference adds presence/air, each change limited to 2.4 dB");
        VocalProfile boomy = own; boomy.lowMidDb += 8; MixSettings m2 = base; m2.lowMidCutDb = -4; VocalMixEngine::applyReference(m2, own, boomy);
        CHECK(m2.lowMidCutDb > -4.0, "Reference with more low-mids reduces the low-mid cut");
    }
    std::printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
