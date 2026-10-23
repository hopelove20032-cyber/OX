#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout OxAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    StringArray names;
    for (const auto& p : ox::patches())
        names.add (p.name);

    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "voice", 1 }, "Voice", names, 0));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 },   "Level",   NormalisableRange<float> (0.0f, 1.0f), 0.7f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 },    "Tone",    NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 },  "Attack",  NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "chorus", 1 },  "Chorus",  NormalisableRange<float> (0.0f, 1.0f), 0.15f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reverb", 1 },  "Reverb",  NormalisableRange<float> (0.0f, 1.0f), 0.2f));

    return { params.begin(), params.end() };
}

OxAudioProcessor::OxAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OX_STATE", createLayout())
{
    shared.voiceParam   = apvts.getRawParameterValue ("voice");
    shared.attackParam  = apvts.getRawParameterValue ("attack");
    shared.releaseParam = apvts.getRawParameterValue ("release");
    levelParam  = apvts.getRawParameterValue ("level");
    toneParam   = apvts.getRawParameterValue ("tone");
    chorusParam = apvts.getRawParameterValue ("chorus");
    reverbParam = apvts.getRawParameterValue ("reverb");

    for (int i = 0; i < 16; ++i)
        synth.addVoice (new ox::OxVoice (shared));

    synth.addSound (new ox::Sound());
}

void OxAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    chorus.prepare (spec);
    chorus.setRate (0.7f);
    chorus.setDepth (0.35f);
    chorus.setCentreDelay (8.0f);
    chorus.setFeedback (0.0f);

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    lastGain = *levelParam * *levelParam * 1.5f;
}

bool OxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void OxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    buffer.clear();

    // on-screen keyboard -> MIDI
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (m.isPitchWheel())
            shared.bend.store ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isControllerOfType (1))
            shared.mod.store ((float) m.getControllerValue() / 127.0f);
    }

    synth.renderNextBlock (buffer, midi, 0, n);

    // tone (low-pass)
    const float tone = toneParam->load();
    if (tone < 0.995f)
    {
        toneFilter.setCutoffFrequency (500.0f * std::pow (40.0f, tone));
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        toneFilter.process (ctx);
    }

    // chorus
    const float ch = chorusParam->load();
    if (ch > 0.01f)
    {
        chorus.setMix (ch * 0.7f);
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chorus.process (ctx);
    }

    // reverb
    const float rv = reverbParam->load();
    if (rv > 0.005f && buffer.getNumChannels() >= 2)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.55f;
        rp.damping = 0.45f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        rp.wetLevel = rv * 0.35f;
        rp.dryLevel = 1.0f - rv * 0.15f;
        reverb.setParameters (rp);
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    }

    // level + safety clip
    const float lvl = levelParam->load();
    const float g = lvl * lvl * 1.5f;
    buffer.applyGainRamp (0, n, lastGain, g);
    lastGain = g;

    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        float* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            d[i] = juce::jlimit (-0.99f, 0.99f, d[i]);
    }
}

juce::AudioProcessorEditor* OxAudioProcessor::createEditor()
{
    return new OxEditor (*this);
}

void OxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OxAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OxAudioProcessor();
}
