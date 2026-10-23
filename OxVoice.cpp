#include "OxVoice.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace ox
{
namespace
{
constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;
constexpr float kCent  = 0.00057762265f; // ln(2) / 1200

struct MD { float ratio, amp, decay; };

// Mallet / bell mode tables: ratio, amplitude, decay (seconds, at ~440 Hz)
const MD kXylo[]     = { { 1.0f, 1.0f, 0.45f }, { 3.0f, 0.55f, 0.18f }, { 6.0f, 0.28f, 0.09f }, { 10.0f, 0.12f, 0.05f } };
const MD kMarimba[]  = { { 1.0f, 1.0f, 1.20f }, { 4.0f, 0.40f, 0.35f }, { 10.0f, 0.12f, 0.12f } };
const MD kVibes[]    = { { 1.0f, 1.0f, 3.50f }, { 4.0f, 0.30f, 1.20f }, { 10.0f, 0.09f, 0.40f } };
const MD kGlock[]    = { { 1.0f, 1.0f, 1.80f }, { 2.76f, 0.55f, 1.00f }, { 5.40f, 0.40f, 0.50f }, { 8.93f, 0.25f, 0.25f } };
const MD kKalimba[]  = { { 1.0f, 1.0f, 1.40f }, { 6.27f, 0.25f, 0.18f }, { 17.55f, 0.05f, 0.05f } };
const MD kMusicBox[] = { { 1.0f, 1.0f, 1.20f }, { 2.76f, 0.45f, 0.70f }, { 5.40f, 0.30f, 0.35f }, { 8.93f, 0.18f, 0.18f } };
const MD kTubular[]  = { { 0.5f, 0.35f, 4.5f }, { 1.0f, 1.0f, 3.8f }, { 1.19f, 0.55f, 3.0f }, { 1.5f, 0.40f, 2.4f },
                         { 2.0f, 0.45f, 1.8f }, { 2.5f, 0.18f, 1.0f }, { 3.0f, 0.10f, 0.6f } };
const MD kCelesta[]  = { { 1.0f, 1.0f, 1.80f }, { 2.76f, 0.20f, 0.80f }, { 5.40f, 0.08f, 0.35f } };

inline float centsToMul (float c) { return std::pow (2.0f, c / 1200.0f); }
inline float wrap01 (float x)     { return x - std::floor (x); }

// per-sample multiplier giving a 60 dB decay over 'seconds'
inline float t60Mul (float seconds, double sr)
{
    return seconds > 0.0f ? (float) std::exp (-6.907755 / ((double) seconds * sr)) : 1.0f;
}

inline float polyBlep (float t, float dt)
{
    if (t < dt)         { t /= dt;               return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt)  { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
    return 0.0f;
}

inline float sawBL (float ph, float inc) { return 2.0f * ph - 1.0f - polyBlep (ph, inc); }
} // namespace

//==============================================================================
OxVoice::OxVoice (Shared& s) : shared (s)
{
    OxVoice::setCurrentPlaybackSampleRate (44100.0);
}

void OxVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    sr = newRate > 0.0 ? newRate : 44100.0;
    line.assign ((size_t) (sr / 20.0) + 16, 0.0f);
    env.setSampleRate (sr);
    silentLimit = (int) (sr * 0.3);
}

float OxVoice::noiseSample()
{
    if (noiseEnv < 1.0e-5f)
        return 0.0f;

    const float n = rng.nextFloat() * 2.0f - 1.0f;
    noiseLP += noiseK * (n - noiseLP);
    noiseEnv *= noiseDecay;
    return noiseLP * noiseEnv;
}

//==============================================================================
void OxVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    const auto& list = patches();
    const float vp = shared.voiceParam != nullptr ? shared.voiceParam->load() : 0.0f;
    const int idx = juce::jlimit (0, (int) list.size() - 1, (int) std::lround (vp));
    patch = &list[(size_t) idx];

    noteNumber = midiNote;
    freq = juce::jmax (20.0f, (float) juce::MidiMessage::getMidiNoteInHertz (midiNote));
    vel = juce::jlimit (0.05f, 1.0f, velocity);
    outGain = patch->gain;
    age = 0;
    silent = 0;
    lfo = lfo2 = 0.0f;
    noiseEnv = 0.0f;
    noiseLP = 0.0f;

    const float a = shared.attackParam  != nullptr ? shared.attackParam->load()  : 0.5f;
    const float r = shared.releaseParam != nullptr ? shared.releaseParam->load() : 0.5f;
    const float aMul = std::pow (10.0f, (a - 0.5f) * 2.0f);
    const float rMul = std::pow (10.0f, (r - 0.5f) * 2.0f);

    env.setSampleRate (sr);
    env.setParameters (juce::ADSR::Parameters (juce::jmax (0.0005f, patch->attack * aMul), 0.05f, 1.0f,
                                               juce::jmax (0.005f, patch->release * rMul)));
    env.noteOn();

    switch (patch->engine)
    {
        case Engine::Modal:       initModal(); break;
        case Engine::Pluck:       initPluck(); break;
        case Engine::Subtractive: initSub();   break;
        case Engine::FM:          initFM();    break;
        case Engine::Wind:        initWind();  break;
        case Engine::Organ:       initOrgan(); break;
    }
}

void OxVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        env.noteOff();
    }
    else
    {
        clearCurrentNote();
        env.reset();
    }
}

void OxVoice::renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    if (patch == nullptr || ! isVoiceActive())
        return;

    bendRatio = std::pow (2.0f, (shared.bend.load() * 2.0f) / 12.0f);
    modAmt = shared.mod.load();

    float* L = out.getWritePointer (0, startSample);
    float* R = out.getNumChannels() > 1 ? out.getWritePointer (1, startSample) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float l = 0.0f, r = 0.0f;

        switch (patch->engine)
        {
            case Engine::Modal:       renderModal (l, r); break;
            case Engine::Pluck:       renderPluck (l, r); break;
            case Engine::Subtractive: renderSub   (l, r); break;
            case Engine::FM:          renderFM    (l, r); break;
            case Engine::Wind:        renderWind  (l, r); break;
            case Engine::Organ:       renderOrgan (l, r); break;
        }

        const float e = env.getNextSample() * outGain;
        l *= e;
        r *= e;

        L[i] += l;
        if (R != nullptr)
            R[i] += r;

        ++age;

        if (! env.isActive())
        {
            clearCurrentNote();
            break;
        }

        if (std::abs (l) + std::abs (r) < 1.0e-6f)
        {
            if (++silent > silentLimit)
            {
                clearCurrentNote();
                env.reset();
                break;
            }
        }
        else
        {
            silent = 0;
        }
    }
}

//==============================================================================
// MODAL: mallets, bells, piano (bank of damped resonators)
//==============================================================================
void OxVoice::addMode (float ratio, float amp, float decaySec)
{
    if (numModes >= (int) modes.size())
        return;

    const float f = freq * ratio;
    if (f > (float) sr * 0.45f)
        return;

    const double w = 2.0 * (double) kPi * (double) f / sr;
    auto& m = modes[(size_t) numModes++];
    m.c = (float) std::cos (w);
    m.s = (float) std::sin (w);
    m.d = t60Mul (juce::jmax (0.01f, decaySec), sr);
    m.re = amp;
    m.im = 0.0f;
}

