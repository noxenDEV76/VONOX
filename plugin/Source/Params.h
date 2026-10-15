#pragma once
// VONOX parameter model. Every parameter is a real DSP / operational control (no decorative ones).
// IDs are part of the saved-project format: never rename, reorder or reuse them.
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/VocalMixEngine.h"
#include "engine/VocalChain.h"
#include <array>

namespace vonox::params {

enum F : int {
    mix, output, compress,
    hpf, lowMidCut, lowMidHz, boxCut, harshCut, harshHz,
    fetThresh, fetRatio, fetAttack, fetRelease,
    optoThresh, optoRatio, optoAttack, optoRelease,
    deEssHz, deEssThresh, deEssAmount, resAmount, resThresh,
    presence, presenceHz, air, warmth,
    satDrive, ceiling,
    space, character, width, parComp, exciter,
    rvAmount, rvDecay, rvPre, rvDamp, rvHp, rvLp,
    dlAmount, dlMs, dlFb, dlHp, dlLp, doubler, chorus, duck,
    // ---- v0.3 voice stages (appended: ids of earlier parameters never change)
    tune, retune, humanize, harm1Gain, harm2Gain, octDown, octUp,
    nrAmount, breathAmount, plosiveAmount, inputGain, autoLevelTarget,
    dynEqHz, dynEqAmount, dynEqThresh, unmask, bandLimit,
    kNumFloats
};

struct Def { const char* id; const char* name; float lo, hi, def; const char* unit; float skew; };

// Order must match enum F.
inline constexpr Def kFloats[kNumFloats] = {
    { "mix",         "Mix Amount",        0.f,    100.f, 100.f,  "%",  1.f },
    { "output",      "Output",           -12.f,   12.f,    0.f,  "dB", 1.f },
    { "compress",    "Compress",          0.f,    1.f,     0.06f, "",   1.f },
    { "hpf",         "High-Pass",         20.f,   200.f,   70.f, "Hz", 0.5f },
    { "lowmid_cut",  "Low-Mid Cut",       0.f,    8.f,     1.44f,  "dB", 1.f },
    { "lowmid_hz",   "Low-Mid Freq",      150.f,  450.f,   300.f,"Hz", 1.f },
    { "box_cut",     "Boxiness Cut",      0.f,    6.f,     0.1f,  "dB", 1.f },
    { "harsh_cut",   "Harshness Cut",     0.f,    6.f,     0.43f,  "dB", 1.f },
    { "harsh_hz",    "Harshness Freq",    1500.f, 5000.f,  3613.67f,"Hz",1.f },
    { "fet_thresh",  "FET Threshold",    -50.f,   0.f,    -5.9f, "dB", 1.f },
    { "fet_ratio",   "FET Ratio",         1.f,    10.f,    3.8f, ":1", 1.f },
    { "fet_attack",  "FET Attack",        0.1f,   30.f,    5.f,  "ms", 0.5f },
    { "fet_release", "FET Release",       10.f,   300.f,   74.5f, "ms", 0.5f },
    { "opto_thresh", "Opto Threshold",   -50.f,   0.f,    -18.5f, "dB", 1.f },
    { "opto_ratio",  "Opto Ratio",        1.f,    6.f,     2.09f,  ":1", 1.f },
    { "opto_attack", "Opto Attack",       1.f,    80.f,    15.f, "ms", 0.5f },
    { "opto_release","Opto Release",      50.f,   1000.f,  300.f,"ms", 0.5f },
    { "deess_hz",    "De-Esser Freq",     4000.f, 12000.f, 8773.05f,"Hz",1.f },
    { "deess_thresh","De-Esser Threshold",-60.f,  0.f,    -6.6f, "dB", 1.f },
    { "deess_amount","De-Esser Amount",   0.f,    1.f,     0.28f, "",   1.f },
    { "res_amount",  "Resonance Control", 0.f,    1.f,     0.48f,  "",   1.f },
    { "res_thresh",  "Resonance Threshold",-60.f, 0.f,    -26.9f, "dB", 1.f },
    { "presence",    "Presence",          0.f,    8.f,     6.4f,  "dB", 1.f },
    { "presence_hz", "Presence Freq",     1500.f, 6000.f,  3668.75f,"Hz",1.f },
    { "air",         "Air",               0.f,    8.f,     3.6f,  "dB", 1.f },
    { "warmth",      "Warmth",           -4.f,    6.f,     5.9f,  "dB", 1.f },
    { "sat_drive",   "Saturation",        0.f,    18.f,    3.4f,  "dB", 1.f },
    { "ceiling",     "Ceiling",          -6.f,    0.f,    -1.f,  "dB", 1.f },
    { "space",       "Space",             0.f,    1.f,     0.11f, "",   1.f },
    { "character",   "Character",         0.f,    1.f,     0.61f, "",   1.f },
    { "width",       "Width",             0.f,    200.f, 198.f,  "%",  1.f },
    { "parcomp",     "Parallel Compression",0.f,  1.f,     0.35f,  "",   1.f },
    { "exciter",     "Harmonic Exciter",  0.f,    1.f,     0.15f,  "",   1.f },
    { "rv_amount",   "Reverb Amount",     0.f,    1.f,     0.28f,  "",   1.f },
    { "rv_decay",    "Reverb Decay",      0.3f,   3.f,     0.85f,  "x",  1.f },
    { "rv_pre",      "Reverb Pre-Delay",  0.f,    100.f,   15.f, "ms", 1.f },
    { "rv_damp",     "Reverb Damping",    0.f,    1.f,     0.64f, "",   1.f },
    { "rv_hp",       "Reverb Low Cut",    20.f,   800.f,   250.f,"Hz", 0.5f },
    { "rv_lp",       "Reverb High Cut",   2000.f, 16000.f, 8147.67f,"Hz",0.5f },
    { "dl_amount",   "Delay Amount",      0.f,    1.f,     0.06f,  "",   1.f },
    { "dl_ms",       "Delay Time (free / slap)",20.f,1000.f,95.f,"ms", 0.5f },
    { "dl_fb",       "Delay Feedback",    0.f,    0.9f,    0.15f, "",   1.f },
    { "dl_hp",       "Delay Low Cut",     20.f,   800.f,   300.f,"Hz", 0.5f },
    { "dl_lp",       "Delay High Cut",    1500.f, 16000.f, 5373.05f,"Hz",0.5f },
    { "doubler",     "Doubler",           0.f,    1.f,     0.04f,  "",   1.f },
    { "chorus",      "Chorus",            0.f,    1.f,     0.f,  "",   1.f },
    { "duck",        "Effects Ducking",   0.f,    1.f,     0.55f,"",   1.f },
    { "tune",        "Tune Amount",       0.f,    100.f,   0.f,  "%",  1.f },
    { "retune",      "Retune Speed",      0.f,    400.f,   60.f, "ms", 0.5f },
    { "humanize",    "Humanize",          0.f,    1.f,     0.3f, "",   1.f },
    { "harm1_gain",  "Harmony 1 Level",   0.f,    1.f,     0.f,  "",   1.f },
    { "harm2_gain",  "Harmony 2 Level",   0.f,    1.f,     0.f,  "",   1.f },
    { "oct_down",    "Octave Down",       0.f,    1.f,     0.f,  "",   1.f },
    { "oct_up",      "Octave Up",         0.f,    1.f,     0.f,  "",   1.f },
    { "nr_amount",   "Noise Reduction",   0.f,    1.f,     0.45f,  "",   1.f },
    { "breath",      "Breath Control",    0.f,    1.f,     0.f,  "",   1.f },
    { "plosive",     "Plosive Control",   0.f,    1.f,     0.f,  "",   1.f },
    { "input_gain",  "Input Gain",       -24.f,   24.f,    0.f,  "dB", 1.f },
    { "autolevel_db","Auto-Level Target",-30.f,  -12.f,   -20.f, "dB", 1.f },
    { "dyneq_hz",    "Dynamic EQ Freq",   500.f,  9000.f,  3000.f,"Hz",0.5f },
    { "dyneq_amount","Dynamic EQ Amount", 0.f,    1.f,     0.f,  "",   1.f },
    { "dyneq_thresh","Dynamic EQ Threshold",-60.f,0.f,    -30.f, "dB", 1.f },
    { "unmask",      "Sidechain Unmask",  0.f,    1.f,     0.f,  "",   1.f },
    { "bandlimit",   "Band-Limit (phone)",0.f,    1.f,     0.f,  "",   1.f },
};

inline constexpr const char* kStyleId = "style";
inline constexpr const char* kSatModeId = "sat_mode";
inline constexpr const char* kRvModeId = "rv_mode";
inline constexpr const char* kDlModeId = "dl_mode";
inline constexpr const char* kDlDivId = "dl_div";
inline constexpr const char* kBypassId = "bypass";
inline constexpr const char* kLevelMatchId = "level_match";
inline constexpr const char* kKeyId = "key";
inline constexpr const char* kScaleId = "scale";
inline constexpr const char* kHarm1Id = "harm1";
inline constexpr const char* kHarm2Id = "harm2";
inline constexpr const char* kAutoLevelId = "autolevel";
inline constexpr const char* kAdlibId = "adlib";

inline juce::StringArray styleNames() {
    juce::StringArray s; for (int i = 0; i < 9; ++i) s.add(vonox::engine::styleName(static_cast<vonox::engine::Style>(i))); return s;
}
inline juce::StringArray satModeNames() { return { "Clean", "Warm", "Analog", "Aggressive", "Tube", "Tape", "Soft Clip" }; }
inline juce::StringArray rvModeNames() { return { "Room", "Plate", "Hall", "Chamber" }; }
inline juce::StringArray dlModeNames() { return { "Stereo (sync)", "Slapback", "Ping-Pong" }; }
inline juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
inline juce::StringArray scaleNames() { return { "Chromatic", "Major", "Minor", "Harmonic Minor", "Pentatonic Major", "Pentatonic Minor", "Blues" }; }
inline juce::StringArray harmNames() { return { "Off", "3rd up", "3rd down", "5th up", "5th down", "6th up", "Octave up", "Octave down" }; }
inline juce::StringArray dlDivNames() { return { "1/16", "1/8 triplet", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2", "Free (ms)" }; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ kStyleId, 1 }, "Vocal Profile", styleNames(), 1));
    for (const auto& d : kFloats) {
        NormalisableRange<float> r(d.lo, d.hi, 0.01f, d.skew);
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{ d.id, 1 }, d.name, r, d.def, AudioParameterFloatAttributes().withLabel(d.unit)));
    }
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ kSatModeId, 1 }, "Saturation Character", satModeNames(), 1));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ kRvModeId, 1 }, "Reverb Type", rvModeNames(), 2));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ kDlModeId, 1 }, "Delay Type", dlModeNames(), 1));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ kDlDivId, 1 }, "Delay Sync", dlDivNames(), 2));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ kBypassId, 1 }, "Bypass", false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ kLevelMatchId, 1 }, "Level Match", false));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ kKeyId, 1 }, "Key", keyNames(), 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ kScaleId, 1 }, "Scale", scaleNames(), 1));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ kHarm1Id, 1 }, "Harmony 1", harmNames(), 1));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ kHarm2Id, 1 }, "Harmony 2", harmNames(), 3));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ kAutoLevelId, 1 }, "Auto-Level", false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ kAdlibId, 1 }, "Ad-Lib Mode", false));
    return l;
}

