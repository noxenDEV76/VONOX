#pragma once
#include "VocalAnalyzer.h"

namespace vonox::engine {

enum class Style { CleanStudio, ModernRap, MelodicRap, AggressiveRap, Drill, Trap, RnB, Pop, Custom };

inline const char* styleName(Style s) {
    switch (s) {
    case Style::CleanStudio: return "Clean Studio"; case Style::ModernRap: return "Modern Rap";
    case Style::MelodicRap: return "Melodic Rap";   case Style::AggressiveRap: return "Aggressive Rap";
    case Style::Drill: return "Drill";              case Style::Trap: return "Trap";
    case Style::RnB: return "R&B";                  case Style::Pop: return "Pop";
    default: return "Custom"; }
}

// Every value maps 1:1 onto a DSP module parameter.
struct MixSettings {
    // Subtractive EQ
    double hpfHz = 80, lowMidCutDb = 0, lowMidHz = 300, boxCutDb = 0, harshCutDb = 0, harshHz = 3000;
    // FET peak compressor
    double fetThreshDb = -18, fetRatio = 3.5, fetAttackMs = 5, fetReleaseMs = 80;
    // Opto leveler
    double optoThreshDb = -20, optoRatio = 2.0, optoAttackMs = 15, optoReleaseMs = 300;
    // De-esser
    double deEssHz = 7000, deEssThreshDb = -30, deEssAmount = 0.4;
    // Dynamic resonance control (dynamic band centred on harshHz)
    double resAmount = 0.0, resThreshDb = -30;
    // Additive EQ
    double presenceDb = 0, presenceHz = 3500, airDb = 0, warmthDb = 0;
    // Saturation
    dsp::Saturator::Mode satMode = dsp::Saturator::Mode::Warm; double satDriveDb = 3;
    // Parallel compression, harmonics, space and width
    double parCompAmount = 0.0;                         // 0..1, heavy compressed copy blended in
    double exciterAmount = 0.0;                         // 0..1, harmonic generation on the highs
    int    reverbMode = 1;                              // 0 Room, 1 Plate, 2 Hall, 3 Chamber
    double reverbAmount = 0.0, reverbDecay = 1.0, reverbPreMs = 20, reverbDamp = 0.5, reverbHp = 250, reverbLp = 8500;
    int    delayMode = 0;                               // 0 Stereo (tempo sync), 1 Slapback, 2 Ping-pong
    int    delayDiv = 2;                                // 0 1/16, 1 1/8T, 2 1/8, 3 1/8., 4 1/4, 5 1/4., 6 1/2, 7 free (delayMs)
    double delayAmount = 0.0, delayMs = 250, delayFeedback = 0.3, delayHp = 300, delayLp = 5500;
    double doublerAmount = 0.0, chorusAmount = 0.0;
    double duck = 0.5;                                  // 0..1: how much reverb/delay duck under the dry vocal
    double width = 1.0;                                 // 0..2
    // Output
    double ceilingDb = -1.0, outputTrimDb = 0;
};

// Base character per style: [presence, air, warmth, comp-depth 0..1, drive, deEss]
struct StyleBase { double presence, air, warmth, compDepth, drive, deEss; dsp::Saturator::Mode sat; };

// Space / colour character per style (all values are real DSP parameters)
struct StyleFx {
    double parComp, exciter; int rvMode; double rv, rvDecay, rvDamp; int dlMode, dlDiv; double dl, dlFb; double dbl, chorus, width;
};
inline StyleFx fxFor(Style s) {
    switch (s) {
    case Style::CleanStudio:   return { 0.10, 0.10, 1, 0.12, 0.8, 0.5, 0, 2, 0.00, 0.25, 0.00, 0.00, 1.00 };
    case Style::ModernRap:     return { 0.25, 0.25, 1, 0.12, 0.8, 0.5, 1, 2, 0.12, 0.15, 0.08, 0.00, 1.00 };
    case Style::MelodicRap:    return { 0.20, 0.25, 2, 0.24, 1.0, 0.5, 2, 5, 0.20, 0.35, 0.15, 0.08, 1.15 };
    case Style::AggressiveRap: return { 0.40, 0.35, 0, 0.08, 0.7, 0.6, 1, 2, 0.12, 0.10, 0.06, 0.00, 1.00 };
    case Style::Drill:         return { 0.35, 0.30, 0, 0.12, 0.8, 0.7, 0, 2, 0.08, 0.25, 0.06, 0.00, 1.00 };
    case Style::Trap:          return { 0.30, 0.25, 2, 0.26, 1.2, 0.6, 2, 4, 0.22, 0.38, 0.20, 0.05, 1.15 };
    case Style::RnB:           return { 0.15, 0.20, 1, 0.26, 1.2, 0.4, 0, 3, 0.14, 0.28, 0.12, 0.15, 1.20 };
    case Style::Pop:           return { 0.25, 0.30, 1, 0.20, 1.0, 0.4, 0, 2, 0.12, 0.25, 0.12, 0.10, 1.20 };
    default:                   return { 0.20, 0.20, 1, 0.15, 1.0, 0.5, 0, 2, 0.05, 0.25, 0.05, 0.00, 1.00 };
    }
}

inline StyleBase baseFor(Style s) {
    using M = dsp::Saturator::Mode;
    switch (s) {
    case Style::CleanStudio:   return { 1.5, 1.5, 0.0, 0.45, 1.0, 0.40, M::Clean };
    case Style::ModernRap:     return { 2.5, 1.5, 0.5, 0.65, 3.0, 0.45, M::Warm };
    case Style::MelodicRap:    return { 2.0, 2.5, 1.0, 0.60, 2.5, 0.50, M::Warm };
    case Style::AggressiveRap: return { 3.5, 1.0, 0.0, 0.85, 6.0, 0.40, M::Aggressive };
    case Style::Drill:         return { 3.0, 0.5,-0.5, 0.80, 5.0, 0.35, M::Analog };
    case Style::Trap:          return { 2.0, 1.0, 1.0, 0.70, 4.5, 0.45, M::Analog };
    case Style::RnB:           return { 1.5, 3.0, 1.0, 0.50, 2.0, 0.55, M::Warm };
    case Style::Pop:           return { 2.5, 3.0, 0.0, 0.60, 1.5, 0.55, M::Clean };
    default:                   return { 1.0, 1.0, 0.0, 0.50, 2.0, 0.40, M::Warm };
    }
}

class VocalMixEngine {
public:
    // Deterministic: same profile + style => same settings.
    static MixSettings compute(const VocalProfile& p, Style style) {
        const StyleBase b = baseFor(style);
        MixSettings m;
        m.satMode = b.sat;
        if (!p.valid) { // conservative defaults when analysis is unreliable
            m.hpfHz = 80; m.presenceDb = b.presence * 0.5; m.airDb = b.air * 0.3;
            m.fetThreshDb = -18; m.optoThreshDb = -22; m.deEssAmount = 0.3; m.satDriveDb = 1.0;
            applyFx(m, p, style);
            return m;
        }
        // --- Subtractive EQ: only cut what was actually measured ---
        m.hpfHz = 70.0 + 50.0 * p.rumble;                              // 70..120 Hz
        m.lowMidCutDb = -std::clamp(5.0 * p.lowMidBuildup, 0.0, 4.5);  // never > 4.5 dB
        m.lowMidHz = 300.0;
        m.boxCutDb = -std::clamp(2.0 * p.lowMidBuildup - 0.5, 0.0, 2.0);
        m.harshCutDb = -std::clamp(2.5 * p.brightness * p.sibilance, 0.0, 2.5);

        // --- Compression follows measured dynamics, not just the style ---
        const double dynNeed = std::clamp((p.dynamicRangeDb - 4.0) / 12.0, 0.0, 1.0);
        const double depth = b.compDepth * (0.4 + 0.6 * dynNeed);       // consistent voice => less
        const double ref = p.rmsDb;                                     // thresholds track level
        // Gentle by design: the FET only catches the loudest peaks (2..5 dB below the measured peak, ratio <= 3.6:1)
        m.fetThreshDb = p.peakDb - 2.0 - 3.0 * depth;
        m.fetRatio = 2.0 + 1.6 * depth;
        m.fetAttackMs = 4.0 + 8.0 * (1.0 - depth);
        m.fetReleaseMs = 50.0 + 70.0 * (1.0 - depth);
        m.optoThreshDb = ref - 2.0 - 3.0 * depth * (0.5 + 0.5 * dynNeed);
        m.optoRatio = 1.4 + 0.7 * depth;

        // --- De-esser scales with measured sibilance ---
        m.deEssHz = 6000.0 + 2500.0 * (1.0 - p.brightness);             // brighter voice => lower focus
        m.deEssAmount = std::clamp(b.deEss * (0.4 + 1.2 * p.sibilance), 0.1, 0.9);
        m.deEssThreshDb = p.sibilanceDb + p.rmsDb + 4.0 - 6.0 * p.sibilance;

        // --- Dynamic resonance control: only engages on bright voices, threshold tracks the level ---
        m.harshHz = 2500.0 + 1500.0 * p.brightness;
        m.resAmount = std::clamp(0.25 + 0.6 * p.brightness * (0.4 + 0.6 * p.sibilance), 0.0, 0.8);
        m.resThreshDb = p.rmsDb - 12.0;

        // --- Additive EQ: reduce boosts on bright/harsh/noisy sources ---
        const double brightFactor = 1.0 - 0.85 * p.brightness;
        const double noiseFactor = p.noisy ? 0.3 : 1.0;
        m.presenceDb = b.presence * brightFactor * (1.0 - 0.5 * p.sibilance);
        m.airDb = b.air * (1.0 - p.brightness) * noiseFactor;
        m.warmthDb = b.warmth * (1.0 - p.lowMidBuildup);

        // --- Saturation: less on already dense/clipped material ---
        m.satDriveDb = b.drive * (p.clipped ? 0.2 : 1.0) * (p.crestDb < 10.0 ? 0.6 : 1.0);
        m.ceilingDb = -1.0;
        applyFx(m, p, style);
        return m;
    }

