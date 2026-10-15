#pragma once
// VONOX ships exactly one factory preset: "Default". It is the set of parameter defaults declared in
// Params.h (the author's tuned ADVANCED / main-knob settings). RESET restores it; users add their own
// presets with "Save current sound as preset" (stored as files in the user's application-data folder).
#include <vector>

namespace vonox {

struct Preset { const char* name; const char* category; };

inline const char* displayName(const Preset& p) { return p.name; }

inline const std::vector<Preset>& factoryPresets() {
    static const std::vector<Preset> p = { { "Default", "Factory" } };
    return p;
}

} // namespace vonox
