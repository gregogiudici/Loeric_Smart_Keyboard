#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>

//==============================================================================
/**
 * Utility: list every audio device (inputs and outputs) across all driver types.
 * Does NOT start the audio engine.
 */
static void listAudioDevices()
{
    juce::AudioDeviceManager manager;
    // Initialise with 0 channels so no device is actually opened
    manager.initialise(0, 0, nullptr, false);

    juce::Logger::writeToLog("--- Available Audio Devices ---");

    for (auto *type : manager.getAvailableDeviceTypes())
    {
        type->scanForDevices();

        juce::Logger::writeToLog("Driver type: " + type->getTypeName());

        const auto inputNames = type->getDeviceNames(/*wantInputNames=*/true);
        const auto outputNames = type->getDeviceNames(/*wantInputNames=*/false);

        juce::Logger::writeToLog(" INPUT devices (" + juce::String(inputNames.size()) + "):");
        for (int i = 0; i < inputNames.size(); ++i)
            juce::Logger::writeToLog("\t[" + juce::String(i) + "] " + inputNames[i]);

        juce::Logger::writeToLog(" OUTPUT devices (" + juce::String(outputNames.size()) + "):");
        for (int i = 0; i < outputNames.size(); ++i)
            juce::Logger::writeToLog("\t[" + juce::String(i) + "] " + outputNames[i]);
    }

    juce::Logger::writeToLog("\n");
}

//==============================================================================
/**
 * Utility: list every MIDI input device.
 * Does NOT start the audio engine.
 */
static void listMidiDevices()
{
    const auto devices = juce::MidiInput::getAvailableDevices();

    juce::Logger::writeToLog("--- Available MIDI Input Devices ---");

    if (devices.isEmpty())
    {
        juce::Logger::writeToLog("  (none found)\n");
        return;
    }

    for (int i = 0; i < devices.size(); ++i)
        juce::Logger::writeToLog("  [" + juce::String(i) + "] " + devices[i].name);

    juce::Logger::writeToLog("\n");
}