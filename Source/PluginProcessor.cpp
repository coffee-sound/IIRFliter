/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
IIRFilterAudioProcessor::IIRFilterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    apvts.addParameterListener("cutoff", this);
    apvts.addParameterListener("Q_value", this);
}

IIRFilterAudioProcessor::~IIRFilterAudioProcessor()
{
    apvts.removeParameterListener("cutoff", this);
    apvts.removeParameterListener("Q_value", this);
}

//==============================================================================
const juce::String IIRFilterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool IIRFilterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool IIRFilterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool IIRFilterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double IIRFilterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int IIRFilterAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int IIRFilterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void IIRFilterAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String IIRFilterAudioProcessor::getProgramName (int index)
{
    return {};
}

void IIRFilterAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void IIRFilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    float cutoff = apvts.getRawParameterValue("cutoff")->load();
    float Q = apvts.getRawParameterValue("Q_value")->load();
    updateFilter(cutoff, Q);
}

void IIRFilterAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool IIRFilterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void IIRFilterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    auto numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        auto& xBuffer = xBuffers[channel];
        auto& yBuffer = yBuffers[channel];

        for (int n = 0; n < numSamples; ++n) {

            float x0 = channelData[n];

            // culc IIR
            // y = \sum(-a_k * y[n-k]) + \sum(b_k * x[n-k])
            float y0 = (b0 * x0 + b1 * xBuffer[0] + b2 * xBuffer[1]
                - a1 * yBuffer[0] - a2 * yBuffer[1]) / a0;

            channelData[n] = y0;

            // shift history
            xBuffer[1] = xBuffer[0];
            xBuffer[0] = x0;
            yBuffer[1] = yBuffer[0];
            yBuffer[0] = y0;
        }
    }
}

//==============================================================================
bool IIRFilterAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* IIRFilterAudioProcessor::createEditor()
{
    return new IIRFilterAudioProcessorEditor (*this);
}

//==============================================================================
void IIRFilterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void IIRFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IIRFilterAudioProcessor();
}


void IIRFilterAudioProcessor::updateFilter(float cutoffFreq, float Q) {

    float fs = static_cast<float>(getSampleRate());
    // prewarping
    float omega = 2.0f * juce::MathConstants<float>::pi * (cutoffFreq / fs);
    float cos_omega = std::cos(omega);
    float sin_omega = std::sin(omega);
    // set variable
    float alpha = sin_omega / (2.0f * Q);

    // LPF
    b0 = (1.0f - cos_omega) / 2.0f;
    b1 = 1.0f - cos_omega;
    b2 = (1.0f - cos_omega) / 2.0f;
    a0 = 1.0f + alpha;
    a1 = -2.0f * cos_omega;
    a2 = 1.0f - alpha;
}

void IIRFilterAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "cutoff" || parameterID == "Q_value") {
        float cutoff = apvts.getRawParameterValue("cutoff")->load();
        float Q = apvts.getRawParameterValue("Q_value")->load();

        updateFilter(cutoff, Q);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout IIRFilterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "cutoff",
        "Freq",
        juce::NormalisableRange<float>{ 50.0f, 20000.0f, 1.0f, 0.5f },
        1000.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Q_value",
        "Q",
        juce::NormalisableRange<float>{ 0.1f, 20.0f, 0.01f, 0.5f },
        0.707f
    ));

    return layout;
}