#pragma once
// Factory presets. Each preset is a complete configuration: vocal style (drives the analysis-based
// corrective chain), tonal offsets, saturation character, compression macro, MIX AMOUNT, and the
// full effects section (parallel compression, exciter, reverb type/amount, delay type/amount,
// doubler, chorus, width). Differences are measurable in the DSP configuration (see tests).
//
// The "US Rap-inspired" and "French Rap-inspired" groups use DESCRIPTIVE names only. They are original
// tonal directions based on general production tendencies, NOT the artists' private chains, not
// endorsed by anyone, and not validated by listening tests. See docs/PRESETS.md.
#include "engine/VocalMixEngine.h"
#include <vector>
#include <algorithm>

namespace vonox {

using engine::Style;

struct Preset {
    const char* name; const char* category; Style style;
    double presence = 0, air = 0, warmth = 0, drive = 0;   // dB offsets on the style result
    int satMode = -1;                                       // -1 keep the style's character
    double compress = 0.5, mix = 100;                       // macro / MIX AMOUNT in %
    double parComp = 0, exciter = 0;
    int rvMode = 1; double rv = 0, rvDecay = 1.0, rvDamp = 0.5, rvPre = 20;
    int dlMode = 0, dlDiv = 2; double dl = 0, dlFb = 0.3, dlMs = 95;
    double dbl = 0, chorus = 0, width = 1.0, duck = 0.55;
};

inline const std::vector<Preset>& factoryPresets() {
    static const std::vector<Preset> p = {
        { .name = "Clean Studio Rap", .category = "Universal", .style = Style::ModernRap, .air = 0.5, .compress = 0.5, .parComp = 0.15, .exciter = 0.15, .rv = 0.08, .rvDecay = 0.7, .dl = 0.06, .dlFb = 0.2, .width = 1.0 },
        { .name = "Natural Rap", .category = "Universal", .style = Style::ModernRap, .drive = -1, .compress = 0.35, .mix = 80, .exciter = 0.08, .rvMode = 0, .rv = 0.06, .rvDecay = 0.7 },
        { .name = "Modern Radio Rap", .category = "Universal", .style = Style::ModernRap, .presence = 1, .air = 1, .compress = 0.65, .parComp = 0.3, .exciter = 0.3, .rv = 0.12, .dl = 0.08, .dbl = 0.1, .width = 1.1 },
        { .name = "Aggressive Drill", .category = "Universal", .style = Style::Drill, .drive = 1, .satMode = 3, .compress = 0.8, .parComp = 0.45, .exciter = 0.35, .rvMode = 0, .rv = 0.06, .rvDamp = 0.7, .dlMode = 1, .dlDiv = 7, .dl = 0.1, .dlMs = 90 },
        { .name = "UK Drill", .category = "Universal", .style = Style::Drill, .presence = 1, .air = -0.5, .satMode = 2, .compress = 0.75, .parComp = 0.4, .exciter = 0.2, .rvMode = 0, .rv = 0.1, .rvDamp = 0.75, .dlDiv = 0, .dl = 0.06, .dlFb = 0.15, .width = 0.95 },
        { .name = "French Drill", .category = "Universal", .style = Style::Drill, .presence = 0.5, .warmth = 0.5, .compress = 0.75, .parComp = 0.35, .exciter = 0.3, .rv = 0.08, .dlMode = 2, .dlDiv = 2, .dl = 0.08, .dlFb = 0.25 },
        { .name = "Melodic Trap", .category = "Universal", .style = Style::Trap, .compress = 0.55, .exciter = 0.25, .rvMode = 2, .rv = 0.26, .rvDecay = 1.2, .rvPre = 35, .dlMode = 2, .dlDiv = 5, .dl = 0.22, .dlFb = 0.38, .dbl = 0.15, .chorus = 0.06, .width = 1.2 },
        { .name = "Dark Trap", .category = "Universal", .style = Style::Trap, .presence = -1, .air = -1.5, .warmth = 1.5, .drive = 1.5, .satMode = 4, .compress = 0.65, .parComp = 0.3, .exciter = 0.1, .rvMode = 2, .rv = 0.2, .rvDecay = 1.4, .rvDamp = 0.8, .dlDiv = 4, .dl = 0.15, .dlFb = 0.4, .width = 1.1 },
        { .name = "Hyperpop Rap", .category = "Universal", .style = Style::Pop, .presence = 2, .air = 3, .drive = 3, .satMode = 6, .compress = 0.8, .parComp = 0.35, .exciter = 0.6, .rv = 0.2, .dlMode = 2, .dlDiv = 0, .dl = 0.18, .dlFb = 0.3, .dbl = 0.3, .chorus = 0.3, .width = 1.5 },
        { .name = "Underground Rap", .category = "Universal", .style = Style::ModernRap, .presence = -0.5, .air = -2, .warmth = 1, .drive = 3, .satMode = 5, .compress = 0.7, .mix = 90, .parComp = 0.3, .exciter = 0.1, .rvMode = 0, .rv = 0.08, .dlMode = 1, .dlDiv = 7, .dl = 0.08, .dlMs = 95, .width = 0.95 },
        { .name = "Intimate Vocal", .category = "Universal", .style = Style::RnB, .drive = -1.5, .compress = 0.35, .exciter = 0.1, .rv = 0.1, .rvDecay = 0.6, .rvPre = 10 },
        { .name = "Bright Vocal", .category = "Universal", .style = Style::Pop, .presence = 1, .air = 2.5, .compress = 0.55, .exciter = 0.4, .rv = 0.15, .dl = 0.1 },
        { .name = "Warm Vocal", .category = "Universal", .style = Style::RnB, .air = -0.5, .warmth = 2.5, .drive = 0.5, .satMode = 4, .compress = 0.45, .exciter = 0.1, .rv = 0.15, .rvDamp = 0.65, .chorus = 0.05 },
        { .name = "Deep Vocal", .category = "Universal", .style = Style::ModernRap, .presence = -1, .air = -1, .warmth = 3, .satMode = 4, .compress = 0.55, .exciter = 0.05, .rvMode = 2, .rv = 0.15, .rvDamp = 0.75 },
        { .name = "Smooth R&B", .category = "Universal", .style = Style::RnB, .air = 0.5, .compress = 0.45, .exciter = 0.15, .rv = 0.28, .rvDecay = 1.3, .dlDiv = 3, .dl = 0.14, .dbl = 0.12, .chorus = 0.15, .width = 1.25 },
        { .name = "Female Rap Lead", .category = "Universal", .style = Style::ModernRap, .presence = 0.5, .air = 1, .warmth = -0.5, .compress = 0.6, .parComp = 0.2, .exciter = 0.25, .rv = 0.1, .dl = 0.08 },
        { .name = "Female Melodic Lead", .category = "Universal", .style = Style::MelodicRap, .air = 1.5, .compress = 0.5, .exciter = 0.25, .rv = 0.24, .dlMode = 2, .dlDiv = 5, .dl = 0.2, .dlFb = 0.35, .dbl = 0.15, .chorus = 0.1, .width = 1.2 },
        { .name = "Male Rap Lead", .category = "Universal", .style = Style::ModernRap, .presence = 0.5, .warmth = 0.5, .compress = 0.65, .parComp = 0.3, .exciter = 0.2, .rvMode = 0, .rv = 0.06, .dlMode = 1, .dlDiv = 7, .dl = 0.08, .dlMs = 95 },
        { .name = "Male Melodic Lead", .category = "Universal", .style = Style::MelodicRap, .warmth = 0.5, .compress = 0.5, .exciter = 0.2, .rvMode = 2, .rv = 0.22, .dlMode = 2, .dlDiv = 5, .dl = 0.18, .dbl = 0.12, .width = 1.15 },
        { .name = "Wide Chorus Vocal", .category = "Universal", .style = Style::Pop, .compress = 0.55, .exciter = 0.2, .rv = 0.2, .dbl = 0.3, .chorus = 0.35, .width = 1.6 },
        { .name = "Atmospheric Lead", .category = "Universal", .style = Style::MelodicRap, .air = 1.5, .compress = 0.5, .exciter = 0.2, .rvMode = 2, .rv = 0.4, .rvDecay = 1.8, .rvPre = 40, .dlMode = 2, .dlDiv = 5, .dl = 0.3, .dlFb = 0.5, .dbl = 0.2, .chorus = 0.12, .width = 1.4, .duck = 0.7 },
        { .name = "Dry and Intimate", .category = "Universal", .style = Style::CleanStudio, .drive = -1, .compress = 0.4, .exciter = 0.08, .rv = 0, .dl = 0, .width = 1.0 },
        { .name = "Distorted Vocal", .category = "Universal", .style = Style::AggressiveRap, .drive = 8, .satMode = 3, .compress = 0.85, .parComp = 0.5, .exciter = 0.5, .rvMode = 0, .rv = 0.05 },
        { .name = "Vintage Vocal", .category = "Universal", .style = Style::RnB, .presence = -0.5, .air = -3, .warmth = 1.5, .drive = 3, .satMode = 5, .compress = 0.55, .rv = 0.18, .rvDamp = 0.8, .dlMode = 1, .dlDiv = 7, .dl = 0.12, .dlMs = 110 },
        { .name = "Lo-Fi Vocal", .category = "Universal", .style = Style::Trap, .presence = -2, .air = -5, .drive = 4, .satMode = 5, .compress = 0.7, .rvMode = 0, .rv = 0.1, .dlMode = 1, .dlDiv = 7, .dl = 0.1, .dlMs = 95, .chorus = 0.15, .width = 0.9 },
        { .name = "Ad-Lib Wide", .category = "Universal", .style = Style::Pop, .compress = 0.6, .exciter = 0.3, .rvMode = 2, .rv = 0.3, .dlMode = 2, .dlDiv = 2, .dl = 0.3, .dlFb = 0.45, .dbl = 0.35, .chorus = 0.25, .width = 1.8, .duck = 0.6 },
        { .name = "Ad-Lib Aggressive", .category = "Universal", .style = Style::AggressiveRap, .drive = 5, .compress = 0.8, .parComp = 0.5, .exciter = 0.45, .rvMode = 0, .rv = 0.12, .dlMode = 1, .dlDiv = 7, .dl = 0.2, .dlMs = 95, .dbl = 0.2, .width = 1.3 },
        { .name = "Doubled Rap", .category = "Universal", .style = Style::ModernRap, .compress = 0.6, .parComp = 0.3, .exciter = 0.2, .rv = 0.08, .dlMode = 1, .dlDiv = 7, .dl = 0.06, .dlMs = 95, .dbl = 0.45, .width = 1.2 },
        { .name = "Large Hall Vocal", .category = "Universal", .style = Style::MelodicRap, .compress = 0.5, .exciter = 0.15, .rvMode = 2, .rv = 0.45, .rvDecay = 2.0, .rvPre = 35, .dl = 0.05, .width = 1.2 },
        { .name = "Short Plate Vocal", .category = "Universal", .style = Style::ModernRap, .compress = 0.55, .exciter = 0.2, .rv = 0.22, .rvDecay = 0.6, .rvPre = 5 },
        { .name = "Slapback Rap", .category = "Universal", .style = Style::ModernRap, .compress = 0.6, .parComp = 0.25, .exciter = 0.2, .rvMode = 0, .rv = 0.05, .dlMode = 1, .dlDiv = 7, .dl = 0.25, .dlFb = 0.12, .dlMs = 95 },
        { .name = "Tempo-Synced Delay", .category = "Universal", .style = Style::MelodicRap, .compress = 0.5, .exciter = 0.2, .rv = 0.1, .dlDiv = 3, .dl = 0.3, .dlFb = 0.4, .width = 1.1 },
        { .name = "Minimal Processing", .category = "Universal", .style = Style::CleanStudio, .compress = 0.2, .mix = 40, .parComp = 0, .exciter = 0, .rv = 0, .dl = 0 },
        { .name = "Live Performance Vocal", .category = "Universal", .style = Style::ModernRap, .compress = 0.6, .parComp = 0.2, .exciter = 0.15, .rvMode = 0, .rv = 0.12, .rvDecay = 0.8 },
        { .name = "Wide Dark Atmospheric Trap", .category = "US Rap-inspired", .style = Style::Trap, .presence = -0.5, .air = -0.5, .warmth = 1, .drive = 2, .satMode = 4, .compress = 0.7, .parComp = 0.3, .exciter = 0.2, .rvMode = 2, .rv = 0.32, .rvDecay = 1.5, .rvPre = 40, .dlMode = 2, .dlDiv = 5, .dl = 0.3, .dlFb = 0.45, .dbl = 0.2, .width = 1.5, .duck = 0.6 },
        { .name = "Clean Intimate Melodic Rap", .category = "US Rap-inspired", .style = Style::MelodicRap, .drive = -0.5, .compress = 0.45, .exciter = 0.15, .rv = 0.12, .rvDecay = 0.8, .dlDiv = 2, .dl = 0.08, .width = 1.05 },
        { .name = "Dense Dark Melodic Flow", .category = "US Rap-inspired", .style = Style::Trap, .presence = -0.5, .warmth = 1, .drive = 2.5, .satMode = 2, .compress = 0.8, .parComp = 0.4, .exciter = 0.15, .rvMode = 2, .rv = 0.2, .rvDamp = 0.7, .dlMode = 2, .dlDiv = 4, .dl = 0.22, .dlFb = 0.4, .width = 1.3 },
        { .name = "Thin Bright Distorted Rage", .category = "US Rap-inspired", .style = Style::AggressiveRap, .presence = 2, .air = 1, .drive = 6, .satMode = 3, .compress = 0.85, .parComp = 0.5, .exciter = 0.5, .rvMode = 0, .rv = 0.06, .dlMode = 1, .dlDiv = 7, .dl = 0.12, .dlMs = 90, .width = 1.1 },
        { .name = "Airy Melodic Wide Hook", .category = "US Rap-inspired", .style = Style::MelodicRap, .presence = 0.5, .air = 2, .drive = 1, .compress = 0.6, .exciter = 0.3, .rvMode = 2, .rv = 0.3, .rvPre = 30, .dlMode = 2, .dlDiv = 5, .dl = 0.25, .dlFb = 0.4, .dbl = 0.25, .chorus = 0.1, .width = 1.4 },
        { .name = "Dynamic Forward Storyteller", .category = "US Rap-inspired", .style = Style::ModernRap, .presence = 0.5, .drive = -1, .compress = 0.35, .parComp = 0.1, .exciter = 0.12, .rvMode = 0, .rv = 0.05, .dl = 0, .width = 1.0 },
        { .name = "Dry Deep Minimal Menace", .category = "US Rap-inspired", .style = Style::ModernRap, .presence = -0.5, .air = -1.5, .warmth = 2, .drive = 1, .satMode = 4, .compress = 0.7, .parComp = 0.35, .exciter = 0.05, .rvMode = 0, .rv = 0.03, .dl = 0, .width = 0.95 },
        { .name = "Clean Forward Trap Flow", .category = "US Rap-inspired", .style = Style::ModernRap, .presence = 0.5, .compress = 0.6, .parComp = 0.25, .exciter = 0.2, .rv = 0.08, .dlDiv = 2, .dl = 0.07, .width = 1.05 },
        { .name = "Bright Saturated Hyper Melodic", .category = "US Rap-inspired", .style = Style::Pop, .presence = 1.5, .air = 2, .drive = 4, .satMode = 2, .compress = 0.8, .parComp = 0.3, .exciter = 0.45, .rv = 0.2, .dlMode = 2, .dlDiv = 0, .dl = 0.2, .dbl = 0.25, .chorus = 0.15, .width = 1.5 },
        { .name = "Punchy Dense Rapid-Fire", .category = "US Rap-inspired", .style = Style::AggressiveRap, .presence = 1.5, .air = -0.5, .drive = 2, .satMode = 2, .compress = 0.85, .parComp = 0.45, .exciter = 0.3, .rvMode = 0, .rv = 0.03, .dl = 0, .width = 1.0 },
        { .name = "Polished Bright Female Punch", .category = "US Rap-inspired", .style = Style::ModernRap, .presence = 1, .air = 1.5, .compress = 0.7, .parComp = 0.3, .exciter = 0.3, .rv = 0.1, .dl = 0.08, .width = 1.1 },
        { .name = "Dense Forward Female Attitude", .category = "US Rap-inspired", .style = Style::AggressiveRap, .presence = 1, .air = 0.5, .drive = 2, .compress = 0.8, .parComp = 0.4, .exciter = 0.3, .rvMode = 0, .rv = 0.06, .dlDiv = 2, .dl = 0.06, .width = 1.0 },
        { .name = "Smooth Confident Female Rap", .category = "US Rap-inspired", .style = Style::ModernRap, .presence = 0.5, .air = 1, .warmth = 0.5, .compress = 0.6, .parComp = 0.25, .exciter = 0.25, .rv = 0.12, .dl = 0.08, .width = 1.1 },
        { .name = "Airy Playful Pop Rap", .category = "US Rap-inspired", .style = Style::Pop, .presence = 0.5, .air = 2.5, .compress = 0.6, .exciter = 0.35, .rv = 0.18, .dlDiv = 2, .dl = 0.12, .dbl = 0.15, .chorus = 0.12, .width = 1.3 },
        { .name = "Punchy Forward Female Flow", .category = "US Rap-inspired", .style = Style::ModernRap, .presence = 1.5, .compress = 0.7, .parComp = 0.3, .exciter = 0.25, .rvMode = 0, .rv = 0.06, .dl = 0.05, .width = 1.0 },
        { .name = "Dry Close Bright Minimal", .category = "US Rap-inspired", .style = Style::ModernRap, .presence = 1, .air = 1, .compress = 0.55, .parComp = 0.2, .exciter = 0.2, .rvMode = 0, .rv = 0.03, .dl = 0, .width = 0.95 },
        { .name = "Raw Aggressive Female Drill", .category = "US Rap-inspired", .style = Style::Drill, .presence = 1, .drive = 3, .satMode = 3, .compress = 0.8, .parComp = 0.45, .exciter = 0.35, .rvMode = 0, .rv = 0.06, .dlMode = 1, .dlDiv = 7, .dl = 0.1, .dlMs = 90, .width = 1.0 },
        { .name = "Expressive Dynamic Multi-Voice", .category = "US Rap-inspired", .style = Style::MelodicRap, .presence = 0.5, .air = 1, .drive = -0.5, .compress = 0.4, .exciter = 0.2, .rv = 0.2, .rvDecay = 1.2, .dlDiv = 3, .dl = 0.15, .dbl = 0.3, .chorus = 0.1, .width = 1.45 },
        { .name = "Dark Dry Drill Weight", .category = "French Rap-inspired", .style = Style::Drill, .presence = 0.5, .air = -1, .warmth = 1, .drive = 1.5, .satMode = 4, .compress = 0.8, .parComp = 0.4, .exciter = 0.2, .rvMode = 0, .rv = 0.05, .rvDamp = 0.8, .dlMode = 1, .dlDiv = 7, .dl = 0.08, .dlMs = 95, .width = 0.95 },
        { .name = "Polished Clean Melodic Rap FR", .category = "French Rap-inspired", .style = Style::MelodicRap, .presence = 0.5, .air = 0.5, .compress = 0.55, .parComp = 0.2, .exciter = 0.2, .rv = 0.12, .dlDiv = 2, .dl = 0.1, .dbl = 0.08, .width = 1.1 },
        { .name = "Warm Melodic Wide Hook FR", .category = "French Rap-inspired", .style = Style::MelodicRap, .warmth = 1.5, .drive = 1, .satMode = 1, .compress = 0.65, .exciter = 0.2, .rv = 0.22, .rvDecay = 1.1, .dlMode = 2, .dlDiv = 5, .dl = 0.22, .dlFb = 0.38, .dbl = 0.2, .chorus = 0.1, .width = 1.4 },
        { .name = "Dark Cinematic Controlled FR", .category = "French Rap-inspired", .style = Style::Trap, .presence = -0.5, .air = -1, .warmth = 1.5, .satMode = 4, .compress = 0.7, .parComp = 0.3, .exciter = 0.1, .rvMode = 2, .rv = 0.22, .rvDecay = 1.6, .rvDamp = 0.75, .dlDiv = 4, .dl = 0.12, .width = 1.15 },
        { .name = "Smooth Afro Melodic Air", .category = "French Rap-inspired", .style = Style::RnB, .air = 2, .warmth = 0.5, .compress = 0.5, .exciter = 0.3, .rv = 0.2, .dlMode = 2, .dlDiv = 3, .dl = 0.16, .dbl = 0.15, .chorus = 0.1, .width = 1.3 },
        { .name = "Smooth Atmospheric Melodic FR", .category = "French Rap-inspired", .style = Style::MelodicRap, .air = 1, .warmth = 1, .compress = 0.5, .exciter = 0.15, .rvMode = 2, .rv = 0.3, .rvDecay = 1.5, .rvPre = 35, .dlDiv = 5, .dl = 0.2, .dbl = 0.15, .width = 1.3 },
        { .name = "Dense Authority Forward Dry", .category = "French Rap-inspired", .style = Style::ModernRap, .presence = 1, .warmth = 0.5, .drive = 1.5, .satMode = 4, .compress = 0.8, .parComp = 0.4, .exciter = 0.2, .rvMode = 0, .rv = 0.04, .dl = 0, .width = 1.0 },
        { .name = "Punchy Direct Forward Rap", .category = "French Rap-inspired", .style = Style::AggressiveRap, .presence = 1.5, .drive = 1, .compress = 0.75, .parComp = 0.35, .exciter = 0.3, .rvMode = 0, .rv = 0.05, .dlMode = 1, .dlDiv = 7, .dl = 0.06, .dlMs = 95, .width = 1.0 },
        { .name = "Dry Controlled Forward Rap FR", .category = "French Rap-inspired", .style = Style::ModernRap, .presence = 0.5, .air = -0.5, .compress = 0.8, .parComp = 0.35, .exciter = 0.15, .rvMode = 0, .rv = 0.03, .dl = 0, .width = 0.95 },
        { .name = "Deep Dark Cinematic Voice", .category = "French Rap-inspired", .style = Style::Trap, .presence = -1, .air = -1.5, .warmth = 3, .satMode = 4, .compress = 0.6, .exciter = 0.05, .rvMode = 2, .rv = 0.28, .rvDecay = 1.8, .rvDamp = 0.8, .dlDiv = 4, .dl = 0.1, .width = 1.2 },
        { .name = "Clean Melodic Forward FR", .category = "French Rap-inspired", .style = Style::MelodicRap, .presence = 0.5, .air = 1, .compress = 0.55, .exciter = 0.2, .rv = 0.14, .dlDiv = 2, .dl = 0.1, .width = 1.1 },
        { .name = "Raw Bright Aggressive FR", .category = "French Rap-inspired", .style = Style::AggressiveRap, .presence = 2, .air = 0.5, .drive = 3, .satMode = 3, .compress = 0.85, .parComp = 0.5, .exciter = 0.4, .rvMode = 0, .rv = 0.05, .dlMode = 1, .dlDiv = 7, .dl = 0.1, .dlMs = 95, .width = 1.0 },
        { .name = "Bright Smooth Pop Urban", .category = "French Rap-inspired", .style = Style::Pop, .presence = 0.5, .air = 2.5, .warmth = 0.5, .compress = 0.55, .exciter = 0.3, .rv = 0.2, .dlDiv = 2, .dl = 0.12, .dbl = 0.12, .chorus = 0.12, .width = 1.3 },
        { .name = "Hard Forward Female Rap FR", .category = "French Rap-inspired", .style = Style::AggressiveRap, .presence = 1.5, .drive = 2, .compress = 0.8, .parComp = 0.4, .exciter = 0.3, .rvMode = 0, .rv = 0.05, .dl = 0.05, .width = 1.0 },
        { .name = "Smooth Clean Female Melodic", .category = "French Rap-inspired", .style = Style::RnB, .presence = 0.5, .air = 1.5, .compress = 0.5, .exciter = 0.2, .rv = 0.18, .dlDiv = 3, .dl = 0.12, .chorus = 0.1, .width = 1.2 },
        { .name = "Polished Forward Female Flow", .category = "French Rap-inspired", .style = Style::ModernRap, .presence = 1, .air = 1, .compress = 0.65, .parComp = 0.25, .exciter = 0.25, .rv = 0.1, .dl = 0.08, .width = 1.1 },
        { .name = "Bright Melodic Female Hook", .category = "French Rap-inspired", .style = Style::MelodicRap, .presence = 0.5, .air = 2, .compress = 0.55, .exciter = 0.3, .rv = 0.22, .dlMode = 2, .dlDiv = 2, .dl = 0.15, .dbl = 0.2, .chorus = 0.1, .width = 1.35 },
        { .name = "Playful Bright Wide FR", .category = "French Rap-inspired", .style = Style::Pop, .presence = 1, .air = 2, .compress = 0.6, .exciter = 0.35, .rv = 0.18, .dlMode = 2, .dlDiv = 0, .dl = 0.14, .dbl = 0.2, .chorus = 0.2, .width = 1.4 },
        { .name = "Airy Smooth R&B Wide FR", .category = "French Rap-inspired", .style = Style::RnB, .air = 2, .warmth = 0.5, .compress = 0.45, .exciter = 0.25, .rv = 0.28, .rvDecay = 1.3, .dlDiv = 3, .dl = 0.14, .dbl = 0.15, .chorus = 0.15, .width = 1.4 },
        { .name = "Raw Direct Female Drill FR", .category = "French Rap-inspired", .style = Style::Drill, .presence = 1, .drive = 2.5, .satMode = 3, .compress = 0.8, .parComp = 0.4, .exciter = 0.3, .rvMode = 0, .rv = 0.05, .dlMode = 1, .dlDiv = 7, .dl = 0.08, .dlMs = 90, .width = 1.0 },
    };
    return p;
}

// A neutral, plausible vocal used to turn a style/preset into concrete settings before AUTO MIX
// has analysed the user's own voice.
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
    m.parCompAmount = pr.parComp; m.exciterAmount = pr.exciter;
    m.reverbMode = pr.rvMode; m.reverbAmount = pr.rv; m.reverbDecay = pr.rvDecay; m.reverbDamp = pr.rvDamp; m.reverbPreMs = pr.rvPre;
    m.reverbLp = std::clamp(9500.0 + 700.0 * pr.air - 2500.0 * pr.rvDamp, 2500.0, 12000.0);
    m.delayMode = pr.dlMode; m.delayDiv = pr.dlDiv; m.delayAmount = pr.dl; m.delayFeedback = pr.dlFb; m.delayMs = pr.dlMs;
    m.delayLp = std::clamp(6500.0 + 600.0 * pr.air, 2500.0, 11000.0);
    m.doublerAmount = pr.dbl; m.chorusAmount = pr.chorus; m.width = pr.width; m.duck = pr.duck;
    return m;
}

} // namespace vonox