// Cached raw-value pointers: lock-free reads on the audio thread.
struct Cache {
    std::array<std::atomic<float>*, kNumFloats> f{};
    std::atomic<float>* satMode = nullptr; std::atomic<float>* rvMode = nullptr; std::atomic<float>* dlMode = nullptr; std::atomic<float>* dlDiv = nullptr; std::atomic<float>* bypass = nullptr; std::atomic<float>* levelMatch = nullptr;
    std::atomic<float>* key = nullptr; std::atomic<float>* scale = nullptr; std::atomic<float>* harm1 = nullptr; std::atomic<float>* harm2 = nullptr; std::atomic<float>* autoLevel = nullptr; std::atomic<float>* adlib = nullptr;
    void bind(juce::AudioProcessorValueTreeState& s) {
        for (int i = 0; i < kNumFloats; ++i) f[i] = s.getRawParameterValue(kFloats[i].id);
        rvMode = s.getRawParameterValue(kRvModeId); dlMode = s.getRawParameterValue(kDlModeId); dlDiv = s.getRawParameterValue(kDlDivId);
        satMode = s.getRawParameterValue(kSatModeId); bypass = s.getRawParameterValue(kBypassId); levelMatch = s.getRawParameterValue(kLevelMatchId);
        key = s.getRawParameterValue(kKeyId); scale = s.getRawParameterValue(kScaleId); harm1 = s.getRawParameterValue(kHarm1Id); harm2 = s.getRawParameterValue(kHarm2Id);
        autoLevel = s.getRawParameterValue(kAutoLevelId); adlib = s.getRawParameterValue(kAdlibId);
    }
    float get(int i) const { return f[i]->load(std::memory_order_relaxed); }
};

