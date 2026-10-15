# Presets

VONOX 0.4.0 ships **one** factory preset: **Default**. It is the set of parameter defaults declared in `plugin/Source/Params.h`: the author's own tuned main knobs and ADVANCED values (EQ, FET / opto compressors, de-esser, saturation "Warm", Hall reverb, slapback delay, doubler, ...). Noise reduction (0.45) and stereo width (198 %) come literally from those settings: lower them if your recording is clean or you want a narrower image. KEY is left at C major (it is song specific; AUTO MIX can detect it).

- **RESET** (bottom right) restores every parameter to Default. BYPASS is not touched.
- **PRESETS** opens the browser: "Factory" (Default) and "My Presets".
- **Save your own**: in "My Presets" click "+ Save current sound as preset...". Presets are plain XML files in your user data folder and work in every project.

Earlier versions had 81 presets (artist-named references, styles and vocal FX); they were removed on request.