    // Space and colour follow the style AND the measured recording (a noisy or already bright take gets
    // less reverb / exciter; a very dynamic take gets more parallel compression).
    static void applyFx(MixSettings& m, const VocalProfile& p, Style style) {
        const StyleFx f = fxFor(style);
        const bool v = p.valid;
        const double noise = (v && p.noisy) ? 0.5 : 1.0;
        const double bright = v ? p.brightness : 0.5, dyn = v ? std::clamp((p.dynamicRangeDb - 4.0) / 12.0, 0.0, 1.0) : 0.5;
        m.parCompAmount = std::clamp(f.parComp * (0.5 + 0.5 * dyn), 0.0, 0.25);
        m.exciterAmount = std::clamp(f.exciter * (1.3 - 1.0 * bright) * noise, 0.0, 1.0);
        m.reverbMode = f.rvMode; m.reverbAmount = f.rv * noise; m.reverbDecay = f.rvDecay;
        m.reverbDamp = std::clamp(f.rvDamp + 0.3 * bright, 0.0, 0.95);
        m.reverbPreMs = 15 + 25 * (f.rvMode >= 2 ? 1 : 0); m.reverbHp = 250; m.reverbLp = 9500 - 3000 * bright;
        m.delayMode = f.dlMode; m.delayDiv = f.dlDiv; m.delayAmount = f.dl * noise; m.delayFeedback = f.dlFb;
        m.delayMs = (f.dlMode == 1) ? 95 : 250; m.delayHp = 300; m.delayLp = 6500 - 2500 * bright;
        m.doublerAmount = f.dbl * noise; m.chorusAmount = f.chorus; m.width = f.width;
        m.duck = 0.55;
    }