inline vonox::engine::MixSettings toSettings(const Cache& c) {
    vonox::engine::MixSettings m;
    m.hpfHz = c.get(hpf); m.lowMidCutDb = -c.get(lowMidCut); m.lowMidHz = c.get(lowMidHz); m.boxCutDb = -c.get(boxCut);
    m.harshCutDb = -c.get(harshCut); m.harshHz = c.get(harshHz);
    m.fetThreshDb = c.get(fetThresh); m.fetRatio = c.get(fetRatio); m.fetAttackMs = c.get(fetAttack); m.fetReleaseMs = c.get(fetRelease);
    m.optoThreshDb = c.get(optoThresh); m.optoRatio = c.get(optoRatio); m.optoAttackMs = c.get(optoAttack); m.optoReleaseMs = c.get(optoRelease);
    m.deEssHz = c.get(deEssHz); m.deEssThreshDb = c.get(deEssThresh); m.deEssAmount = c.get(deEssAmount);
    m.resAmount = c.get(resAmount); m.resThreshDb = c.get(resThresh);
    m.presenceDb = c.get(presence); m.presenceHz = c.get(presenceHz); m.airDb = c.get(air); m.warmthDb = c.get(warmth);
    m.satDriveDb = c.get(satDrive); m.ceilingDb = c.get(ceiling);
    const int sm = juce::jlimit(0, 6, int(c.satMode->load(std::memory_order_relaxed) + 0.5f));
    m.satMode = static_cast<vonox::dsp::Saturator::Mode>(sm);
    m.parCompAmount = c.get(parComp); m.exciterAmount = c.get(exciter);
    m.reverbMode = juce::jlimit(0, 3, int(c.rvMode->load(std::memory_order_relaxed) + 0.5f));
    m.reverbAmount = c.get(rvAmount); m.reverbDecay = c.get(rvDecay); m.reverbPreMs = c.get(rvPre); m.reverbDamp = c.get(rvDamp); m.reverbHp = c.get(rvHp); m.reverbLp = c.get(rvLp);
    m.delayMode = juce::jlimit(0, 2, int(c.dlMode->load(std::memory_order_relaxed) + 0.5f));
    m.delayDiv = juce::jlimit(0, 7, int(c.dlDiv->load(std::memory_order_relaxed) + 0.5f));
    m.delayAmount = c.get(dlAmount); m.delayMs = c.get(dlMs); m.delayFeedback = c.get(dlFb); m.delayHp = c.get(dlHp); m.delayLp = c.get(dlLp);
    m.doublerAmount = c.get(doubler); m.chorusAmount = c.get(chorus); m.duck = c.get(duck); m.width = c.get(width) / 100.0;
    return m;
}

