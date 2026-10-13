#pragma once
// Factory presets. Each one is a vocal style + a character offset applied on top of the settings
// the engine computes for that style (so every preset yields genuinely different processing).
// Artist-inspired entries use DESCRIPTIVE names only; the reference direction is documented in
// docs/PRESETS.md as an unofficial tonal reference (no endorsement, no claim of exact chains).
#include "engine/VocalMixEngine.h"
#include <vector>

namespace vonox {

struct Preset {
    const char* name; const char* category;
    engine::Style style;
    double presence, air, warmth, drive;   // dB offsets added to the style result
    double compress, mix;                  // compress macro 0..1, MIX AMOUNT in % (as set by the preset)
    int satMode;                           // -1 = keep the style's own character, else 0..3
};

using engine::Style;
inline const std::vector<Preset>& factoryPresets() {
    static const std::vector<Preset> p = {
        // ---- Universal ----
        { "Clean Studio Vocal",     "Universal", Style::CleanStudio,   0,    0,    0,   0,   0.50, 100, -1 },
        { "Bright Studio Vocal",    "Universal", Style::Pop,           0.5,  1.5,  -0.5, 0,   0.50, 100, -1 },
        { "Dark Studio Vocal",      "Universal", Style::ModernRap,    -1.0, -2.0,  1.0,  0,   0.55, 100, -1 },
        { "Warm Vocal",             "Universal", Style::RnB,           0,    0,    2.0,  0.5, 0.45, 100,  1 },
        { "Aggressive Rap",         "Universal", Style::AggressiveRap, 0,    0,    0,    0,   0.60, 100, -1 },
        { "Modern Drill",           "Universal", Style::Drill,         0,    0,    0,    0,   0.60, 100, -1 },
        { "Melodic Rap",            "Universal", Style::MelodicRap,    0,    0,    0,    0,   0.50, 100, -1 },
        { "Smooth R&B",             "Universal", Style::RnB,           0,    0.5,  0.5, -1.0, 0.45, 100, -1 },
        { "Intimate Vocal",         "Universal", Style::RnB,          -0.5, -0.5,  1.0, -1.5, 0.35, 100,  1 },
        { "Dense Vocal",            "Universal", Style::Trap,          0,    0,    0.5,  2.0, 0.80, 100, -1 },
        { "Natural Vocal",          "Universal", Style::CleanStudio,   0,    0,    0,   -1.0, 0.40,  60, 0 },
        { "Radio-Ready Vocal",      "Universal", Style::Pop,           1.0,  0.5,  0,    1.0, 0.65, 100, -1 },
        { "Lo-Fi Character",        "Universal", Style::Trap,         -1.5, -4.0,  1.0,  4.0, 0.70, 100,  3 },
        { "Heavy Saturation",       "Universal", Style::Drill,         0,   -1.0,  0,    7.0, 0.65, 100,  3 },
        { "Minimal Processing",     "Universal", Style::CleanStudio,   0,    0,    0,   -1.0, 0.25,  35, 0 },
        // ---- US rap / hip-hop inspired (unofficial tonal references) ----
        { "Atmospheric Dark Melodic",    "US Rap-inspired", Style::Trap,         -1.0, -0.5,  1.0,  1.0, 0.60,  95, -1 },
        { "Polished Intimate Melodic",   "US Rap-inspired", Style::MelodicRap,    0,    0.5,  0.5, -0.5, 0.50, 100,  1 },
        { "Dark Saturated Melodic Trap", "US Rap-inspired", Style::Trap,          0,   -0.5,  1.0,  3.0, 0.70, 100,  2 },
        { "Dynamic Intelligible Rap",    "US Rap-inspired", Style::ModernRap,     0.5,  0,    0,   -1.0, 0.40, 100, -1 },
        { "Distorted Stylized Aggressive","US Rap-inspired", Style::AggressiveRap, 0.5, -1.0,  0,    6.0, 0.80, 100,  3 },
        { "Dry Forward Controlled",      "US Rap-inspired", Style::ModernRap,     0.5, -1.0,  0,    0,   0.70, 100, -1 },
        { "Smooth Spacious Melodic",     "US Rap-inspired", Style::MelodicRap,    0,    1.5,  0.5, -0.5, 0.45, 100, -1 },
        { "Bright Rhythmic Forward",     "US Rap-inspired", Style::ModernRap,     1.5,  1.0,  0,    0,   0.60, 100, -1 },
        { "Dark Polished Trap",          "US Rap-inspired", Style::Trap,          0,   -0.5,  0.5,  0,   0.65, 100, -1 },
        // ---- French rap inspired (unofficial tonal references) ----
        { "Aggressive Drill Forward",    "French Rap-inspired", Style::Drill,        0.5, -0.5, 0,   1.0, 0.70, 100, -1 },
        { "Polished Forward Melodic",    "French Rap-inspired", Style::MelodicRap,   1.0,  0.5, 0,   0,   0.55, 100, -1 },
        { "Energetic Direct Raw",        "French Rap-inspired", Style::AggressiveRap,0,    0,   0,   0,   0.60, 100, -1 },
        { "Dark Cinematic Controlled",   "French Rap-inspired", Style::Trap,         0,   -1.0, 0.5, 0.5, 0.65, 100, -1 },
        { "Atmospheric Spacious Melodic","French Rap-inspired", Style::MelodicRap,  -0.5,  1.0, 1.0, -0.5,0.45,  85, -1 },
        { "Bright Smooth Melodic",       "French Rap-inspired", Style::Pop,          0.5,  1.5, 0,  -0.5, 0.50, 100,  1 },
        { "Smooth Intimate Melodic",     "French Rap-inspired", Style::RnB,          0,    0.5, 0.5,-1.0, 0.40, 100,  1 },
        { "Powerful Forward Rap",        "French Rap-inspired", Style::ModernRap,    1.0,  0,   0,   1.0, 0.70, 100, -1 },
        { "Assertive Weighty Rap",       "French Rap-inspired", Style::ModernRap,    0,   -0.5, 1.5, 0.5, 0.65, 100, -1 },
        { "Dark Present Modern Rap",     "French Rap-inspired", Style::ModernRap,    1.0, -1.0, 0.5, 0.5, 0.60, 100, -1 },
    };
    return p;
}

// A neutral, plausible vocal used to turn a style/preset into concrete settings before
// AUTO MIX has analysed the user's own voice.
inline engine::VocalProfile typicalProfile() {
    engine::VocalProfile p; p.valid = true;
    p.peakDb = -6; p.rmsDb = -20; p.crestDb = 14; p.dynamicRangeDb = 10; p.noiseFloorDb = -60;
    p.lowMidBuildup = 0.4; p.sibilance = 0.4; p.brightness = 0.5; p.rumble = 0.2;
    return p;
}

inline engine::MixSettings applyCharacter(engine::MixSettings m, const Preset& pr) {
    m.presenceDb = std::clamp(m.presenceDb + pr.presence, 0.0, 8.0);
    m.airDb = std::clamp(m.airDb + pr.air, 0.0, 8.0);
    m.warmthDb = std::clamp(m.warmthDb + pr.warmth, -4.0, 6.0);
    m.satDriveDb = std::clamp(m.satDriveDb + pr.drive, 0.0, 18.0);
    if (pr.satMode >= 0) m.satMode = static_cast<dsp::Saturator::Mode>(pr.satMode);
    return m;
}

} // namespace vonox
