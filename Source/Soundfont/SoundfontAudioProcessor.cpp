/*
Adapted from the original juce-soundfonts demo app (by Chris Penny)
https://github.com/cpenny42/juce-soundfonts
*/

#include "SoundfontAudioProcessor.h"

//==============================================================================
//==============================================================================
SoundfontAudioProcessor::SoundfontAudioProcessor(int numberOfVoices)
: AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  numVoices (numberOfVoices)
{
    settings = new_fluid_settings();
    synth = new_fluid_synth(settings);
    
    fluid_synth_set_polyphony(synth, numberOfVoices);
    setGain(0.6f);
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
    juce::ignoreUnused (samplesPerBlock);
    const juce::ScopedLock l (lock);

    if (! juce::exactlyEqual (sampleRate, synthSampleRate))
        rebuildSynth (sampleRate);
}

// FluidLite's fluid_synth_set_sample_rate() ignores its argument (it rebuilds the voices at the old rate),
// so the pitch is wrong at any rate other than 44.1 kHz. The rate only takes effect when the synth is created.
void SoundfontAudioProcessor::rebuildSynth (double sampleRate)
{
    const float gain = fluid_synth_get_gain (synth);

    delete_fluid_synth (synth);
    fluid_settings_setnum (settings, "synth.sample-rate", sampleRate);
    synth = new_fluid_synth (settings);
    synthSampleRate = sampleRate;

    fluid_synth_set_polyphony (synth, numVoices);
    fluid_synth_set_gain (synth, gain);

    if (loadedSoundfont != juce::File())
        sfontID = fluid_synth_sfload (synth, loadedSoundfont.getFullPathName().toRawUTF8(), true);
}

bool SoundfontAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

    void SoundfontAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();

        // Don't block the audio thread if a soundfont is being swapped
        const juce::ScopedTryLock l (lock);
        if (! l.isLocked())
            return;

        const int numSamples = buffer.getNumSamples();
        float* left  = buffer.getWritePointer (0);
        float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

        auto render = [&] (int start, int count)
        {
            if (count > 0)
                fluid_synth_write_float (synth, count, left + start, 0, 1, right + start, 0, 1);
        };

        // Sample-accurate MIDI: render up to each event, apply it, continue
        int pos = 0;
        for (const auto meta : midiMessages)
        {
            const int evPos = juce::jlimit (pos, numSamples, meta.samplePosition);
            render (pos, evPos - pos);
            pos = evPos;
            processMidi (meta.getMessage());
        }
        render (pos, numSamples - pos);
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

bool SoundfontAudioProcessor::loadSoundfontFromMemory (const void* data, size_t size,
                                                       const juce::String& fileName)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (fileName);
    return f.replaceWithData (data, size) && loadSoundfont (f);
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