inline int choiceOf(const std::atomic<float>* a, int n) { return juce::jlimit(0, n - 1, int(a->load(std::memory_order_relaxed) + 0.5f)); }

inline vonox::engine::VoiceSettings toVoice(const Cache& c) {
    vonox::engine::VoiceSettings v;
    v.inputGainDb = c.get(inputGain); v.autoLevel = c.autoLevel->load(std::memory_order_relaxed) > 0.5f; v.autoLevelDb = c.get(autoLevelTarget);
    v.nr = c.get(nrAmount); v.plosive = c.get(plosiveAmount); v.breath = c.get(breathAmount);
    auto& p = v.pitch; p.amount = c.get(tune) / 100.0; p.retuneMs = c.get(retune); p.humanize = c.get(humanize);
    p.key = choiceOf(c.key, 12); p.scale = choiceOf(c.scale, 7); p.harm1 = choiceOf(c.harm1, 8); p.harm2 = choiceOf(c.harm2, 8);
    p.harm1Gain = c.get(harm1Gain); p.harm2Gain = c.get(harm2Gain); p.octDown = c.get(octDown); p.octUp = c.get(octUp);
    v.dynEqHz = c.get(dynEqHz); v.dynEqAmount = c.get(dynEqAmount); v.dynEqThreshDb = c.get(dynEqThresh);
    v.unmask = c.get(unmask); v.adlib = c.adlib->load(std::memory_order_relaxed) > 0.5f; v.bandLimit = c.get(bandLimit);
    return v;
}