void OxVoice::initModal()
{
    numModes = 0;
    modalTremolo = false;

    const int v = patch->variant;
    const float gain = 0.25f * (0.35f + 0.65f * vel);
    const float dScale = patch->decay * juce::jlimit (0.4f, 3.0f, std::pow (440.0f / freq, 0.3f));
    const float tone = patch->bright * 2.0f;

    float nLevel = 0.1f, nDecay = 0.004f, nBright = 0.5f;
    const MD* tab = nullptr;
    int n = 0;

    switch (v)
    {
        case 0: tab = kXylo;     n = (int) std::size (kXylo);     nLevel = 0.35f; nDecay = 0.004f; nBright = 0.60f; break;
        case 1: tab = kMarimba;  n = (int) std::size (kMarimba);  nLevel = 0.15f; nDecay = 0.006f; nBright = 0.25f; break;
        case 2: tab = kVibes;    n = (int) std::size (kVibes);    nLevel = 0.08f; nDecay = 0.004f; nBright = 0.40f; modalTremolo = true; break;
        case 3: tab = kGlock;    n = (int) std::size (kGlock);    nLevel = 0.10f; nDecay = 0.002f; nBright = 0.70f; break;
        case 4: tab = kKalimba;  n = (int) std::size (kKalimba);  nLevel = 0.12f; nDecay = 0.003f; nBright = 0.50f; break;
        case 5: tab = kMusicBox; n = (int) std::size (kMusicBox); nLevel = 0.05f; nDecay = 0.002f; nBright = 0.60f; break;
        case 6: tab = kTubular;  n = (int) std::size (kTubular);  nLevel = 0.12f; nDecay = 0.010f; nBright = 0.30f; break;
        case 7: tab = kCelesta;  n = (int) std::size (kCelesta);  nLevel = 0.06f; nDecay = 0.002f; nBright = 0.60f; break;
        default: break;
    }

    if (tab != nullptr)
    {
        for (int i = 0; i < n; ++i)
        {
            float amp = tab[i].amp * gain * std::pow (vel, 0.3f * (float) i);
            if (i > 0)
                amp *= tone;
            addMode (tab[i].ratio, amp, tab[i].decay * dScale);
        }
    }
    else
    {
        // Piano family: inharmonic partials, two slightly detuned strings per partial
        float B = 0.00035f, tilt = 1.0f, T0 = 1.0f;
        switch (v)
        {
            case 8:  B = 0.00035f; tilt = 1.00f; T0 = 1.0f; break;
            case 9:  B = 0.00030f; tilt = 0.60f; T0 = 1.0f; break;
            case 10: B = 0.00040f; tilt = 1.30f; T0 = 0.7f; break;
            default: B = 0.00060f; tilt = 0.90f; T0 = 0.8f; break;
        }

        const float T = juce::jlimit (1.2f, 14.0f, 7.0f * std::pow (261.63f / freq, 0.5f)) * T0 * patch->decay;

        for (int p = 1; p <= 14; ++p)
        {
            const float pf = (float) p;
            const float ratio = pf * std::sqrt (1.0f + B * pf * pf);
            float amp = std::pow (pf, -tilt) * gain * std::pow (vel, 0.35f * (pf - 1.0f));
            if (p > 1)
                amp *= tone;
            const float dec = T / (1.0f + 0.55f * (pf - 1.0f));

            if (p <= 8)
            {
                const float half = patch->detune * 0.5f;
                addMode (ratio * centsToMul (-half), amp * 0.5f, dec);
                addMode (ratio * centsToMul (half),  amp * 0.5f, dec);
            }
            else
            {
                addMode (ratio, amp, dec);
            }
        }

        nLevel = 0.12f; nDecay = 0.008f; nBright = 0.35f;
    }

    noiseEnv   = nLevel * vel;
    noiseDecay = t60Mul (nDecay * 3.0f, sr);
    noiseK     = juce::jlimit (0.02f, 0.95f, nBright);

    const float pan = juce::jlimit (-0.3f, 0.3f, ((float) noteNumber - 60.0f) / 60.0f * 0.3f);
    gL = 1.0f - pan;
    gR = 1.0f + pan;
}