    // REFERENCE TONAL MATCH: moves the tonal controls toward the band balance of a reference recording.
    // Only 4 measured band energies are compared (150-450 Hz, 2-5 kHz, 10-16 kHz, plus level-independent by construction);
    // each correction is limited to +/-3 dB and applied at 80 %, so a reference can never push the chain to extremes.
    // It matches coarse tonal balance only - it is NOT a full spectral match and not a loudness match.
    static void applyReference(MixSettings& m, const VocalProfile& own, const VocalProfile& ref) {
        if (!own.valid || !ref.valid) return;
        auto lim = [](double d) { return std::clamp(d, -3.0, 3.0) * 0.8; };
        m.presenceDb = std::clamp(m.presenceDb + lim(ref.presenceDb - own.presenceDb), 0.0, 8.0);
        m.airDb = std::clamp(m.airDb + lim(ref.airDb - own.airDb), 0.0, 8.0);
        const double low = lim(ref.lowMidDb - own.lowMidDb);        // reference has more low-mids than the input -> less cut / more warmth
        if (low < 0) m.lowMidCutDb = std::clamp(m.lowMidCutDb + low, -8.0, 0.0); else { m.lowMidCutDb = std::clamp(m.lowMidCutDb + low, -8.0, 0.0); m.warmthDb = std::clamp(m.warmthDb + 0.5 * low, -4.0, 6.0); }
        if (low < 0) m.warmthDb = std::clamp(m.warmthDb + 0.5 * low, -4.0, 6.0);
    }

    // MIX AMOUNT (0..1): scales each stage in its own way instead of dry/wet crossfading
    // (a crossfade through a non-linear-phase chain causes comb filtering).
    //  - EQ gains scale linearly (amount * gain), frequencies stay fixed.
    //  - Compression thresholds move up toward peak level; ratio scales toward 1.
    //  - De-esser amount and saturation drive scale linearly.
    static MixSettings scale(const MixSettings& s, double amount, double peakDb) {
        const double a = std::clamp(amount, 0.0, 1.0);
        MixSettings r = s;
        r.lowMidCutDb *= a; r.boxCutDb *= a; r.harshCutDb *= a;
        r.presenceDb *= a; r.airDb *= a; r.warmthDb *= a;
        r.hpfHz = 20.0 + (s.hpfHz - 20.0) * a;
        r.fetRatio = 1.0 + (s.fetRatio - 1.0) * a;
        r.optoRatio = 1.0 + (s.optoRatio - 1.0) * a;
        r.fetThreshDb = peakDb + (s.fetThreshDb - peakDb) * a;
        r.optoThreshDb = peakDb + (s.optoThreshDb - peakDb) * a;
        r.deEssAmount *= a; r.resAmount *= a;
        r.satDriveDb *= a;
        r.parCompAmount *= a; r.exciterAmount *= a; r.reverbAmount *= a; r.delayAmount *= a;
        r.doublerAmount *= a; r.chorusAmount *= a; r.width = 1.0 + (s.width - 1.0) * a;
        return r;
    }
};

} // namespace vonox::engine
