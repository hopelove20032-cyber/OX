#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>
#include "Patches.h"

namespace ox
{
// State shared between the processor, the editor and all voices.
struct Shared
{
    std::atomic<float>* voiceParam   = nullptr; // choice index
    std::atomic<float>* attackParam  = nullptr; // 0..1 (0.5 = patch default)
    std::atomic<float>* releaseParam = nullptr; // 0..1 (0.5 = patch default)
    std::atomic<float> bend { 0.0f };           // -1..1
    std::atomic<float> mod  { 0.0f };           // 0..1
};

struct Sound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Cytomic/TPT state variable filter
struct SVF
{
    float s1 = 0.0f, s2 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.4f;

    void reset() { s1 = s2 = 0.0f; }

    void set (float cutoff, float q, double sr)
    {
        cutoff = juce::jlimit (20.0f, (float) (sr * 0.45), cutoff);
        const float g = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sr);
        k  = 1.0f / juce::jmax (0.3f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // returns low-pass, band-pass (un-normalised) in 'band'
    inline float process (float v0, float& band)
    {
        const float v3 = v0 - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;
        band = v1;
        return v2;
    }

    inline float lp (float v0) { float b; return process (v0, b); }
};

class OxVoice : public juce::SynthesiserVoice
{
public:
    explicit OxVoice (Shared& s);

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<Sound*> (s) != nullptr; }
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    using juce::SynthesiserVoice::renderNextBlock;
    void setCurrentPlaybackSampleRate (double newRate) override;

private:
    struct Mode { float re = 0.0f, im = 0.0f, c = 1.0f, s = 0.0f, d = 1.0f; };

    void initModal();  void renderModal (float& l, float& r);
    void initPluck();  void renderPluck (float& l, float& r);
    void initSub();    void renderSub   (float& l, float& r);
    void initFM();     void renderFM    (float& l, float& r);
    void initWind();   void renderWind  (float& l, float& r);
    void initOrgan();  void renderOrgan (float& l, float& r);

    void addMode (float ratio, float amp, float decaySec);
    float noiseSample();

    static constexpr int kMaxOsc = 7;

    Shared& shared;
    const Patch* patch = nullptr;
    double sr = 44100.0;
    float freq = 440.0f, vel = 1.0f, outGain = 1.0f;
    float bendRatio = 1.0f, modAmt = 0.0f;
    int noteNumber = 60;
    juce::int64 age = 0;
    int silent = 0, silentLimit = 13000;
    juce::ADSR env;
    juce::Random rng;

    // short noise transient (hammer / pick / key click)
    float noiseEnv = 0.0f, noiseDecay = 0.99f, noiseK = 0.5f, noiseLP = 0.0f;

    // shared low-frequency oscillators
    float lfo = 0.0f, lfo2 = 0.0f;

    // modal
    std::array<Mode, 32> modes;
    int numModes = 0;
    bool modalTremolo = false;
    float gL = 1.0f, gR = 1.0f;

    // plucked string
    std::vector<float> line;
    int plSize = 2, plPos = 0;
    float plPrev = 0.0f, plApX = 0.0f, plApY = 0.0f, plApC = 0.0f, plGain = 0.99f, plDamp = 0.5f;
    SVF cab;

    // subtractive
    std::array<float, kMaxOsc> oscPh {}, oscRatio {}, oscGL {}, oscGR {};
    int numOsc = 1;
    float subPh = 0.0f, pwmPh = 0.0f, fenv = 1.0f, fDecayMul = 0.999f;
    int filtCounter = 0;
    SVF fL, fR;
    std::array<SVF, 3> formant;

    // FM
    float fmR1 = 1.0f, fmR2 = 1.0f, fmI1 = 1.0f, fmI2 = 1.0f;
    float fmE1 = 1.0f, fmE2 = 1.0f, fmD1 = 1.0f, fmD2 = 1.0f;
    float fmAmp = 1.0f, fmAmpMul = 1.0f, fmLevel = 0.5f;
    int fmCopies = 1;
    float fph[2][3] = {};
    float fmDet[2] = { 1.0f, 1.0f };
    float scoopCents = 0.0f, scoopMul = 0.999f;

    // wind
    float wph = 0.0f, breathLevel = 0.05f, chiffEnv = 0.0f, chiffMul = 0.99f;
    std::array<float, 8> wHarm {};
    SVF breathF;

    // organ
    float oph = 0.0f, percEnv = 0.0f, percDecay = 0.999f, percAmp = 0.0f;
    float lesDepth = 0.1f, organNorm = 0.2f, organDrive = 0.0f;
    std::array<float, 9> bars {};
};
} // namespace ox