void OxVoice::renderModal (float& l, float& r)
{
    float s = 0.0f;

    for (int m = 0; m < numModes; ++m)
    {
        auto& md = modes[(size_t) m];
        const float nre = md.d * (md.re * md.c - md.im * md.s);
        const float nim = md.d * (md.re * md.s + md.im * md.c);
        md.re = nre;
        md.im = nim;
        s += nim;
    }

    s += noiseSample();

    if (modalTremolo)
    {
        lfo += 5.0f / (float) sr;
        if (lfo >= 1.0f) lfo -= 1.0f;
        s *= 1.0f - 0.3f * (0.5f + 0.5f * std::sin (kTwoPi * lfo));
    }

    l = s * gL;
    r = s * gR;
}

//==============================================================================
// PLUCK: Karplus-Strong string with tuned all-pass, pick position, cabinet
//==============================================================================
void OxVoice::initPluck()
{
    const int v = juce::jlimit (0, 6, patch->variant);
    static const float pickPos[7] = { 0.13f, 0.22f, 0.16f, 0.12f, 0.35f, 0.08f, 0.18f };
    static const float cabHz[7]   = { 7500.0f, 5500.0f, 4500.0f, 3000.0f, 9000.0f, 9000.0f, 2500.0f };

    const float bright = patch->bright;
    plDamp = 1.0f - bright * 0.85f;

    const double D = sr / (double) freq - 0.5 * (double) plDamp;
    int N = (int) std::floor (D - 0.5);
    N = juce::jlimit (2, (int) line.size() - 2, N);
    const float frac = (float) (D - (double) N);
    plApC = (1.0f - frac) / (1.0f + frac);
    plSize = N;
    plPos = 0;
    plPrev = plApX = plApY = 0.0f;

    plGain = std::pow (10.0f, -3.0f / (freq * juce::jmax (0.05f, patch->decay)));
    plGain = juce::jmin (plGain, 0.99995f);

    // excitation: low-passed noise burst, pick-position comb, normalised
    const float kEx = juce::jlimit (0.02f, 0.98f, 0.12f + bright * 0.85f * (0.4f + 0.6f * vel));
    float x = 0.0f, mean = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        const float nz = rng.nextFloat() * 2.0f - 1.0f;
        x += kEx * (nz - x);
        line[(size_t) i] = x;
        mean += x;
    }
    mean /= (float) N;
    for (int i = 0; i < N; ++i)
        line[(size_t) i] -= mean;

    const int pp = juce::jmax (1, (int) ((float) N * pickPos[v]));
    for (int i = N - 1; i >= pp; --i)
        line[(size_t) i] -= line[(size_t) (i - pp)];

    float peak = 1.0e-6f;
    for (int i = 0; i < N; ++i)
        peak = juce::jmax (peak, std::abs (line[(size_t) i]));
    const float sc = (0.35f + 0.65f * vel) / peak;
    for (int i = 0; i < N; ++i)
        line[(size_t) i] *= sc;

    cab.reset();
    cab.set (cabHz[v], 0.8f, sr);

    const bool muted = (v == 3);
    noiseEnv   = (muted ? 0.25f : 0.04f) * vel;
    noiseDecay = t60Mul (muted ? 0.03f : 0.006f, sr);
    noiseK     = muted ? 0.06f : 0.5f;
}

void OxVoice::renderPluck (float& l, float& r)
{
    const float y = line[(size_t) plPos];

    const float filtered = y * (1.0f - 0.5f * plDamp) + 0.5f * plDamp * plPrev;
    plPrev = y;

    const float ap = plApC * filtered + plApX - plApC * plApY;
    plApX = filtered;
    plApY = ap;

    line[(size_t) plPos] = ap * plGain;
    if (++plPos >= plSize)
        plPos = 0;

    float o = y;
    if (patch->drive > 0.0f)
        o = std::tanh (o * (1.0f + patch->drive * 18.0f)) * 0.7f;
    else
        o *= 0.6f;

    float band;
    o = cab.process (o, band);
    o += noiseSample();

    l = r = o;
}

