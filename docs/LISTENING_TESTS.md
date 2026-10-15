# Listening tests: how to check VONOX on real voices

These have **not** been done by the author. Here is a short, honest procedure you can follow in 20 minutes, and what to report.

1. **Latency / bypass**: put VONOX on a vocal track, toggle BYPASS while playing. The level should not jump and there should be no click. Your DAW should show about 43 ms of plugin delay and keep the track aligned with the beat (check against a kick with the track duplicated and polarity-flipped on one copy: bypassed, they should cancel).
2. **AUTO MIX**: play 5-10 s of a dry vocal, press AUTO MIX for 3 different profiles (then RESET and compare with Default). Note anything harsh, boomy or lispy.
3. **TUNE**: set KEY / SCALE, TUNE 100 %, retune 0 ms: robotic is expected. TUNE 40 % / 150 ms: should sound natural. Report warble on breathy, very low or fast passages.
4. **Harmonies**: set the right key; Harmony 1 = 3rd up at 0.5. Listen for out-of-key notes and artefacts on consonants.
5. **Noise reduction**: on a noisy room recording raise it slowly; stop when it sounds watery.
6. **Sidechain**: route the beat to VONOX's sidechain input, set SIDECHAIN UNMASK to 0.5. The vocal should get a little more presence when the beat is busy.
7. **True peak**: export with the ceiling at -1 dB and check the file with any true-peak meter.

Report: DAW + version, sample rate, buffer size, the preset, and a 5 second clip if you can. Open an issue on GitHub.
