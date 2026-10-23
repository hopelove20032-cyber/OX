#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "OxVoice.h"

class OxAudioProcessor : public juce::AudioProcessor
{
public:
    OxAudioProcessor();
    ~OxAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OX"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::MidiKeyboardState keyboardState;
    juce::AudioProcessorValueTreeState apvts;
    ox::Shared shared;

private:
    juce::Synthesiser synth;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::StateVariableTPTFilter<float> toneFilter;
    juce::Reverb reverb;

    std::atomic<float>* levelParam  = nullptr;
    std::atomic<float>* toneParam   = nullptr;
    std::atomic<float>* chorusParam = nullptr;
    std::atomic<float>* reverbParam = nullptr;
    float lastGain = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OxAudioProcessor)
};