//==============================================================================
// SUBTRACTIVE: analog-style leads, pads, strings, brass, bass, choir
//==============================================================================
void OxVoice::initSub()
{
    const int v = juce::jlimit (0, 7, patch->variant);
    static const int oscCount[8] = { 2, 7, 5, 5, 3, 2, 2, 3 };
    numOsc = oscCount[v];

    const bool randomPhase = (v == 1 || v == 2 || v == 3 || v == 7);
    for (int k = 0; k < numOsc; ++k)
    {
        const float pos = numOsc > 1 ? (2.0f * (float) k / (float) (numOsc - 1) - 1.0f) : 0.0f;
        oscRatio[(size_t) k] = centsToMul (pos * patch->detune * 0.5f);
        oscPh[(size_t) k] = randomPhase ? rng.nextFloat() : 0.0f;
        const float pan = pos * 0.7f;
        oscGL[(size_t) k] = std::cos ((pan + 1.0f) * kPi * 0.25f);
        oscGR[(size_t) k] = std::sin ((pan + 1.0f) * kPi * 0.25f);
    }

    subPh = 0.0f;
    pwmPh = rng.nextFloat();
    fenv = 1.0f;
    fDecayMul = t60Mul (juce::jmax (0.02f, patch->decay) * 0.5f, sr);
    filtCounter = 0;
    fL.reset();
    fR.reset();
    for (auto& f : formant)
        f.reset();
}

void OxVoice::renderSub (float& l, float& r)
{
    const int v = patch->variant;
    const float tSec = (float) age / (float) sr;

    // vibrato (delayed onset, mod wheel adds depth)
    const float vibRamp = juce::jlimit (0.0f, 1.0f, (tSec - 0.2f) * 2.5f);
    const float vibCents = patch->vib * vibRamp + modAmt * 25.0f;
    lfo += 5.4f / (float) sr;
    if (lfo >= 1.0f) lfo -= 1.0f;
    const float vibMul = 1.0f + vibCents * kCent * std::sin (kTwoPi * lfo);
    const float baseInc = freq * bendRatio * vibMul / (float) sr;

    pwmPh += 0.6f / (float) sr;
    if (pwmPh >= 1.0f) pwmPh -= 1.0f;

    float L = 0.0f, R = 0.0f;
    for (int k = 0; k < numOsc; ++k)
    {
        const float inc = baseInc * oscRatio[(size_t) k];
        float ph = oscPh[(size_t) k];
        float s;

        if (v == 6)
        {
            const float pw = wrap01 (ph + 0.5f + 0.3f * std::sin (kTwoPi * pwmPh));
            s = 0.5f * (sawBL (ph, inc) - sawBL (pw, inc));
        }
        else
        {
            s = sawBL (ph, inc);
        }

        ph += inc;
        if (ph >= 1.0f) ph -= 1.0f;
        oscPh[(size_t) k] = ph;

        L += s * oscGL[(size_t) k];
        R += s * oscGR[(size_t) k];
    }

    const float norm = 0.9f / std::sqrt ((float) numOsc);
    L *= norm;
    R *= norm;

    if (v == 5) // sub oscillator for bass
    {
        const float sb = std::sin (kTwoPi * subPh) * 0.5f;
        subPh = wrap01 (subPh + baseInc * 0.5f);
        L += sb;
        R += sb;
    }

    // filter envelope
    if (v == 4)
        fenv = 1.0f - std::exp (-tSec * 7.0f);   // brass swell
    else
        fenv *= fDecayMul;

    if (filtCounter-- <= 0)
    {
        filtCounter = 15;

        const float base = 80.0f * std::pow (2.0f, patch->bright * 8.0f);
        float cutoff = base * std::pow (2.0f, fenv * patch->fltEnv) * std::pow (freq / 261.63f, 0.4f);
        cutoff *= (0.6f + 0.8f * vel);
        fL.set (cutoff, patch->res, sr);
        fR.set (cutoff, patch->res, sr);

        if (v == 7)
        {
            formant[0].set (730.0f - 330.0f * modAmt, 6.0f, sr);
            formant[1].set (1090.0f - 500.0f * modAmt, 6.0f, sr);
            formant[2].set (2440.0f, 6.0f, sr);
        }
    }

    if (v == 7) // choir: formant filters (mod wheel morphs Ah -> Oh)
    {
        const float mono = 0.5f * (L + R);
        float b1, b2, b3;
        formant[0].process (mono, b1);
        formant[1].process (mono, b2);
        formant[2].process (mono, b3);
        const float ch = (b1 * formant[0].k + 0.7f * b2 * formant[1].k + 0.35f * b3 * formant[2].k) * 3.0f;
        l = ch + 0.12f * fL.lp (L);
        r = ch + 0.12f * fR.lp (R);
        return;
    }

    l = fL.lp (L);
    r = fR.lp (R);
}

