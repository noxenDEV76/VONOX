#pragma once
// Musical key / scale helpers: scale masks, note quantisation with hysteresis, diatonic stepping and
// key estimation (Krumhansl-Schmuckler profiles on a confidence-weighted pitch-class histogram).
#include <array>
#include <cmath>
#include <vector>
#include <algorithm>
namespace vonox::engine {

enum class ScaleType { Chromatic, Major, Minor, HarmonicMinor, PentatonicMajor, PentatonicMinor, Blues };
inline constexpr int kNumScales = 7;
inline const char* scaleName(int s) {
    static const char* n[] = { "Chromatic", "Major", "Minor", "Harmonic Minor", "Pentatonic Major", "Pentatonic Minor", "Blues" }; return n[std::clamp(s, 0, 6)]; }
inline const char* keyName(int k) {
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n[((k % 12) + 12) % 12]; }

inline std::array<bool, 12> scaleMask(int key, ScaleType t) {
    static const int iv[7][12] = { { 0,1,2,3,4,5,6,7,8,9,10,11 }, { 0,2,4,5,7,9,11,-1,-1,-1,-1,-1 }, { 0,2,3,5,7,8,10,-1,-1,-1,-1,-1 },
        { 0,2,3,5,7,8,11,-1,-1,-1,-1,-1 }, { 0,2,4,7,9,-1,-1,-1,-1,-1,-1,-1 }, { 0,3,5,7,10,-1,-1,-1,-1,-1,-1,-1 }, { 0,3,5,6,7,10,-1,-1,-1,-1,-1,-1 } };
    std::array<bool, 12> m{}; for (int i = 0; i < 12; ++i) { const int v = iv[int(t)][i]; if (v >= 0) m[size_t((key + v) % 12)] = true; } return m;
}

// Nearest allowed note to a (fractional) MIDI pitch. `prev` (or -1) adds hysteresis so notes do not flicker at borders.
inline int quantizeNote(double midi, const std::array<bool, 12>& mask, int prev = -1, double hysteresisSemis = 0.25) {
    int best = -1; double bd = 1e9;
    const int c = int(std::floor(midi + 0.5));
    for (int n = c - 7; n <= c + 7; ++n) { if (!mask[size_t(((n % 12) + 12) % 12)]) continue; const double dd = std::abs(midi - n); if (dd < bd) { bd = dd; best = n; } }
    if (best < 0) return c;
    if (prev >= 0 && mask[size_t(((prev % 12) + 12) % 12)] && std::abs(midi - prev) <= bd + hysteresisSemis) return prev;
    return best;
}
// Move `steps` scale degrees from `note` (positive = up). Falls back to semitone math when the mask is chromatic.
inline int stepInScale(int note, int steps, const std::array<bool, 12>& mask) {
    int n = note; int dir = steps >= 0 ? 1 : -1; int left = std::abs(steps), guard = 0;
    while (left > 0 && guard++ < 200) { n += dir; if (mask[size_t(((n % 12) + 12) % 12)]) --left; }
    return n;
}

struct KeyEstimate { int key = 0; bool minor = false; double confidence = 0; bool valid = false; };
// hist[12] = confidence-weighted counts per pitch class (C = 0)
inline KeyEstimate estimateKey(const std::array<double, 12>& hist) {
    static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double min[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    KeyEstimate r; double total = 0; for (double h : hist) total += h;
    if (total < 20.0) return r;                                             // too little evidence
    auto corr = [&](const double* prof, int shift) {
        double mh = total / 12.0, mp = 0; for (int i = 0; i < 12; ++i) mp += prof[i]; mp /= 12.0;
        double num = 0, dh = 0, dp = 0;
        for (int i = 0; i < 12; ++i) { const double h = hist[size_t((i + shift) % 12)] - mh, p = prof[i] - mp; num += h * p; dh += h * h; dp += p * p; }
        return (dh > 0 && dp > 0) ? num / std::sqrt(dh * dp) : 0.0; };
    std::vector<std::pair<double, std::pair<int, bool>>> sc;
    for (int k = 0; k < 12; ++k) { sc.push_back({ corr(maj, k), { k, false } }); sc.push_back({ corr(min, k), { k, true } }); }
    std::sort(sc.begin(), sc.end(), [](auto& a, auto& b) { return a.first > b.first; });
    // The relative major/minor pair shares the same notes: compare the best with the best NON-relative competitor.
    const auto best = sc[0]; double second = -1;
    for (size_t i = 1; i < sc.size(); ++i) {
        const auto& c = sc[i]; const bool relative = (c.second.second != best.second.second) && (((best.second.second ? best.second.first + 3 : best.second.first + 9) % 12) == c.second.first);
        if (!relative) { second = c.first; break; }
    }
    r.key = best.second.first; r.minor = best.second.second; r.valid = true;
    r.confidence = std::clamp((best.first - second) * 4.0 * std::max(0.0, best.first), 0.0, 1.0);   // margin AND absolute fit
    return r;
}
} // namespace vonox::engine
