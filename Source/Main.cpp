#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <JuceHeader.h>
#include <fatar_sl_link/fatar_sl_link.h>
#include <BinaryData.h>

#include "AppConfig.h"
#include "Utils.h"
#include "Soundfont/SoundfontAudioProcessor.h"
#include "Loeric/LoericController.h"
#include "Loeric/LoericStore.h"
#include "SL/CustomPages.h"

//==============================================================================

class LoericSmartKeyboard : public juce::JUCEApplicationBase
{
public:
    //==============================================================================
    LoericSmartKeyboard()
        : processor(SF_NUM_VOICES), apvts(processor,
                                          nullptr,
                                          "Params",
                                          createParameterLayout()),
          uiManager(processor,
                    audioDeviceManager,
                    "LoericSmartKeyboard",
                    apvts,
                    &player.getMidiMessageCollector())
    {
        // Connect the sf2 processor to the audio player (used for the AudioIOCallback and MIDI input callbacks)
        player.setProcessor(&processor);
    }

    const juce::String getApplicationName() override { return ProjectInfo::projectName; }

    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }

    bool moreThanOneInstanceAllowed() override { return false; }

    //==============================================================================
    void initialise(const juce::String &commandLine) override
    {
        juce::Logger::writeToLog("===== LOERIC SMART KEYBOARD v0.0.1 =====");
        config = AppConfig::parse(commandLine);

        if (config.help)
        {
            quit();
            return;
        }

        // --list-audio and --list-midi: utility mode, no engine started
        if (config.listAudio)
        {
            listAudioDevices();
            quit();
            return;
        }

        if (config.listMidi)
        {
            listMidiDevices();
            quit();
            return;
        }

        if (!setupSoundfont()) { quit(); return; }
        if (!setupAudio()) { quit(); return; }
        if (!setupMidi()) { quit(); return; }
        if (!setupKeyboardGUI()) { quit(); return; }

        if (config.dryRun)
        {
            juce::Logger::writeToLog("Dry-run complete. Exiting.");
            quit();
            return;
        }
    }

    void shutdown() override
    {
        if (midiIdentifier.isNotEmpty())
        {
            audioDeviceManager.removeMidiInputDeviceCallback(midiIdentifier, &player);
            audioDeviceManager.setMidiInputDeviceEnabled(midiIdentifier, false);
        }

        audioDeviceManager.removeAudioCallback(&player);
        audioDeviceManager.closeAudioDevice();
        player.setProcessor(nullptr);

        juce::Logger::writeToLog("===== SHUTDOWN, BYE! =====");
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
        juce::Logger::writeToLog("Unhandled exception: " + juce::String(e->what()));
        juce::Logger::writeToLog("  In file: " + sourceFilename);
        juce::Logger::writeToLog("  Line: " + juce::String(lineNumber));
    }

