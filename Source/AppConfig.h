#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
// Simple command-line argument parser
//==============================================================================
struct AppConfig
{
    bool listAudio = false;
    bool listMidi = false;
    bool dryRun = false;
    bool help = false;
    juce::String audioDevice; // substring match; empty = first available
    juce::String midiDevice;  // substring match; empty = first available
    double sampleRate = 44100.0;
    int bufferSize = 512;
    juce::String soundfont;

    static AppConfig parse(const juce::String &commandLine)
    {
        AppConfig cfg;
        const auto tokens = juce::StringArray::fromTokens(commandLine, " ", "\"");

        for (int i = 0; i < tokens.size(); ++i)
        {
            const auto &t = tokens[i];

            if (t == "--list-audio")
            {
                cfg.listAudio = true;
            }
            else if (t == "--list-midi")
            {
                cfg.listMidi = true;
            }
            else if (t == "--dry-run")
            {
                cfg.dryRun = true;
            }
            else if (t == "--audio-device" && i + 1 < tokens.size())
                cfg.audioDevice = tokens[++i];
            else if (t == "--midi-device" && i + 1 < tokens.size())
                cfg.midiDevice = tokens[++i];
            else if (t == "--sample-rate" && i + 1 < tokens.size())
                cfg.sampleRate = tokens[++i].getDoubleValue();
            else if (t == "--buffer-size" && i + 1 < tokens.size())
                cfg.bufferSize = tokens[++i].getIntValue();
            else if (t == "--soundfont" && i + 1 < tokens.size())
                cfg.soundfont = tokens[++i];
            else if (t == "--help")
            {
                juce::Logger::writeToLog("Usage:");
                juce::Logger::writeToLog("  LoericSmartKeyboard [options]");
                juce::Logger::writeToLog("Options:");
                juce::Logger::writeToLog("  --list-audio              List available audio input/output devices and exit");
                juce::Logger::writeToLog("  --list-midi               List available MIDI input devices and exit");
                juce::Logger::writeToLog("  --audio-device <name>     Use the audio device whose name contains <name>");
                juce::Logger::writeToLog("  --midi-device <name>      Use the MIDI device whose name contains <name>");
                juce::Logger::writeToLog("  --sample-rate <rate>      Sample rate in Hz (default: 44100)");
                juce::Logger::writeToLog("  --buffer-size <samples>   Buffer size in samples (default: 512)");
                juce::Logger::writeToLog("  --soundfont <file>        Load the specified soundfont file");
                juce::Logger::writeToLog("  --dry-run                 Validate device availability, print resolved config, then exit");
                juce::Logger::writeToLog("  --help                    Show this help message and exit");
                juce::Logger::writeToLog("\n");
                cfg.help = true;
            }
        }

        return cfg;
    }
};
