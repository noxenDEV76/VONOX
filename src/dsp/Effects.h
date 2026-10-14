#pragma once
// Time-based and spatial effects. All processors are stereo-in / stereo-out, allocation happens only in
// prepare(), and every wet processor outputs the WET signal only (the router decides how much to send
// and where to return it, so wet paths can be filtered, ducked and balanced independently).
#include "Biquad.h"
#include "Dynamics.h"
#include <array>
#include <vector>
#include <cstdint>

namespace vonox::dsp {

// ---------------------------------------------------------------- fractional delay line
class DelayLine {
public:
    void prepare(size_t maxSamples) {
        size_t n = 16; while (n < maxSamples + 4) n <<= 1;
        buf.assign(n, 0.0); mask = n - 1; w = 0;
    }
    void reset() { std::fill(buf.begin(), buf.end(), 0.0); w = 0; }
    inline void push(double x) { buf[w] = x; w = (w + 1) & mask; }
    // delay in samples (>=1), linear interpolation; 1 = the last pushed sample
    inline double read(double d) const {
        const double pos = double(w) - d;
        const double fl = std::floor(pos); const double fr = pos - fl;
        const size_t i0 = size_t(int64_t(fl)) & mask, i1 = (i0 + 1) & mask;
        return buf[i0] * (1.0 - fr) + buf[i1] * fr;
    }
private:
    std::vector<double> buf; size_t mask = 0, w = 0;
};

// ---------------------------------------------------------------- reverb (8-line feedback delay network)
class Reverb {
public:
    enum class Mode { Room, Plate, Hall, Chamber };
    struct Params {
        Mode mode = Mode::Plate;
        double decay = 1.0;        // multiplier on the mode's RT60 (0.3 .. 3)
        double preDelayMs = 15;
        double damping = 0.5;      // 0 = bright tail, 1 = very dark tail
        double diffusion = 0.7;    // 0..1
        double hpHz = 200, lpHz = 9000;  // wet filters
        double width = 1.0;        // 0 = mono wet, 1 = full stereo
    };
    void prepare(double sr) {
        sampleRate = sr;
        const size_t maxLine = size_t(sr * 0.25) + 8;
        for (auto& d : line) d.prepare(maxLine);
        pre.prepare(size_t(sr * 0.25));
        for (int i = 0; i < 4; ++i) apBuf[i].assign(size_t(sr * 0.03) + 8, 0.0);
        set(p); reset();
    }
    void reset() {
        for (auto& d : line) d.reset();
        pre.reset(); for (auto& b : apBuf) std::fill(b.begin(), b.end(), 0.0);
        for (auto& v : lp) { v = 0.0; }
        apPos = {0, 0, 0, 0};
        hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset(); lfoPh = 0.0;
    }
    void set(const Params& np) {
        p = np;
        struct M { double size, rt, damp, diff; };
        M m{};
        switch (p.mode) {
        case Mode::Room:    m = { 0.55, 0.45, 0.60, 0.55 }; break;
        case Mode::Plate:   m = { 0.85, 1.40, 0.25, 0.90 }; break;
        case Mode::Hall:    m = { 1.45, 2.80, 0.45, 0.80 }; break;
        case Mode::Chamber: m = { 0.95, 1.20, 0.55, 0.70 }; break;
        }
        static const double baseMs[8] = { 29.7, 37.1, 41.1, 43.7, 53.3, 59.9, 67.1, 71.3 };
        const double rt = std::max(0.1, m.rt * std::clamp(p.decay, 0.2, 4.0));
        for (int i = 0; i < 8; ++i) {
            lineLen[i] = std::clamp(baseMs[i] * 1e-3 * m.size * sampleRate, 8.0, double(sampleRate * 0.24));
            fb[i] = std::pow(10.0, -3.0 * lineLen[i] / (rt * sampleRate));   // -60 dB after rt seconds
        }
        const double d = std::clamp(0.5 * (p.damping + m.damp), 0.0, 0.97);
        dampCoef = d;
        apG = 0.25 + 0.5 * std::clamp(0.5 * (p.diffusion + m.diff), 0.0, 1.0);
        static const double apMs[4] = { 5.0, 7.7, 11.3, 15.1 };
        for (int i = 0; i < 4; ++i) apLen[i] = std::max<size_t>(2, size_t(apMs[i] * 1e-3 * sampleRate * (0.6 + 0.4 * m.size)));
        preSamples = std::clamp(p.preDelayMs, 0.0, 200.0) * 1e-3 * sampleRate;
        hpL.set(Biquad::Type::HighPass, sampleRate, p.hpHz, 0.7071); hpR.set(Biquad::Type::HighPass, sampleRate, p.hpHz, 0.7071);
        lpL.set(Biquad::Type::LowPass, sampleRate, p.lpHz, 0.7071);  lpR.set(Biquad::Type::LowPass, sampleRate, p.lpHz, 0.7071);
    }
    // in: stereo dry (or send); out: wet only
    inline void process(double inL, double inR, double& outL, double& outR) {
        double x = 0.5 * (inL + inR);
        pre.push(x); x = (preSamples >= 1.0) ? pre.read(preSamples) : x;
        for (int i = 0; i < 4; ++i) {                       // series Schroeder allpass diffusion
            auto& b = apBuf[i]; size_t& pos = apPos[i]; const size_t n = apLen[i];
            const double dl = b[pos];
            const double v = x - apG * dl;
            b[pos] = v; x = dl + apG * v;
            if (++pos >= n) pos = 0;
        }
        lfoPh += 0.35 / sampleRate; if (lfoPh >= 1.0) lfoPh -= 1.0;
        double y[8];
        for (int i = 0; i < 8; ++i) {
            const double mod = (i & 1) ? std::sin(2 * kPi * (lfoPh + 0.125 * i)) * 0.0004 * sampleRate * 0.5 : 0.0;
            y[i] = line[i].read(std::max(2.0, lineLen[i] + mod));
        }
        // Hadamard mixing (energy preserving) + decay + damping, then feed back with the input
        double h[8];
        for (int i = 0; i < 8; ++i) h[i] = y[i];
        for (int len = 1; len < 8; len <<= 1)
            for (int i = 0; i < 8; i += len << 1)
                for (int j = i; j < i + len; ++j) { const double a = h[j], b = h[j + len]; h[j] = a + b; h[j + len] = a - b; }
        constexpr double norm = 0.35355339059327373;      // 1/sqrt(8)
        for (int i = 0; i < 8; ++i) {
            double v = h[i] * norm * fb[i];
            lp[i] += (1.0 - dampCoef) * (v - lp[i]);         // one-pole damping inside the loop
            line[i].push(lp[i] + x * 0.3);
        }
        static const double sL[8] = { 1, -1, 1, 1, -1, 1, -1, -1 }, sR[8] = { 1, 1, -1, 1, 1, -1, -1, 1 };
        double l = 0, r = 0;
        for (int i = 0; i < 8; ++i) { l += sL[i] * y[i]; r += sR[i] * y[i]; }
        l *= 0.35; r *= 0.35;
        const double mid = 0.5 * (l + r), side = 0.5 * (l - r) * std::clamp(p.width, 0.0, 1.0);
        outL = lpL.process(hpL.process(mid + side)); outR = lpR.process(hpR.process(mid - side));
    }
private:
    double sampleRate = 48000, dampCoef = 0.5, apG = 0.5, preSamples = 0, lfoPh = 0;
    std::array<DelayLine, 8> line; DelayLine pre;
    double lineLen[8] = {}, fb[8] = {}, lp[8] = {};
    std::array<std::vector<double>, 4> apBuf; std::array<size_t, 4> apLen{ 2, 2, 2, 2 }, apPos{ 0, 0, 0, 0 };
    Biquad hpL, hpR, lpL, lpR; Params p;
};

// ---------------------------------------------------------------- stereo delay (sync / slapback / ping-pong)
class StereoDelay {
public:
    enum class Mode { Stereo, Slapback, PingPong };
    struct Params { Mode mode = Mode::Stereo; double timeMs = 250; double feedback = 0.35; double hpHz = 250, lpHz = 5500; double width = 1.0; };
    void prepare(double sr) {
        sampleRate = sr; dl.prepare(size_t(sr * 2.2)); dr.prepare(size_t(sr * 2.2)); reset(); set(p);
    }
    void reset() { dl.reset(); dr.reset(); fl = fr = 0; hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset(); curL = curR = -1; }
    void set(const Params& np) {
        p = np;
        tgtL = tgtR = std::clamp(p.timeMs, 1.0, 2000.0);
        if (p.mode == Mode::Slapback) { tgtL = std::clamp(p.timeMs, 40.0, 180.0); tgtR = tgtL * 1.18; }
        fbk = std::clamp(p.feedback, 0.0, 0.92); if (p.mode == Mode::Slapback) fbk = std::min(fbk, 0.15);
        hpL.set(Biquad::Type::HighPass, sampleRate, p.hpHz, 0.7071); hpR.set(Biquad::Type::HighPass, sampleRate, p.hpHz, 0.7071);
        lpL.set(Biquad::Type::LowPass, sampleRate, p.lpHz, 0.7071);  lpR.set(Biquad::Type::LowPass, sampleRate, p.lpHz, 0.7071);
        if (curL < 0) { curL = tgtL; curR = tgtR; }
    }
    inline void process(double inL, double inR, double& outL, double& outR) {
        const double a = 1.0 - std::exp(-1.0 / (sampleRate * 0.05));   // 50 ms time glide: no clicks
        curL += a * (tgtL - curL); curR += a * (tgtR - curR);
        const double tl = dl.read(curL * 1e-3 * sampleRate), tr = dr.read(curR * 1e-3 * sampleRate);
        const double m = 0.5 * (inL + inR);
        double wl = tl, wr = tr;
        // filtered, soft-limited feedback (keeps the loop stable even at high feedback)
        const double fL = std::tanh(lpL.process(hpL.process(tl)) * fbk), fR = std::tanh(lpR.process(hpR.process(tr)) * fbk);
        if (p.mode == Mode::PingPong) { dl.push(m + fR); dr.push(fL); }     // L feeds R feeds L, input enters on the left
        else                           { dl.push(0.5 * (inL + m) + fL); dr.push(0.5 * (inR + m) + fR); }
        const double mid = 0.5 * (wl + wr), side = 0.5 * (wl - wr) * std::clamp(p.width, 0.0, 1.0);
        outL = mid + side; outR = mid - side;
    }
private:
    double sampleRate = 48000, tgtL = 250, tgtR = 250, curL = -1, curR = -1, fbk = 0.3, fl = 0, fr = 0;
    DelayLine dl, dr; Biquad hpL, hpR, lpL, lpR; Params p;
};

// ---------------------------------------------------------------- chorus / micro-pitch (wet only)
class Chorus {
public:
    struct Params { double rateHz = 0.6, depthMs = 2.5, delayMs = 12; };
    void prepare(double sr) { sampleRate = sr; dl.prepare(size_t(sr * 0.06)); dr.prepare(size_t(sr * 0.06)); reset(); }
    void reset() { dl.reset(); dr.reset(); ph = 0; }
    void set(const Params& np) { p = np; }
    inline void process(double inL, double inR, double& outL, double& outR) {
        dl.push(inL); dr.push(inR);
        ph += p.rateHz / sampleRate; if (ph >= 1.0) ph -= 1.0;
        const double s1 = std::sin(2 * kPi * ph), s2 = std::sin(2 * kPi * (ph + 0.5));
        const double base = p.delayMs * 1e-3 * sampleRate, dep = p.depthMs * 1e-3 * sampleRate;
        outL = dl.read(std::max(2.0, base + dep * s1)); outR = dr.read(std::max(2.0, base + dep * s2));
    }
private:
    double sampleRate = 48000, ph = 0; DelayLine dl, dr; Params p;
};

// ---------------------------------------------------------------- vocal doubler (wet only: two detuned, delayed voices)
class Doubler {
public:
    void prepare(double sr) { sampleRate = sr; dl.prepare(size_t(sr * 0.08)); dr.prepare(size_t(sr * 0.08)); reset(); }
    void reset() { dl.reset(); dr.reset(); p1 = 0; p2 = 0.37; p3 = 0.71; p4 = 0.13; }
    inline void process(double in, double& outL, double& outR) {   // mono-sum in, stereo out
        dl.push(in); dr.push(in);
        p1 = wrap(p1 + 0.31 / sampleRate); p2 = wrap(p2 + 0.47 / sampleRate); p3 = wrap(p3 + 0.23 / sampleRate); p4 = wrap(p4 + 0.61 / sampleRate);
        const double ms = 1e-3 * sampleRate;
        const double dL = 21.0 * ms + 1.6 * ms * (std::sin(2 * kPi * p1) + 0.5 * std::sin(2 * kPi * p2));
        const double dR = 29.0 * ms + 1.6 * ms * (std::sin(2 * kPi * p3) + 0.5 * std::sin(2 * kPi * p4));
        outL = dl.read(dL); outR = dr.read(dR);
    }
private:
    static double wrap(double x) { return x >= 1.0 ? x - 1.0 : x; }
    double sampleRate = 48000, p1 = 0, p2 = 0, p3 = 0, p4 = 0; DelayLine dl, dr;
};

// ---------------------------------------------------------------- stereo width (mono-safe: lows stay centred)
class Width {
public:
    void prepare(double sr) { sampleRate = sr; set(1.0); reset(); }
    void reset() { hp.reset(); }
    void set(double w, double monoBelowHz = 150.0) { width = std::clamp(w, 0.0, 2.0); hp.set(Biquad::Type::HighPass, sampleRate, monoBelowHz, 0.7071); }
    inline void process(double& l, double& r) {
        const double m = 0.5 * (l + r), s = 0.5 * (l - r);
        const double sHigh = hp.process(s);              // only the part of the side signal above the corner is scaled
        const double sOut = (s - sHigh) * std::min(1.0, width) + sHigh * width;
        l = m + sOut; r = m - sOut;
    }
private:
    double sampleRate = 48000, width = 1.0; Biquad hp;
};

// ---------------------------------------------------------------- harmonic exciter (adds 2nd/3rd harmonics of the highs)
class Exciter {
public:
    void prepare(double sr) { sampleRate = sr; atk = 1.0 - std::exp(-1.0 / (sr * 0.001)); rel = 1.0 - std::exp(-1.0 / (sr * 0.040)); set(0.0, 2800.0); reset(); }
    void reset() { hp.reset(); env = 0; dc = 0; }
    void set(double amt, double fromHz) { amount = std::clamp(amt, 0.0, 1.0); hp.set(Biquad::Type::HighPass, sampleRate, fromHz, 0.7071); }
    // Level-independent: the high band is normalised, shaped (asymmetric curve -> 2nd + 3rd harmonics),
    // and only the generated harmonic residue is added back at the band's own level.
    inline double process(double x) {
        if (amount <= 0.001) return x;
        const double h = hp.process(x), a = std::abs(h);
        env += (a > env ? atk : rel) * (a - env);
        const double e = std::max(env, 1e-5);
        const double u = h / e * 0.6;
        double r = std::tanh(u + 0.35 * u * u) - u;
        dc += 0.002 * (r - dc); r -= dc;
        const double gate = std::min(1.0, env / 0.002);       // do not excite the noise floor
        return x + amount * 7.0 * r * e / 0.6 * gate;
    }
private:
    double sampleRate = 48000, amount = 0, env = 0, dc = 0, atk = 0.1, rel = 0.001; Biquad hp;
};

} // namespace vonox::dsp
