## VONOX 0.3.0: the voice release

0.2.0 added space and colour. 0.3.0 adds the voice tools that were missing.

**New**
- **TUNE**: automatic pitch correction (key and scale aware, retune speed, humanize).
- **Harmonizer** with 2 in-key voices, **octave** up / down.
- **Noise reduction**, **plosive control**, **breath control**, **input gain**, **auto-level**.
- **Sidechain unmasking** (optional sidechain input), **dynamic EQ** cut band, **telephone band-limit**, **AD-LIB MODE**.
- **Reference tone match** (load a file, AUTO MIX moves presence / air / low-mid toward it).
- AUTO MIX now also detects the **key** you sing in, and sets noise reduction / plosive control when the analysis calls for it.
- **Look-ahead true-peak limiter** and a true-peak readout; 2x oversampled saturation; K-weighted LEVEL MATCH.
- **81 presets sorted by artist** (US Rap, US Female Rap, French Rap, French Female) plus 34 styles and 9 Vocal FX, in a new black preset browser. Save your own presets.

**Changed**
- The plugin now has a constant latency of about 43 ms, reported to the host. Bypass is sample-aligned with the processed signal.
- LOW-MID moved from the front panel to ADVANCED; TUNE took its place.

**Honest status**: see docs/STATUS.md. Automated tests on synthetic signals pass; no listening tests and no DAW test by the author. Pitch, harmony, noise reduction, breath and sidechain features are experimental. Artist names are creative references only, not endorsed.
