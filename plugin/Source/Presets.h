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
    const char* artist = nullptr;                           // creative reference shown in the menu (see docs/PRESETS.md disclaimer)
    // voice stages (v0.3): pitch correction / harmonies / octaves / band-limit / noise reduction / ad-lib role
    double tune = 0, retune = 60, humanize = 0.3; int scale = -1;
    int harm1 = 0; double harm1Gain = 0; int harm2 = 0; double harm2Gain = 0; double octDown = 0, octUp = 0;
    double bandLimit = 0, nr = 0; bool adlib = false;
};

inline const char* displayName(const Preset& p) { return p.artist ? p.artist : p.name; }

inline const std::vector<Preset>& factoryPresets() {
    static const std::vector<Preset> p = {
        { .name = "Clean Studio Rap", .category = "Vocal Styles", .style = Style::ModernRap, .air = 0.5, .compress = 0.5, .parComp = 0.15, .exciter = 0.15, .rv = 0.08, .rvDecay = 0.7, .dl = 0.06, .dlFb = 0.2, .width = 1.0 },
        { .name = "Natural Rap", .category = "Vocal Styles", .style = Style::ModernRap, .drive = -1, .compress = 0.35, .mix = 80, .exciter = 0.08, .rvMode = 0, .rv = 0.06, .rvDecay = 0.7 },
        { .name = "Modern Radio Rap", .category = "Vocal Styles", .style = Style::ModernRap, .presence = 1, .air = 1, .compress = 0.65, .parComp = 0.3, .exciter = 0.3, .rv = 0.12, .dl = 0.08, .dbl = 0.1, .width = 1.1 },
        { .name = "Aggressive Drill", .category = "Vocal Styles", .style = Style::Drill, .drive = 1, .satMode = 3, .compress = 0.8, .parComp = 0.45, .exciter = 0.35, .rvMode = 0, .rv = 0.06, .rvDamp = 0.7, .dlMode = 1, .dlDiv = 7, .dl = 0.1, .dlMs = 90 },
        { .name = "UK Drill", .category = "Vocal Styles", .style = Style::Drill, .presence = 1, .air = -0.5, .satMode = 2, .compress = 0.75, .parComp = 0.4, .exciter = 0.2, .rvMode = 0, .rv = 0.1, .rvDamp = 0.75, .dlDiv = 0, .dl = 0.06, .dlFb = 0.15, .width = 0.95 },
        { .name = "French Drill", .category = "Vocal Styles", .style = Style::Drill, .presence = 0.5, .warmth = 0.5, .compress = 0.75, .parComp = 0.35, .exciter = 0.3, .rv = 0.08, .dlMode = 2, .dlDiv = 2, .dl = 0.08, .dlFb = 0.25 },
        { .name = "Melodic Trap", .category = "Vocal Styles", .style = Style::Trap, .compress = 0.55, .exciter = 0.25, .rvMode = 2, .rv = 0.26, .rvDecay = 1.2, .rvPre = 35, .dlMode = 2, .dlDiv = 5, .dl = 0.22, .dlFb = 0.38, .dbl = 0.15, .chorus = 0.06, .width = 1.2 },
        { .name = "Dark Trap", .category = "Vocal Styles", .style = Style::Trap, .presence = -1, .air = -1.5, .warmth = 1.5, .drive = 1.5, .satMode = 4, .compress = 0.65, .parComp = 0.3, .exciter = 0.1, .rvMode = 2, .rv = 0.2, .rvDecay = 1.4, .rvDamp = 0.8, .dlDiv = 4, .dl = 0.15, .dlFb = 0.4, .width = 1.1 },
        { .name = "Hyperpop Rap", .category = "Vocal Styles", .style = Style::Pop, .presence = 2, .air = 3, .drive = 3, .satMode = 6, .compress = 0.8, .parComp = 0.35, .exciter = 0.6, .rv = 0.2, .dlMode = 2, .dlDiv = 0, .dl = 0.18, .dlFb = 0.3, .dbl = 0.3, .chorus = 0.3, .width = 1.5 },
        { .name = "Underground Rap", .category = "Vocal Styles", .style = Style::ModernRap, .presence = -0.5, .air = -2, .warmth = 1, .drive = 3, .satMode = 5, .compress = 0.7, .mix = 90, .parComp = 0.3, .exciter = 0.1, .rvMode = 0, .rv = 0.08, .dlMode = 1, .dlDiv = 7, .dl = 0.08, .dlMs = 95, .width = 0.95 },
        { .name = "Intimate Vocal", .category = "Vocal Styles", .style = Style::RnB, .drive = -1.5, .compress = 0.35, .exciter = 0.1, .rv = 0.1, .rvDecay = 0.6, .rvPre = 10 },
        { .name = "Bright Vocal", .category = "Vocal Styles", .style = Style::Pop, .presence = 1, .air = 2.5, .compress = 0.55, .exciter = 0.4, .rv = 0.15, .dl = 0.1 },
        { .name = "Warm Vocal", .category = "Vocal Styles", .style = Style::RnB, .air = -0.5, .warmth = 2.5, .drive = 0.5, .satMode = 4, .compress = 0.45, .exciter = 0.1, .rv = 0.15, .rvDamp = 0.65, .chorus = 0.05 },
        { .name = "Deep Vocal", .category = "Vocal Styles", .style = Style::ModernRap, .presence = -1, .air = -1, .warmth = 3, .satMode = 4, .compress = 0.55, .exciter = 0.05, .rvMode = 2, .rv = 0.15, .rvDamp = 0.75 },
        { .name = "Smooth R&B", .category = "Vocal Styles", .style = Style::RnB, .air = 0.5, .compress = 0.45, .exciter = 0.15, .rv = 0.28, .rvDecay = 1.3, .dlDiv = 3, .dl = 0.14, .dbl = 0.12, .chorus = 0.15, .width = 1.25 },
        { .name = "Female Rap Lead", .category = "Vocal Styles", .style = Style::ModernRap, .presence = 0.5, .air = 1, .warmth = -0.5, .compress = 0.6, .parComp = 0.2, .exciter = 0.25, .rv = 0.1, .dl = 0.08 },
        { .name = "Female Melodic Lead", .category = "Vocal Styles", .style = Style::MelodicRap, .air = 1.5, .compress = 0.5, .exciter = 0.25, .rv = 0.24, .dlMode = 2, .dlDiv = 5, .dl = 0.2, .dlFb = 0.35, .dbl = 0.15, .chorus = 0.1, .width = 1.2 },
        { .name = "Male Rap Lead", .category = "Vocal Styles", .style = Style::ModernRap, .presence = 0.5, .warmth = 0.5, .compress = 0.65, .parComp = 0.3, .exciter = 0.2, .rvMode = 0, .rv = 0.06, .dlMode = 1, .dlDiv = 7, .dl = 0.08, .dlMs = 95 },
        { .name = "Male Melodic Lead", .category = "Vocal Styles", .style = Style::MelodicRap, .warmth = 0.5, .compress = 0.5, .exciter = 0.2, .rvMode = 2, .rv = 0.22, .dlMode = 2, .dlDiv = 5, .dl = 0.18, .dbl = 0.12, .width = 1.15 },
        { .name = "Wide Chorus Vocal", .category = "Vocal Styles", .style = Style::Pop, .compress = 0.55, .exciter = 0.2, .rv = 0.2, .dbl = 0.3, .chorus = 0.35, .width = 1.6 },
        { .name = "Atmospheric Lead", .category = "Vocal Styles", .style = Style::MelodicRap, .air = 1.5, .compress = 0.5, .exciter = 0.2, .rvMode = 2, .rv = 0.4, .rvDecay = 1.8, .rvPre = 40, .dlMode = 2, .dlDiv = 5, .dl = 0.3, .dlFb = 0.5, .dbl = 0.2, .chorus = 0.12, .width = 1.4, .duck = 0.7 },
        { .name = "Dry and Intimate", .category = "Vocal Styles", .style = Style::CleanStudio, .drive = -1, .compress = 0.4, .exciter = 0.08, .rv = 0, .dl = 0, .width = 1.0 },
        { .name = "Distorted Vocal", .category = "Vocal Styles", .style = Style::AggressiveRap, .drive = 8, .satMode = 3, .compress = 0.85, .parComp = 0.5, .exciter = 0.5, .rvMode = 0, .rv = 0.05 },
        { .name = "Vintage Vocal", .category = "Vocal Styles", .style = Style::RnB, .presence = -0.5, .air = -3, .warmth = 1.5, .drive = 3, .satMode = 5, .compress = 0.55, .rv = 0.18, .rvDamp = 0.8, .dlMode = 1, .dlDiv = 7, .dl = 0.12, .dlMs = 110 },
        { .name = "Lo-Fi Vocal", .category = "Vocal Styles", .style = Style::Trap, .presence = -2, .air = -5, .drive = 4, .satMode = 5, .compress = 0.7, .rvMode = 0, .rv = 0.1, .dlMode = 1, .dlDiv = 7, .dl = 0.1, .dlMs = 95, .chorus = 0.15, .width = 0.9 },
        { .name = "Ad-Lib Wide", .category = "Vocal Styles", .style = Style::Pop, .compress = 0.6, .exciter = 0.3, .rvMode = 2, .rv = 0.3, .dlMode = 2, .dlDiv = 2, .dl = 0.3, .dlFb = 0.45, .dbl = 0.35, .chorus = 0.25, .width = 1.8, .duck = 0.6 },
        { .name = "Ad-Lib Aggressive", .category = "Vocal Styles", .style = Style::AggressiveRap, .drive = 5, .compress = 0.8, .parComp = 0.5, .exciter = 0.45, .rvMode = 0, .rv = 0.12, .dlMode = 1, .dlDiv = 7, .dl = 0.2, .dlMs = 95, .dbl = 0.2, .width = 1.3 },
        { .name = "Doubled Rap", .category = "Vocal Styles", .style = Style::ModernRap, .compress = 0.6, .parComp = 0.3, .exciter = 0.2, .rv = 0.08, .dlMode = 1, .dlDiv = 7, .dl = 0.06, .dlMs = 95, .dbl = 0.45, .width = 1.2 },
        { .name = "Large Hall Vocal", .category = "Vocal Styles", .style = Style::MelodicRap, .compress = 0.5, .exciter = 0.15, .rvMode = 2, .rv = 0.45, .rvDecay = 2.0, .rvPre = 35, .dl = 0.05, .width = 1.2 },
        { .name = "Short Plate Vocal", .category = "Vocal Styles", .style = Style::ModernRap, .compress = 0.55, .exciter = 0.2, .rv = 0.22, .rvDecay = 0.6, .rvPre = 5 },
        { .name = "Slapback Rap", .category = "Vocal Styles", .style = Style::ModernRap, .compress = 0.6, .parComp = 0.25, .exciter = 0.2, .rvMode = 0, .rv = 0.05, .dlMode = 1, .dlDiv = 7, .dl = 0.25, .dlFb = 0.12, .dlMs = 95 },
        { .name = "Tempo-Synced Delay", .category = "Vocal Styles", .style = Style::MelodicRap, .compress = 0.5, .exciter = 0.2, .rv = 0.1, .dlDiv = 3, .dl = 0.3, .dlFb = 0.4, .width = 1.1 },
        { .name = "Minimal Processing", .category = "Vocal Styles", .style = Style::CleanStudio, .compress = 0.2, .mix = 40, .parComp = 0, .exciter = 0, .rv = 0, .dl = 0 },
        { .name = "Live Performance Vocal", .category = "Vocal Styles", .style = Style::ModernRap, .compress = 0.6, .parComp = 0.2, .exciter = 0.15, .rvMode = 0, .rv = 0.12, .rvDecay = 0.8 },
        { .name = "Wide Dark Atmospheric Trap", .category = "US Rap", .style = Style::Trap, .presence = -0.5, .air = -0.5, .warmth = 1, .drive = 2, .satMode = 4, .compress = 0.7, .parComp = 0.3, .exciter = 0.2, .rvMode = 2, .rv = 0.32, .rvDecay = 1.5, .rvPre = 40, .dlMode = 2, .dlDiv = 5, .dl = 0.3, .dlFb = 0.45, .dbl = 0.2, .width = 1.5, .duck = 0.6, .artist = "Travis Scott" },
        { .name = "Clean Intimate Melodic Rap", .category = "US Rap", .style = Style::MelodicRap, .drive = -0.5, .compress = 0.45, .exciter = 0.15, .rv = 0.12, .rvDecay = 0.8, .dlDiv = 2, .dl = 0.08, .width = 1.05, .artist = "Drake" },
        { .name = "Dense Dark Melodic Flow", .category = "US Rap", .style = Style::Trap, .presence = -0.5, .warmth = 1, .drive = 2.5, .satMode = 2, .compress = 0.8, .parComp = 0.4, .exciter = 0.15, .rvMode = 2, .rv = 0.2, .rvDamp = 0.7, .dlMode = 2, .dlDiv = 4, .dl = 0.22, .dlFb = 0.4, .width = 1.3, .artist = "Future" },
        { .name = "Thin Bright Distorted Rage", .category = "US Rap", .style = Style::AggressiveRap, .presence = 2, .air = 1, .drive = 6, .satMode = 3, .compress = 0.85, .parComp = 0.5, .exciter = 0.5, .rvMode = 0, .rv = 0.06, .dlMode = 1, .dlDiv = 7, .dl = 0.12, .dlMs = 90, .width = 1.1, .artist = "Playboi Carti" },
        { .name = "Airy Melodic Wide Hook", .category = "US Rap", .style = Style::MelodicRap, .presence = 0.5, .air = 2, .drive = 1, .compress = 0.6, .exciter = 0.3, .rvMode = 2, .rv = 0.3, .rvPre = 30, .dlMode = 2, .dlDiv = 5, .dl = 0.25, .dlFb = 0.4, .dbl = 0.25, .chorus = 0.1, .width = 1.4, .artist = "Don Toliver" },
        { .name = "Dynamic Forward Storyteller", .category = "US Rap", .style = Style::ModernRap, .presence = 0.5, .drive = -1, .compress = 0.35, .parComp = 0.1, .exciter = 0.12, .rvMode = 0, .rv = 0.05, .dl = 0, .width = 1.0, .artist = "Kendrick Lamar" },
        { .name = "Dry Deep Minimal Menace", .category = "US Rap", .style = Style::ModernRap, .presence = -0.5, .air = -1.5, .warmth = 2, .drive = 1, .satMode = 4, .compress = 0.7, .parComp = 0.35, .exciter = 0.05, .rvMode = 0, .rv = 0.03, .dl = 0, .width = 0.95, .artist = "21 Savage" },
        { .name = "Clean Forward Trap Flow", .category = "US Rap", .style = Style::ModernRap, .presence = 0.5, .compress = 0.6, .parComp = 0.25, .exciter = 0.2, .rv = 0.08, .dlDiv = 2, .dl = 0.07, .width = 1.05, .artist = "Lil Baby" },
        { .name = "Bright Saturated Hyper Melodic", .category = "US Rap", .style = Style::Pop, .presence = 1.5, .air = 2, .drive = 4, .satMode = 2, .compress = 0.8, .parComp = 0.3, .exciter = 0.45, .rv = 0.2, .dlMode = 2, .dlDiv = 0, .dl = 0.2, .dbl = 0.25, .chorus = 0.15, .width = 1.5, .artist = "Lil Uzi Vert" },
        { .name = "Punchy Dense Rapid-Fire", .category = "US Rap", .style = Style::AggressiveRap, .presence = 1.5, .air = -0.5, .drive = 2, .satMode = 2, .compress = 0.85, .parComp = 0.45, .exciter = 0.3, .rvMode = 0, .rv = 0.03, .dl = 0, .width = 1.0, .artist = "Eminem" },
        { .name = "Polished Bright Female Punch", .category = "US Female Rap", .style = Style::ModernRap, .presence = 1, .air = 1.5, .compress = 0.7, .parComp = 0.3, .exciter = 0.3, .rv = 0.1, .dl = 0.08, .width = 1.1, .artist = "Nicki Minaj" },
        { .name = "Dense Forward Female Attitude", .category = "US Female Rap", .style = Style::AggressiveRap, .presence = 1, .air = 0.5, .drive = 2, .compress = 0.8, .parComp = 0.4, .exciter = 0.3, .rvMode = 0, .rv = 0.06, .dlDiv = 2, .dl = 0.06, .width = 1.0, .artist = "Cardi B" },
        { .name = "Smooth Confident Female Rap", .category = "US Female Rap", .style = Style::ModernRap, .presence = 0.5, .air = 1, .warmth = 0.5, .compress = 0.6, .parComp = 0.25, .exciter = 0.25, .rv = 0.12, .dl = 0.08, .width = 1.1, .artist = "Megan Thee Stallion" },
        { .name = "Airy Playful Pop Rap", .category = "US Female Rap", .style = Style::Pop, .presence = 0.5, .air = 2.5, .compress = 0.6, .exciter = 0.35, .rv = 0.18, .dlDiv = 2, .dl = 0.12, .dbl = 0.15, .chorus = 0.12, .width = 1.3, .artist = "Doja Cat" },
        { .name = "Punchy Forward Female Flow", .category = "US Female Rap", .style = Style::ModernRap, .presence = 1.5, .compress = 0.7, .parComp = 0.3, .exciter = 0.25, .rvMode = 0, .rv = 0.06, .dl = 0.05, .width = 1.0, .artist = "Latto" },
        { .name = "Dry Close Bright Minimal", .category = "US Female Rap", .style = Style::ModernRap, .presence = 1, .air = 1, .compress = 0.55, .parComp = 0.2, .exciter = 0.2, .rvMode = 0, .rv = 0.03, .dl = 0, .width = 0.95, .artist = "Ice Spice" },
        { .name = "Raw Aggressive Female Drill", .category = "US Female Rap", .style = Style::Drill, .presence = 1, .drive = 3, .satMode = 3, .compress = 0.8, .parComp = 0.45, .exciter = 0.35, .rvMode = 0, .rv = 0.06, .dlMode = 1, .dlDiv = 7, .dl = 0.1, .dlMs = 90, .width = 1.0, .artist = "Lola Brooke" },
        { .name = "Expressive Dynamic Multi-Voice", .category = "US Female Rap", .style = Style::MelodicRap, .presence = 0.5, .air = 1, .drive = -0.5, .compress = 0.4, .exciter = 0.2, .rv = 0.2, .rvDecay = 1.2, .dlDiv = 3, .dl = 0.15, .dbl = 0.3, .chorus = 0.1, .width = 1.45, .artist = "Doechii" },
        { .name = "Dark Dry Drill Weight", .category = "French Rap", .style = Style::Drill, .presence = 0.5, .air = -1, .warmth = 1, .drive = 1.5, .satMode = 4, .compress = 0.8, .parComp = 0.4, .exciter = 0.2, .rvMode = 0, .rv = 0.05, .rvDamp = 0.8, .dlMode = 1, .dlDiv = 7, .dl = 0.08, .dlMs = 95, .width = 0.95, .artist = "Gazo" },
        { .name = "Polished Clean Melodic Rap FR", .category = "French Rap", .style = Style::MelodicRap, .presence = 0.5, .air = 0.5, .compress = 0.55, .parComp = 0.2, .exciter = 0.2, .rv = 0.12, .dlDiv = 2, .dl = 0.1, .dbl = 0.08, .width = 1.1, .artist = "Ninho" },
        { .name = "Warm Melodic Wide Hook FR", .category = "French Rap", .style = Style::MelodicRap, .warmth = 1.5, .drive = 1, .satMode = 1, .compress = 0.65, .exciter = 0.2, .rv = 0.22, .rvDecay = 1.1, .dlMode = 2, .dlDiv = 5, .dl = 0.22, .dlFb = 0.38, .dbl = 0.2, .chorus = 0.1, .width = 1.4, .artist = "Jul" },
        { .name = "Dark Cinematic Controlled FR", .category = "French Rap", .style = Style::Trap, .presence = -0.5, .air = -1, .warmth = 1.5, .satMode = 4, .compress = 0.7, .parComp = 0.3, .exciter = 0.1, .rvMode = 2, .rv = 0.22, .rvDecay = 1.6, .rvDamp = 0.75, .dlDiv = 4, .dl = 0.12, .width = 1.15, .artist = "SCH" },
        { .name = "Smooth Afro Melodic Air", .category = "French Rap", .style = Style::RnB, .air = 2, .warmth = 0.5, .compress = 0.5, .exciter = 0.3, .rv = 0.2, .dlMode = 2, .dlDiv = 3, .dl = 0.16, .dbl = 0.15, .chorus = 0.1, .width = 1.3, .artist = "Tiakola" },
        { .name = "Smooth Atmospheric Melodic FR", .category = "French Rap", .style = Style::MelodicRap, .air = 1, .warmth = 1, .compress = 0.5, .exciter = 0.15, .rvMode = 2, .rv = 0.3, .rvDecay = 1.5, .rvPre = 35, .dlDiv = 5, .dl = 0.2, .dbl = 0.15, .width = 1.3, .artist = "Hamza" },
        { .name = "Dense Authority Forward Dry", .category = "French Rap", .style = Style::ModernRap, .presence = 1, .warmth = 0.5, .drive = 1.5, .satMode = 4, .compress = 0.8, .parComp = 0.4, .exciter = 0.2, .rvMode = 0, .rv = 0.04, .dl = 0, .width = 1.0, .artist = "Booba" },
        { .name = "Punchy Direct Forward Rap", .category = "French Rap", .style = Style::AggressiveRap, .presence = 1.5, .drive = 1, .compress = 0.75, .parComp = 0.35, .exciter = 0.3, .rvMode = 0, .rv = 0.05, .dlMode = 1, .dlDiv = 7, .dl = 0.06, .dlMs = 95, .width = 1.0, .artist = "SDM" },
        { .name = "Dry Controlled Forward Rap FR", .category = "French Rap", .style = Style::ModernRap, .presence = 0.5, .air = -0.5, .compress = 0.8, .parComp = 0.35, .exciter = 0.15, .rvMode = 0, .rv = 0.03, .dl = 0, .width = 0.95, .artist = "Werenoi" },
        { .name = "Deep Dark Cinematic Voice", .category = "French Rap", .style = Style::Trap, .presence = -1, .air = -1.5, .warmth = 3, .satMode = 4, .compress = 0.6, .exciter = 0.05, .rvMode = 2, .rv = 0.28, .rvDecay = 1.8, .rvDamp = 0.8, .dlDiv = 4, .dl = 0.1, .width = 1.2, .artist = "Damso" },
        { .name = "Clean Melodic Forward FR", .category = "French Rap", .style = Style::MelodicRap, .presence = 0.5, .air = 1, .compress = 0.55, .exciter = 0.2, .rv = 0.14, .dlDiv = 2, .dl = 0.1, .width = 1.1, .artist = "PLK" },
        { .name = "Raw Bright Aggressive FR", .category = "French Rap", .style = Style::AggressiveRap, .presence = 2, .air = 0.5, .drive = 3, .satMode = 3, .compress = 0.85, .parComp = 0.5, .exciter = 0.4, .rvMode = 0, .rv = 0.05, .dlMode = 1, .dlDiv = 7, .dl = 0.1, .dlMs = 95, .width = 1.0, .artist = "La Fève" },
        { .name = "Bright Smooth Pop Urban", .category = "French Female", .style = Style::Pop, .presence = 0.5, .air = 2.5, .warmth = 0.5, .compress = 0.55, .exciter = 0.3, .rv = 0.2, .dlDiv = 2, .dl = 0.12, .dbl = 0.12, .chorus = 0.12, .width = 1.3, .artist = "Aya Nakamura" },
        { .name = "Hard Forward Female Rap FR", .category = "French Female", .style = Style::AggressiveRap, .presence = 1.5, .drive = 2, .compress = 0.8, .parComp = 0.4, .exciter = 0.3, .rvMode = 0, .rv = 0.05, .dl = 0.05, .width = 1.0, .artist = "Shay" },
        { .name = "Smooth Clean Female Melodic", .category = "French Female", .style = Style::RnB, .presence = 0.5, .air = 1.5, .compress = 0.5, .exciter = 0.2, .rv = 0.18, .dlDiv = 3, .dl = 0.12, .chorus = 0.1, .width = 1.2, .artist = "Meryl" },
        { .name = "Polished Forward Female Flow", .category = "French Female", .style = Style::ModernRap, .presence = 1, .air = 1, .compress = 0.65, .parComp = 0.25, .exciter = 0.25, .rv = 0.1, .dl = 0.08, .width = 1.1, .artist = "Chilla" },
        { .name = "Bright Melodic Female Hook", .category = "French Female", .style = Style::MelodicRap, .presence = 0.5, .air = 2, .compress = 0.55, .exciter = 0.3, .rv = 0.22, .dlMode = 2, .dlDiv = 2, .dl = 0.15, .dbl = 0.2, .chorus = 0.1, .width = 1.35, .artist = "Doria" },
        { .name = "Playful Bright Wide FR", .category = "French Female", .style = Style::Pop, .presence = 1, .air = 2, .compress = 0.6, .exciter = 0.35, .rv = 0.18, .dlMode = 2, .dlDiv = 0, .dl = 0.14, .dbl = 0.2, .chorus = 0.2, .width = 1.4, .artist = "Le Juiice" },
        { .name = "Airy Smooth R&B Wide FR", .category = "French Female", .style = Style::RnB, .air = 2, .warmth = 0.5, .compress = 0.45, .exciter = 0.25, .rv = 0.28, .rvDecay = 1.3, .dlDiv = 3, .dl = 0.14, .dbl = 0.15, .chorus = 0.15, .width = 1.4, .artist = "Lala &ce" },
        { .name = "Raw Direct Female Drill FR", .category = "French Female", .style = Style::Drill, .presence = 1, .drive = 2.5, .satMode = 3, .compress = 0.8, .parComp = 0.4, .exciter = 0.3, .rvMode = 0, .rv = 0.05, .dlMode = 1, .dlDiv = 7, .dl = 0.08, .dlMs = 90, .width = 1.0, .artist = "KT Gorique" },
        { .name = "Heavy Autotune", .category = "Vocal FX", .style = Style::Pop, .presence = 1, .air = 1.5, .drive = 1, .compress = 0.65, .exciter = 0.3, .rv = 0.16, .dlMode = 2, .dlDiv = 2, .dl = 0.12, .width = 1.2, .tune = 100, .retune = 0, .humanize = 0, .scale = 0 },
        { .name = "Robotic Vocal", .category = "Vocal FX", .style = Style::Pop, .presence = 1.5, .air = 0.5, .drive = 3, .satMode = 3, .compress = 0.8, .exciter = 0.4, .rvMode = 0, .rv = 0.1, .chorus = 0.35, .width = 1.3, .tune = 100, .retune = 0, .humanize = 0, .scale = 5, .bandLimit = 0.15 },
        { .name = "Octave Harmony", .category = "Vocal FX", .style = Style::MelodicRap, .air = 1, .compress = 0.55, .exciter = 0.2, .rvMode = 2, .rv = 0.24, .rvDecay = 1.3, .width = 1.3, .tune = 45, .retune = 90, .humanize = 0.5, .octDown = 0.45, .octUp = 0.3 },
        { .name = "Natural Harmony", .category = "Vocal FX", .style = Style::RnB, .air = 1.5, .compress = 0.5, .exciter = 0.2, .rv = 0.24, .rvDecay = 1.2, .dbl = 0.1, .width = 1.35, .tune = 35, .retune = 140, .humanize = 0.7, .scale = 1, .harm1 = 1, .harm1Gain = 0.5, .harm2 = 4, .harm2Gain = 0.4 },
        { .name = "Subtle Pitch Polish", .category = "Vocal FX", .style = Style::ModernRap, .compress = 0.5, .exciter = 0.15, .rv = 0.08, .tune = 40, .retune = 160, .humanize = 0.7, .scale = 0 },
        { .name = "Tuned Melodic Hook", .category = "Vocal FX", .style = Style::MelodicRap, .presence = 0.5, .air = 1.5, .compress = 0.6, .exciter = 0.3, .rvMode = 2, .rv = 0.3, .dlMode = 2, .dlDiv = 5, .dl = 0.22, .dbl = 0.2, .width = 1.4, .tune = 75, .retune = 25, .humanize = 0.2, .scale = 0 },
        { .name = "Whisper Vocal", .category = "Vocal FX", .style = Style::CleanStudio, .presence = 0.5, .air = 3, .warmth = -1, .drive = -2, .compress = 0.85, .parComp = 0.5, .exciter = 0.5, .rv = 0.2, .rvDecay = 1.0, .width = 1.2, .nr = 0.35 },
        { .name = "Telephone Vocal", .category = "Vocal FX", .style = Style::Trap, .presence = 1, .air = -4, .drive = 4, .satMode = 3, .compress = 0.8, .rvMode = 0, .rv = 0.04, .width = 0.9, .bandLimit = 1.0 },
        { .name = "Backing Stack Ad-Libs", .category = "Vocal FX", .style = Style::Pop, .compress = 0.65, .exciter = 0.3, .rvMode = 2, .rv = 0.3, .dlMode = 2, .dlDiv = 2, .dl = 0.28, .dlFb = 0.4, .dbl = 0.4, .chorus = 0.2, .width = 1.6, .tune = 30, .retune = 100, .humanize = 0.6, .scale = 1, .harm1 = 1, .harm1Gain = 0.4, .harm2 = 3, .harm2Gain = 0.3, .adlib = true },
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
