/*
Adapted from the original juce-soundfonts demo app (by Chris Penny)
https://github.com/cpenny42/juce-soundfonts
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <fluidlite.h>

#include <JuceHeader.h>

#ifndef SF_NUM_VOICES
#define SF_NUM_VOICES 88
#endif

//==========================================================================
//==========================================================================
/** This uses the Fluidlite library to load soundfont files and play them.
    By default it uses MIDI channel 1, though you can optionally specify a channel.
    To inspect or create soundfont files, I recommend the "Polyphone" app.
 */
class SoundfontAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    SoundfontAudioProcessor(int numberOfVoices);
    ~SoundfontAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;


    //==============================================================================
    /** Load a .sf2 file. Will not reload a file if it is already loaded.
    If another file is loaded, it will unload that first. */
    bool loadSoundfont (const juce::File file);

    /** Load a soundfont from memory. */
    bool loadSoundfontFromMemory (const void* data, size_t size, const juce::String& fileName);
    
    /** Sends an incoming midi message to fluidsynth */
    void processMidi (const juce::MidiMessage& message);
    
    /** Send a noteon message. */
    void noteOn (int note, int velocity, int channel = 1);
    
    /** Send a noteoff message. */
    void noteOff (int note, int channel = 1);
    
    /** Send a continuous controller message. */
    void cc (int control, int value, int channel = 1);
    
    /** Get a continuous controller value. */
    int getCc (int control, int channel = 1);
    
    /** Send a pitch bend message. */
    void pitchBend (int value, int channel = 1);
    
    /** Get the current pitch bend value. */
    int getPitchBend (int channel = 1);
    
    /** Set the pitch wheel sensitivity. */
    void setPitchBendRange (int value, int channel = 1);
    
    /** Get the pitch wheel sensitivity. */
    int getPitchBendRange (int channel = 1);
    
    /** Send a channel pressure message. */
    void channelPressure(int value, int channel = 1);
    
    /** Set the fluidsynth gain */
    void setGain (float gain);
    
    /** Get the fluidsynth gain */
    float getGain();
    
    /** Send a reset. A reset turns all the notes off and resets the
        controller values. */
    void systemReset();
    
    /** Returns the raw fluid_synth_t object for direct use with the Fluidsynth API. */
    fluid_synth_t* getSynth()       { return synth; }
    
    /** Returns the raw settings for use with the Fluidsynth API. */
    fluid_settings_t* getSettings() { return settings; }
    
private:

    juce::CriticalSection lock;
    fluid_settings_t* settings;
    fluid_synth_t* synth;
    int sfontID = -1;
    juce::File loadedSoundfont;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundfontAudioProcessor)
};