//==============================================================================
// FM: electric pianos, fluid voices, bells, clav
//==============================================================================
void OxVoice::initFM()
{
    const int v = patch->variant;
    const float tone = patch->bright * 2.0f;
    float t1 = 1.5f, t2 = 0.05f;

    fmCopies = (v == 2 || v == 5) ? 2 : 1;

    switch (v)
    {
        case 0:  fmR1 = 1.0f;  fmR2 = 14.0f; fmI1 = 1.7f; fmI2 = 0.9f; t1 = 1.5f;  t2 = 0.04f; break; // Rhodes
        case 1:  fmR1 = 1.0f;  fmR2 = 7.0f;  fmI1 = 2.6f; fmI2 = 1.0f; t1 = 0.9f;  t2 = 0.06f; break; // DX
        case 2:  fmR1 = 2.0f;  fmR2 = 3.01f; fmI1 = 2.4f; fmI2 = 1.0f; t1 = 1.2f;  t2 = 0.35f; break; // Fluid keys
        case 3:  fmR1 = 3.5f;  fmR2 = 1.41f; fmI1 = 3.0f; fmI2 = 1.2f; t1 = 3.0f;  t2 = 1.5f;  break; // Bell
        case 4:  fmR1 = 2.0f;  fmR2 = 4.0f;  fmI1 = 3.0f; fmI2 = 1.2f; t1 = 0.15f; t2 = 0.07f; break; // Clav
        default: fmR1 = 2.0f;  fmR2 = 3.0f;  fmI1 = 1.4f; fmI2 = 0.5f; t1 = 0.0f;  t2 = 0.5f;  break; // Fluid lead
    }

    const float velBright = 0.45f + 0.9f * vel;
    fmI1 *= tone * velBright;
    fmI2 *= tone * velBright;
    fmE1 = fmE2 = 1.0f;
    fmD1 = t60Mul (t1, sr);
    fmD2 = t60Mul (t2, sr);
    fmAmp = 1.0f;
    fmAmpMul = t60Mul (patch->decay, sr);
    fmLevel = 0.5f * (0.35f + 0.65f * vel);

    for (auto& row : fph)
        for (auto& p : row)
            p = 0.0f;

    fmDet[0] = centsToMul (-patch->detune * 0.5f);
    fmDet[1] = centsToMul (patch->detune * 0.5f);

    scoopCents = (v == 2) ? 35.0f : (v == 5 ? -60.0f : 0.0f);
    scoopMul = t60Mul ((v == 2 ? 0.03f : 0.08f) * 3.0f, sr);
}

