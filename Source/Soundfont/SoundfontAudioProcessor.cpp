/*
Adapted from the original juce-soundfonts demo app (by Chris Penny)
https://github.com/cpenny42/juce-soundfonts
*/

#include "SoundfontAudioProcessor.h"

//==============================================================================
//==============================================================================
SoundfontAudioProcessor::SoundfontAudioProcessor(int numberOfVoices)
{
    settings = new_fluid_settings();
    synth = new_fluid_synth(settings);
    
    fluid_synth_set_polyphony(synth, numberOfVoices);
    setGain(1.0f);
}

SoundfontAudioProcessor::~SoundfontAudioProcessor()
{
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

//==============================================================================
const juce::String SoundfontAudioProcessor::getName() const
{
    return "SoundfontAudioProcessor";
}

bool SoundfontAudioProcessor::acceptsMidi() const
{
    return true;
}

bool SoundfontAudioProcessor::producesMidi() const
{
    return false;
}

bool SoundfontAudioProcessor::isMidiEffect() const
{
    return false;
}

double SoundfontAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SoundfontAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SoundfontAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SoundfontAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String SoundfontAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void SoundfontAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void SoundfontAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

bool SoundfontAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void SoundfontAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        juce::ignoreUnused (channelData);
        // ..do something to the data...
    }
}

//==============================================================================
bool SoundfontAudioProcessor::hasEditor() const
{
    return false; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SoundfontAudioProcessor::createEditor()
{
    return nullptr;
}

//==============================================================================
void SoundfontAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void SoundfontAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

void SoundfontAudioProcessor::releaseResources()
{
    systemReset();
}

//==============================================================================
bool SoundfontAudioProcessor::loadSoundfont(const juce::File file)
{
    if (file == loadedSoundfont) {
        // Don't reload an already loaded soundfont
        return false;
    }
    loadedSoundfont = file;
    
    // Lock while switching soundfonts
    const juce::ScopedLock l (lock);
    
    // All notes off
    fluid_synth_system_reset(synth);
    
    // If a soundfont is already loaded, unload the previous one (this demo doesn't go into banks, etc).
    if (fluid_synth_sfcount(synth) > 0) {
        int err = fluid_synth_sfunload(synth, (unsigned int) sfontID, true);
        if (err == -1) {
            return false;
        }
    }
    
    // Load the soundfont, store the handle
    sfontID = fluid_synth_sfload(synth, file.getFullPathName().toRawUTF8(), true);
    return (sfontID != -1);
}

void SoundfontAudioProcessor::processMidi (const juce::MidiMessage& message)
{
    if (message.isNoteOn()) {
        noteOn(message.getNoteNumber(), message.getVelocity());
    }
    else if (message.isNoteOff()) {
        noteOff(message.getNoteNumber());
    }
    else if (message.isController()) {
        cc(message.getControllerNumber(), message.getControllerValue());
    }
    else if (message.isPitchWheel()) {
        pitchBend(message.getPitchWheelValue());
    }
    else if (message.isChannelPressure()) {
        channelPressure(message.getChannelPressureValue());
    }
    // Add support for other types of MIDI messages here
}

void SoundfontAudioProcessor::noteOn (int note, int velocity, int channel)
{
    // Actually do note off if the velocity is 0
    velocity == 0
        ? fluid_synth_noteoff(synth, channel, note)
        : fluid_synth_noteon(synth, channel, note, velocity);
}

void SoundfontAudioProcessor::noteOff (int note, int channel)
{
    fluid_synth_noteoff(synth, channel, note);
}

void SoundfontAudioProcessor::cc(int control, int value, int channel)
{
    fluid_synth_cc(synth, channel, control, value);
}

int SoundfontAudioProcessor::getCc(int control, int channel)
{
    int value = 0;
    fluid_synth_get_cc(synth, channel, control, &value);
    return value;
}

void SoundfontAudioProcessor::pitchBend(int value, int channel)
{
    fluid_synth_pitch_bend(synth, channel, value);
}

int SoundfontAudioProcessor::getPitchBend(int channel)
{
    int value = 0;
    fluid_synth_get_pitch_bend(synth, channel, &value);
    return value;
}

void SoundfontAudioProcessor::setPitchBendRange(int value, int channel)
{
    fluid_synth_pitch_wheel_sens(synth, channel, value);
}

int SoundfontAudioProcessor::getPitchBendRange(int channel)
{
    int value = 0;
    fluid_synth_get_pitch_wheel_sens(synth, channel, &value);
    return value;
}

void SoundfontAudioProcessor::channelPressure(int value, int channel)
{
    fluid_synth_channel_pressure(synth, channel, value);
}

void SoundfontAudioProcessor::setGain (float gain)
{
    fluid_synth_set_gain(synth, gain);
}

float SoundfontAudioProcessor::getGain()
{
    return fluid_synth_get_gain(synth);
}

void SoundfontAudioProcessor::systemReset()
{
    fluid_synth_system_reset(synth);
}