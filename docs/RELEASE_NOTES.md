## VONOX 0.4.0: galaxy

Your FL Studio feedback, applied.

**New**
- **Galaxy theme**: black window with moving white stars, violet / blue nebula flames, nebula glass in the centre. The steel / chains design is unchanged.
- **Voice-reactive stars** inside the big MIX AMOUNT octagon: calm when quiet, brighter and agitated with the loudness of the voice.
- **ADVANCED** is now the big button in the hexagon (the V is gone), written in full.
- **RESET** button (all parameters back to Default) and **PRESETS** button.
- One factory preset, **Default** = the author's tuned settings (main knobs and every ADVANCED value). Your own presets can still be saved.
- New fonts (Orbitron, Rajdhani; SIL OFL, see plugin/assets/fonts), nicer LATENCY and TRUE PEAK readouts, the status text now sits inside the glass.
- Small animations: button ripples, cross-fades and a star burst on AUTO MIX, knob hover halo and drag glow.
- ADVANCED panel: black starfield background like the preset browser, metal knobs with a knurled edge.

**Fixed**
- **BYPASS did nothing** (the button never toggled) and showed no state. It now works and the whole plugin dims with a "BYPASSED" notice in the glass.
- The white hover square on buttons is gone (rounded glow instead).
- **AUTO MIX over-compressed**: the FET now only catches the loudest peaks (threshold 2-5 dB below the measured peak, ratio at most 3.6:1), the opto is gentler (1.4-2.1:1) and parallel compression is capped at 25 %.

**Removed**
- The 80 other presets (artist, style and FX presets), the GAIN REDUCTION meter, the LEVEL MATCH button (the parameter stays in the file format for old projects but has no control), and the circles drawn on the knobs.

**Honest status**: see docs/STATUS.md. Automated tests on synthetic signals pass; no listening tests and no DAW test by the author.

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
