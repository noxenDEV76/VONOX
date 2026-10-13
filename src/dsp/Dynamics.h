#pragma once
#include "Biquad.h"

namespace vonox::dsp {

inline double dbToLin(double db) { return std::pow(10.0, db / 20.0); }
inline double linToDb(double lin) { return 20.0 * std::log10(std::max(lin, 1e-9)); }

// Feed-forward compressor with soft knee, log-domain detector.
// FET-style: fast attack/release. Opto-style: slow, program-dependent release.
class Compressor {
public:
    void prepare(double sr) { sampleRate = sr; envDb = 0.0; grDb = 0.0; }

    void set(double thresholdDb, double ratio, double attackMs, double releaseMs,
             double kneeDb, double makeupDb) {
        thr = thresholdDb; rat = std::max(1.0, ratio); knee = std::max(0.0, kneeDb);
        makeup = makeupDb;
        atk = std::exp(-1.0 / (0.001 * std::max(0.05, attackMs) * sampleRate));
        rel = std::exp(-1.0 / (0.001 * std::max(1.0, releaseMs) * sampleRate));
    }

    // Process one sample; `detector` is the level source (linked stereo max).
    inline double gainFor(double detector) {
        const double levelDb = linToDb(std::abs(detector));
        const double over = levelDb - thr;
        double target; // desired gain reduction in dB (>= 0)
        if (2.0 * over < -knee) target = 0.0;
        else if (2.0 * std::abs(over) <= knee) {
            const double x = over + knee / 2.0;
            target = (1.0 - 1.0 / rat) * x * x / (2.0 * knee);
        } else target = (1.0 - 1.0 / rat) * over;
        const double coef = (target > grDb) ? atk : rel;
        grDb = coef * grDb + (1.0 - coef) * target;
        return dbToLin(-grDb + makeup);
    }

    double gainReductionDb() const { return grDb; }
    void reset() { grDb = 0.0; }

private:
    double sampleRate = 44100.0, thr = -18.0, rat = 3.0, knee = 6.0, makeup = 0.0;
    double atk = 0.0, rel = 0.0, envDb = 0.0, grDb = 0.0;
};

// De-esser: band-pass detector, wideband gain reduction (smoothed).
class DeEsser {
public:
    void prepare(double sr) { sampleRate = sr; comp.prepare(sr); reset(); }
    // Dynamic-EQ de-esser: narrow band detector -> compressor -> bell cut at the
    // same frequency whose depth follows the detected gain reduction.
    void set(double freqHz, double q, double thresholdDb, double amount) {
        freq = freqHz; bellQ = std::clamp(q, 0.5, 4.0);
        detHp.set(Biquad::Type::HighPass, sampleRate, freqHz * 0.7, 0.7071);
        detLp.set(Biquad::Type::LowPass, sampleRate, freqHz * 1.6, 0.7071);
        amt = std::clamp(amount, 0.0, 1.0);
        comp.set(thresholdDb, 1.0 + 7.0 * amt, 0.5, 40.0, 4.0, 0.0);
    }
    inline double process(double x) {
        const double sib = detLp.process(detHp.process(x));
        comp.gainFor(sib);
        if (++counter >= kUpdateInterval) { // coefficient update every few samples
            counter = 0;
            const double cutDb = -std::min(comp.gainReductionDb() * (0.5 + 0.5 * amt), 18.0);
            bell.set(Biquad::Type::Bell, sampleRate, freq, bellQ, cutDb);
        }
        return bell.process(x);
    }
    double gainReductionDb() const { return comp.gainReductionDb(); }
    void reset() { detHp.reset(); detLp.reset(); bell.reset(); comp.reset(); counter = 0; }
private:
    static constexpr int kUpdateInterval = 8;
    double sampleRate = 44100.0, amt = 0.0, freq = 6500.0, bellQ = 1.5;
    int counter = 0;
    Biquad detHp, detLp, bell;
    Compressor comp;
};

// Lookahead-free soft peak limiter with hard ceiling safety clip.
class Limiter {
public:
    void prepare(double sr) { sampleRate = sr; gain = 1.0; }
    void set(double ceilingDb, double releaseMs) {
        ceil = dbToLin(ceilingDb);
        rel = std::exp(-1.0 / (0.001 * std::max(1.0, releaseMs) * sampleRate));
    }
    inline double process(double x) {
        const double need = (std::abs(x) > ceil) ? ceil / std::abs(x) : 1.0;
        gain = (need < gain) ? need : rel * gain + (1.0 - rel) * need;
        double y = x * gain;
        return std::clamp(y, -ceil, ceil); // guaranteed ceiling
    }
    // Stereo-linked: one gain from the louder channel, so the image never shifts.
    inline void processLinked(double& a, double& b) {
        const double pk = std::max(std::abs(a), std::abs(b));
        const double need = (pk > ceil) ? ceil / pk : 1.0;
        gain = (need < gain) ? need : rel * gain + (1.0 - rel) * need;
        a = std::clamp(a * gain, -ceil, ceil); b = std::clamp(b * gain, -ceil, ceil);
    }
    double gainReductionDb() const { return -linToDb(gain); }
    void reset() { gain = 1.0; }
private:
    double sampleRate = 44100.0, ceil = 0.89, rel = 0.999, gain = 1.0;
};

// Oversampling-free saturator with DC blocker (oversampling added in plugin phase).
class Saturator {
public:
    enum class Mode { Clean, Warm, Analog, Aggressive };
    void prepare(double sr) { dcR = 1.0 - (2.0 * kPi * 10.0 / sr); x1 = y1 = 0.0; }
    void set(Mode m, double driveDb) { mode = m; drive = dbToLin(driveDb); }
    inline double process(double x) {
        const double d = x * drive;
        double s;
        switch (mode) {
        // all curves have unity small-signal gain, so low drive never changes the level
        case Mode::Clean:      s = std::tanh(d * 0.6) / 0.6; break;
        case Mode::Warm:       s = std::tanh(d + 0.1 * d * d) ; break;
        case Mode::Analog:     s = d / (1.0 + std::abs(d)); break;
        case Mode::Aggressive: s = std::clamp(d * 1.5, -1.0, 1.0); s = (s - s * s * s / 3.0) / 1.5; break;
        default: s = d;
        }
        s /= std::max(1.0, drive); // keep level roughly constant
        const double y = s - x1 + dcR * y1; // DC blocker
        x1 = s; y1 = y;
        return y;
    }
    void reset() { x1 = y1 = 0.0; }
private:
    Mode mode = Mode::Clean; double drive = 1.0, dcR = 0.999, x1 = 0.0, y1 = 0.0;
};

} // namespace vonox::dsp