private:
    // Create the parameter layout for the audio processor
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        // For now an empty parameter layout.
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("loeric_transpose", 0),
        "Transpose",
        juce::NormalisableRange<float>(),
        0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int)
                                                                    { return String(static_cast<int>(value * 16 - 8)); })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("loeric_responsiveness", 0),
        "Responsiveness",
        juce::NormalisableRange<float>(),
        0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction([](auto x, auto)
                                                                    { return String(x, 2); })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("loeric_third", 0),
        "?",
        juce::NormalisableRange<float>(),
        0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction([](auto x, auto)
                                                                    { return String(static_cast<int>(x * 127)); })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("loeric_fourth", 0),
        "?",
        juce::NormalisableRange<float>(),
        0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction([](auto x, auto)
                                                                    { return String(static_cast<int>(x * 127)); })));


        return layout;
    }

    bool setupLoeric()
    {
        // Check that the config.loericUrl is valid and if it's different from the base URL, we need to update it, by substituing the LoericController
        if (config.loericUrl.isNotEmpty() && baseLoericUrl != config.loericUrl)
        {
            loericController.setUrl(config.loericUrl);
        }

        return true;
    }

    bool setupSoundfont()
    {
        if (config.soundfont.isNotEmpty())
        {
            juce::File f(config.soundfont);
            if (!f.existsAsFile() || !processor.loadSoundfont(f))
            {
                juce::Logger::writeToLog("Could not load soundfont: " + config.soundfont);
                return false;
            }
            juce::Logger::writeToLog("Soundfont: " + f.getFileName());
            return true;
        }

        // Default: embedded K-Grand
        const bool ok = processor.loadSoundfontFromMemory(BinaryData::Full_Grand_Piano_sf2,
                                                          (size_t)BinaryData::Full_Grand_Piano_sf2Size,
                                                          "loeric_default.sf2");
        juce::Logger::writeToLog(ok ? "Soundfont: default (Piano)" : "Default soundfont failed to load");
        return ok;
    }

    bool setupAudio()
    {
        // 0 inputs, 2 outputs. No saved state. Fall back to the default device on failure.
        // Asking for 0 inputs also avoids the macOS microphone permission prompt.
        auto err = audioDeviceManager.initialise(0, 2, nullptr, true);
        if (err.isNotEmpty())
        {
            juce::Logger::writeToLog("Audio init error: " + err);
            return false;
        }

        auto setup = audioDeviceManager.getAudioDeviceSetup();

        // Find an output device whose name contains the requested substring
        if (config.audioDevice.isNotEmpty())
        {
            bool found = false;

            for (auto *type : audioDeviceManager.getAvailableDeviceTypes())
            {
                type->scanForDevices();

                for (auto &name : type->getDeviceNames(/*wantInputNames=*/false))
                {
                    if (name.containsIgnoreCase(config.audioDevice))
                    {
                        audioDeviceManager.setCurrentAudioDeviceType(type->getTypeName(), true);
                        setup = audioDeviceManager.getAudioDeviceSetup();
                        setup.outputDeviceName = name;
                        found = true;
                        break;
                    }
                }
                if (found)
                    break;
            }

            if (!found)
            {
                juce::Logger::writeToLog("Audio device not found: " + config.audioDevice);
                listAudioDevices();
                return false;
            }
        }

        setup.inputDeviceName = {};
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        setup.useDefaultOutputChannels = true;
        setup.sampleRate = config.sampleRate;
        setup.bufferSize = config.bufferSize;

        err = audioDeviceManager.setAudioDeviceSetup(setup, /*treatAsChosenDevice=*/true);
        if (err.isNotEmpty())
        {
            juce::Logger::writeToLog("Audio setup error: " + err);
            return false;
        }

        // The processor was already attached to the player in the constructor
        audioDeviceManager.addAudioCallback(&player);

        if (auto *dev = audioDeviceManager.getCurrentAudioDevice())
            juce::Logger::writeToLog("Audio: " + dev->getName() + " @ " + juce::String(dev->getCurrentSampleRate()) + " Hz, " + juce::String(dev->getCurrentBufferSizeSamples()) + " samples");
        return true;
    }

    bool setupMidi()
    {
        for (const auto &d : juce::MidiInput::getAvailableDevices())
        {
            if (config.midiDevice.isEmpty() || d.name.containsIgnoreCase(config.midiDevice))
            {
                audioDeviceManager.setMidiInputDeviceEnabled(d.identifier, true);
                audioDeviceManager.addMidiInputDeviceCallback(d.identifier, &player); // Attach MIDI input callback (player)
                midiIdentifier = d.identifier;
                juce::Logger::writeToLog("MIDI: " + d.name);
                return true;
            }
        }

        juce::Logger::writeToLog("MIDI device not found: " + config.midiDevice);
        listMidiDevices();
        return false;
    }

    bool setupKeyboardGUI()
    {
        std::vector<juce::String> loericParamIDs = {
            "loeric_transpose",
            "loeric_responsiveness",
            "loeric_fourth",
            "loeric_third"
        };
        std::vector<std::unique_ptr<UIPage>> newPages;
        newPages.push_back(std::make_unique<LoericPage>(
            "loeric", "LOERIC", "", "LOERIC", juce::Colours::lightcoral,
            loericParamIDs, uiManager.slLinkManager, apvts));

        newPages.back()->buttonToPageIndex = {
            {slmk2::SLButton::HOME, 0},
            {slmk2::SLButton::DAW, 1},
            {slmk2::SLButton::GLOBAL, 2}};

        uiManager.setPages(std::move(newPages));
        uiManager.setCurrentPage(0);
        return true;
    }

    //==========================================================================
    AppConfig config;
    juce::String midiIdentifier;
    juce::AudioDeviceManager audioDeviceManager;
    SoundfontAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorPlayer player;
    LoericStore loericStore;
    juce::String baseLoericUrl = "http://nexus-keyboard.local:8080";
    LoericController loericController { baseLoericUrl, loericStore };
    UIManager uiManager;
};

//==============================================================================
START_JUCE_APPLICATION(LoericSmartKeyboard)