void OxVoice::renderFM (float& l, float& r)
{
    const int v = patch->variant;
    const float tSec = (float) age / (float) sr;

    lfo += (v == 0 ? 4.2f : 5.2f) / (float) sr;
    if (lfo >= 1.0f) lfo -= 1.0f;
    lfo2 += 0.7f / (float) sr;
    if (lfo2 >= 1.0f) lfo2 -= 1.0f;

    const float vibRamp = juce::jlimit (0.0f, 1.0f, (tSec - 0.2f) * 2.5f);
    const float vibCents = patch->vib * vibRamp + modAmt * 20.0f;
    scoopCents *= scoopMul;
    const float pitchMul = (1.0f + (vibCents * std::sin (kTwoPi * lfo) + scoopCents) * kCent) * bendRatio;

    float i1 = fmI1 * fmE1;
    if (v == 5)
        i1 *= 0.75f + 0.25f * std::sin (kTwoPi * lfo2);
    const float i2 = fmI2 * fmE2;

    float y[2] = { 0.0f, 0.0f };
    for (int c = 0; c < fmCopies; ++c)
    {
        const float inc = freq * pitchMul * fmDet[c] / (float) sr;
        float* p = fph[c];
        const float m1 = std::sin (kTwoPi * p[1]);
        const float m2 = std::sin (kTwoPi * p[2]);
        y[c] = std::sin (kTwoPi * p[0] + i1 * m1 + i2 * m2);

        p[0] = wrap01 (p[0] + inc);
        p[1] = wrap01 (p[1] + inc * fmR1);
        p[2] = wrap01 (p[2] + inc * fmR2);
    }

    fmE1 *= fmD1;
    fmE2 *= fmD2;
    fmAmp *= fmAmpMul;

    if (fmCopies == 2)
    {
        l = (0.75f * y[0] + 0.25f * y[1]) * fmAmp * fmLevel * 1.4f;
        r = (0.25f * y[0] + 0.75f * y[1]) * fmAmp * fmLevel * 1.4f;
    }
    else
    {
        l = r = y[0] * fmAmp * fmLevel * 1.4f;
    }

    if (v == 0) // suitcase-style auto-pan
    {
        const float pan = 0.15f * std::sin (kTwoPi * lfo);
        l *= 1.0f - pan;
        r *= 1.0f + pan;
    }
}

//==============================================================================
// WIND: ocarina, flute, pan flute, clarinet, whistle
//==============================================================================
void OxVoice::initWind()
{
    static const float harm[5][8] = {
        { 1.0f, 0.06f, 0.02f, 0.0f,  0.0f, 0.0f,  0.0f,  0.0f },               // ocarina
        { 1.0f, 0.35f, 0.12f, 0.05f, 0.0f, 0.0f,  0.0f,  0.0f },               // flute
        { 1.0f, 0.15f, 0.05f, 0.0f,  0.0f, 0.0f,  0.0f,  0.0f },               // pan flute
        { 1.0f, 0.02f, 0.55f, 0.02f, 0.3f, 0.02f, 0.15f, 0.0f },               // clarinet
        { 1.0f, 0.02f, 0.0f,  0.0f,  0.0f, 0.0f,  0.0f,  0.0f } };             // whistle
    static const float breath[5] = { 0.05f, 0.12f, 0.22f, 0.02f, 0.03f };
    static const float scoop[5]  = { -25.0f, -15.0f, 0.0f, 0.0f, 0.0f };
    static const float chiff[5]  = { 0.5f, 1.5f, 1.0f, 0.5f, 0.3f };

    const int v = juce::jlimit (0, 4, patch->variant);
    for (int i = 0; i < 8; ++i)
        wHarm[(size_t) i] = harm[v][i];

    wph = 0.0f;
    breathLevel = breath[v];
    scoopCents = scoop[v];
    scoopMul = t60Mul (0.15f, sr);
    chiffEnv = chiff[v];
    chiffMul = t60Mul (0.1f, sr);

    breathF.reset();
    breathF.set (freq * 2.0f, 1.0f, sr);
}

