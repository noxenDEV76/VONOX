# What is finished, what is tested, what is not

Read this before trusting any claim. **"Tested" means an automated test on synthetic signals** (tones, harmonic stacks, noise, bursts) that measures the real output. **Nothing here has been listening-tested on real voices, and the plugin has not been loaded in FL Studio or any other DAW by the author.** Windows and Linux builds and all tests run on GitHub Actions.

Legend: **Done + tested** = implemented and measured in tests. **Experimental** = implemented and tested on synthetic signals, but quality on real voices is unverified and known to have limits. **Partial** = only part of the idea is implemented. **Missing** = not built, no control exists.

| Module | Status | What the tests measure / honest limits |
|---|---|---|
| Input gain, Auto-Level | Done + tested | Reaches the target level from -30 dBFS within 3 dB; does not boost a -66 dBFS noise floor. Slow (1.5 s up / 0.5 s down) leveller, not a compressor. |
| Noise reduction | Experimental | STFT spectral subtraction with continuous minimum-statistics noise tracking (no learn button). ≥ 8 dB (about 16 dB measured) lower noise floor, 440 Hz tone within 1 dB; exact delay at 0. Works on steady noise; "musical noise" artefacts on real recordings are unchecked. |
| Plosive control | Done + tested | Dynamic 150 Hz high-pass on low-frequency bursts: 10.6 dB less pop energy, voice tone unchanged (0.00 dB). |
| Breath control | Experimental | Uses the pitch tracker: only quiet, unvoiced, noisy passages are attenuated (-14 dB measured), loud sibilants and voiced audio untouched. Heuristic. |
| Pitch correction (TUNE) | Experimental | YIN tracker (< 8 cents, 90-880 Hz, 44.1 / 48 / 96 kHz) + PSOLA. +40 cents → under 1 cent in the full chain; retune speed, humanize, scale lock. Real voices (breathy, very low, noisy, fast runs) may produce artefacts; no formant control. |
| Key / scale detection | Experimental | Krumhansl-Schmuckler on the sung-pitch histogram: finds the right key on a synthetic A-major melody. Heuristic, needs several seconds of sung notes. Not used unless you press AUTO MIX or set KEY / SCALE. |
| Harmonizer (2 voices) | Experimental | In-key diatonic steps (3rd, 5th, 6th, octave): a 3rd-up line measured +50 dB above the baseline at the correct pitch. Follows the KEY / SCALE you set. Mono-voiced input only. |
| Octave generator | Experimental | **Octave up** = PSOLA; **octave down** = pitch-tracked ring-modulated sub-octave (not PSOLA, which cannot make a clean one). Sounds like a synth-ish sub-octave, not a real second voice. |
| Subtractive EQ, FET and opto compressors, de-esser, resonance control | Done + tested | Unchanged from 0.2.0. |
| Dynamic EQ | Partial | One dynamic **cut** band (frequency / threshold / amount). No dynamic boost band. |
| Saturation (7 modes) | Done + tested | Now 2× oversampled (alias of a 13 kHz tone down by 59 dB). |
| Reverb ×4, delay (sync, slapback, ping-pong), chorus, doubler, width | Done + tested | Unchanged from 0.2.0. |
| Band-limit ("Telephone") | Done + tested | 450 Hz - 3.2 kHz, 4th order, blendable. |
| Output limiter + meter | Done + tested | Look-ahead true-peak limiter; checked against an independent 8× true-peak measurement at 44.1 / 48 / 96 kHz (stays 0.1-0.2 dB under the ceiling, 0.26 dB internal margin). The readout under the OUTPUT meter is an estimate from a 4× interpolator. |
| LEVEL MATCH (A/B loudness) | Partial | Gain-matches processed to dry using **K-weighted** RMS (BS.1770-style filters). It is **not** a certified LUFS meter and there is no LUFS readout. |
| Sidechain unmasking | Experimental | Optional sidechain input (1-2 ch): where it has energy in 1.2-4.5 kHz and masks the vocal, the vocal gets up to +4 dB presence and a small low-mid trim. Measured on synthetic noise. **Your DAW must route a sidechain to the plugin; this has not been tried in FL Studio.** |
| Ad-lib processing | Partial | **AD-LIB MODE** is a role switch on the same chain (more space, width, doubler, higher low cut). It is not a separate processor for a separate track. Put one VONOX per track. |
| Reference tonal analysis | Partial | Load a WAV / AIFF / FLAC / OGG / MP3; AUTO MIX moves presence, air and low-mid toward its band balance (each ≤ 2.4 dB). Coarse 3-band match only: not a full spectral match, no loudness match. |
| Latency compensation | Done + tested | Constant (about 43 ms at 48 kHz, shown in the UI), reported to the host. Bypass and LEVEL MATCH are delay-matched; bypass is bit-exact to the delayed input. Host-side compensation itself is untested in a DAW. |
| AUTO MIX | Done + tested | Analysis-based (measured rules, no neural network, no cloud). Also sets noise reduction and plosive control when the analysis calls for it, and key / scale when enough pitch was heard. |
| Presets | Done + tested | 81 presets apply and render finite audio under the ceiling. Quality by ear: unverified. |
| User presets | Done + tested | Save / load / delete in the browser, XML in your user folder. |
| CPU | Measured | Everything on, stereo 48 kHz: about 18 % of one core (single-thread benchmark in a cloud VM, Release). |

## Missing / not planned for this version
- A real LUFS (EBU R128) meter and loudness target.
- Formant shifting, separate "gender" control, polyphonic pitch handling.
- A spectrum analyser display and a resizable window.
- macOS and Linux plugin binaries (the code builds on Linux in CI; only the Windows installer is released).
- Code signing of the installer.
- "AI" in the machine-learning sense: there is none. "AI MIX" here means measurement-driven automatic setup.
