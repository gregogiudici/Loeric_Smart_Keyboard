#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <JuceHeader.h>
#include <fatar_sl_link/fatar_sl_link.h>

#include "Soundfont/SoundfontAudioProcessor.h"

//==============================================================================

class LoericSmartKeyboard : public juce::JUCEApplicationBase
{
public:
    //==============================================================================
    LoericSmartKeyboard()
        : processor(SF_NUM_VOICES)
        , apvts (processor,
                 nullptr,
                 "Params",
                 createParameterLayout())
        , uiManager (processor,
                     audioDeviceManager,
                     "Loeric Smart Keyboard",
                     apvts,
                     &player.getMidiMessageCollector())
    {
        // Connect the sf2 processor to the audio player (used for the AudioIOCallback)
        player.setProcessor(&processor);
    }

    const juce::String getApplicationName() override { return ProjectInfo::projectName; }

    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }

    bool moreThanOneInstanceAllowed() override { return false; }

    //==============================================================================
    void initialise(const juce::String &commandLine) override
    {
        // Handle application initialization
        DBG("Application Initialised");
    }

    void shutdown() override
    {
        // Handle application shutdown
        DBG("Application Shutdown");
    }

    void anotherInstanceStarted(const juce::String &commandLine) override
    {
        // Handle another instance starting
    }

    void systemRequestedQuit() override
    {
        // Handle system requested quit
    }

    void suspended() override
    {
        // Handle application suspended
    }

    void resumed() override
    {
        // Handle application resumed
    }

    void unhandledException(const std::exception *e, const juce::String &sourceFilename, int lineNumber) override
    {
        // Log the exception details
    }

private:
    // Create the parameter layout for the audio processor
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        return layout;
    }

    // Internal member variables
    juce::AudioDeviceManager audioDeviceManager;
    juce::AudioProcessorPlayer player;
    SoundfontAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts;
    UIManager uiManager;
};


//==============================================================================
START_JUCE_APPLICATION(LoericSmartKeyboard)