void OxVoice::renderWind (float& l, float& r)
{
    const float tSec = (float) age / (float) sr;

    const float vibRamp = juce::jlimit (0.0f, 1.0f, (tSec - 0.25f) * 3.0f);
    const float vibCents = patch->vib * vibRamp + modAmt * 30.0f;
    lfo += 5.3f / (float) sr;
    if (lfo >= 1.0f) lfo -= 1.0f;

    scoopCents *= scoopMul;
    const float mul = (1.0f + (vibCents * std::sin (kTwoPi * lfo) + scoopCents) * kCent) * bendRatio;

    wph = wrap01 (wph + freq * mul / (float) sr);

    float s = 0.0f;
    for (int h = 0; h < 8; ++h)
        if (wHarm[(size_t) h] > 0.0f)
            s += wHarm[(size_t) h] * std::sin (kTwoPi * (float) (h + 1) * wph);

    const float n = rng.nextFloat() * 2.0f - 1.0f;
    float band;
    breathF.process (n, band);
    const float breathSig = band * breathF.k * breathLevel * (1.0f + 2.5f * chiffEnv);
    chiffEnv *= chiffMul;

    const float o = (s * 0.45f + breathSig) * (0.45f + 0.4f * vel);
    l = r = o;
}

//==============================================================================
// ORGAN: drawbar organs and pipe organ
//==============================================================================
void OxVoice::initOrgan()
{
    static const float drawbars[3][9] = {
        { 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },   // jazz 888000000
        { 1.0f, 1.0f, 1.0f, 0.8f, 0.0f, 0.6f, 0.0f, 0.0f, 0.7f },   // rock
        { 0.0f, 0.0f, 1.0f, 0.8f, 0.35f, 0.5f, 0.0f, 0.0f, 0.3f } };// church / pipe

    const int v = juce::jlimit (0, 2, patch->variant);
    float sum = 0.0f;
    for (int i = 0; i < 9; ++i)
    {
        bars[(size_t) i] = drawbars[v][i];
        sum += drawbars[v][i];
    }
    organNorm = 0.6f / juce::jmax (1.0f, sum);
    organDrive = patch->drive;
    oph = 0.0f;

    percAmp = (v == 0) ? 0.5f : 0.0f;
    percEnv = (v == 0) ? 1.0f : 0.0f;
    percDecay = t60Mul (0.5f, sr);

    lesDepth = (v == 0) ? 0.20f : (v == 1 ? 0.15f : 0.02f);

    noiseEnv   = (v == 2 ? 0.05f : 0.03f);
    noiseDecay = t60Mul (0.02f, sr);
    noiseK     = (v == 2 ? 0.3f : 0.7f);
}

void OxVoice::renderOrgan (float& l, float& r)
{
    static const float mult[9] = { 1.0f, 3.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 16.0f };

    lfo += 6.6f / (float) sr;
    if (lfo >= 1.0f) lfo -= 1.0f;

    const float vibM = 1.0f + 0.0006f * std::sin (kTwoPi * lfo) * (lesDepth > 0.05f ? 1.0f : 0.2f);
    oph = wrap01 (oph + freq * 0.5f * bendRatio * vibM / (float) sr);
    const float th = kTwoPi * oph;

    float s = 0.0f;
    for (int d = 0; d < 9; ++d)
        if (bars[(size_t) d] > 0.0f)
            s += bars[(size_t) d] * std::sin (th * mult[d]);
    s *= organNorm;

    if (percEnv > 1.0e-4f)
    {
        s += percAmp * 0.3f * percEnv * std::sin (th * 6.0f);
        percEnv *= percDecay;
    }

    s += noiseSample();

    if (organDrive > 0.0f)
        s = std::tanh (s * (1.0f + organDrive * 4.0f)) * 0.8f;

    const float am = lesDepth * std::sin (kTwoPi * lfo);
    l = s * (1.0f + am);
    r = s * (1.0f - am);
}

} // namespace ox
