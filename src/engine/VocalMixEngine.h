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
    // Additive EQ
    double presenceDb = 0, presenceHz = 3500, airDb = 0, warmthDb = 0;
    // Saturation
    dsp::Saturator::Mode satMode = dsp::Saturator::Mode::Warm; double satDriveDb = 3;
    // Output
    double ceilingDb = -1.0, outputTrimDb = 0;
};

// Base character per style: [presence, air, warmth, comp-depth 0..1, drive, deEss]
struct StyleBase { double presence, air, warmth, compDepth, drive, deEss; dsp::Saturator::Mode sat; };

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
        m.fetThreshDb = ref + 2.0 - 6.0 * depth;
        m.fetRatio = 2.5 + 2.0 * depth;
        m.fetAttackMs = 4.0 + 8.0 * (1.0 - depth);
        m.fetReleaseMs = 50.0 + 70.0 * (1.0 - depth);
        m.optoThreshDb = ref - 1.0 - 4.0 * depth * (0.5 + 0.5 * dynNeed);
        m.optoRatio = 1.5 + 0.9 * depth;

        // --- De-esser scales with measured sibilance ---
        m.deEssHz = 6000.0 + 2500.0 * (1.0 - p.brightness);             // brighter voice => lower focus
        m.deEssAmount = std::clamp(b.deEss * (0.4 + 1.2 * p.sibilance), 0.1, 0.9);
        m.deEssThreshDb = p.sibilanceDb + p.rmsDb + 4.0 - 6.0 * p.sibilance;

        // --- Additive EQ: reduce boosts on bright/harsh/noisy sources ---
        const double brightFactor = 1.0 - 0.85 * p.brightness;
        const double noiseFactor = p.noisy ? 0.3 : 1.0;
        m.presenceDb = b.presence * brightFactor * (1.0 - 0.5 * p.sibilance);
        m.airDb = b.air * (1.0 - p.brightness) * noiseFactor;
        m.warmthDb = b.warmth * (1.0 - p.lowMidBuildup);

        // --- Saturation: less on already dense/clipped material ---
        m.satDriveDb = b.drive * (p.clipped ? 0.2 : 1.0) * (p.crestDb < 10.0 ? 0.6 : 1.0);
        m.ceilingDb = -1.0;
        return m;
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
        r.deEssAmount *= a;
        r.satDriveDb *= a;
        return r;
    }
};

} // namespace vonox::engine