// Values to write into the parameters for a MixSettings (mix / output / compress are not part of it).
inline std::array<float, kNumFloats> fromSettings(const vonox::engine::MixSettings& m) {
    std::array<float, kNumFloats> v; v.fill(std::numeric_limits<float>::quiet_NaN());
    v[hpf] = float(m.hpfHz); v[lowMidCut] = float(-m.lowMidCutDb); v[lowMidHz] = float(m.lowMidHz); v[boxCut] = float(-m.boxCutDb);
    v[harshCut] = float(-m.harshCutDb); v[harshHz] = float(m.harshHz);
    v[fetThresh] = float(m.fetThreshDb); v[fetRatio] = float(m.fetRatio); v[fetAttack] = float(m.fetAttackMs); v[fetRelease] = float(m.fetReleaseMs);
    v[optoThresh] = float(m.optoThreshDb); v[optoRatio] = float(m.optoRatio); v[optoAttack] = float(m.optoAttackMs); v[optoRelease] = float(m.optoReleaseMs);
    v[deEssHz] = float(m.deEssHz); v[deEssThresh] = float(m.deEssThreshDb); v[deEssAmount] = float(m.deEssAmount);
    v[resAmount] = float(m.resAmount); v[resThresh] = float(m.resThreshDb);
    v[presence] = float(m.presenceDb); v[presenceHz] = float(m.presenceHz); v[air] = float(m.airDb); v[warmth] = float(m.warmthDb);
    v[satDrive] = float(m.satDriveDb); v[ceiling] = float(m.ceilingDb);
    v[parComp] = float(m.parCompAmount); v[exciter] = float(m.exciterAmount);
    v[rvAmount] = float(m.reverbAmount); v[rvDecay] = float(m.reverbDecay); v[rvPre] = float(m.reverbPreMs); v[rvDamp] = float(m.reverbDamp); v[rvHp] = float(m.reverbHp); v[rvLp] = float(m.reverbLp);
    v[dlAmount] = float(m.delayAmount); v[dlMs] = float(m.delayMs); v[dlFb] = float(m.delayFeedback); v[dlHp] = float(m.delayHp); v[dlLp] = float(m.delayLp);
    v[doubler] = float(m.doublerAmount); v[chorus] = float(m.chorusAmount); v[duck] = float(m.duck); v[width] = float(m.width * 100.0);
    for (int i = 0; i < kNumFloats; ++i) if (!std::isnan(v[i])) v[i] = juce::jlimit(kFloats[i].lo, kFloats[i].hi, v[i]);
    return v;
}

} // namespace vonox::params
