#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include "LoericState.h"

//==============================================================================
/** The single place where the server state lives on the C++ side.

    * Everything here runs on the MESSAGE THREAD: apply...() is called by LoericController through
      MessageManager::callAsync, listeners are called synchronously from apply...().
    * Listeners are only notified when something actually changed (the first value is always notified).
    * Never touch this from the audio thread. */
class LoericStore
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /** The server became reachable / unreachable. `error` is empty when connected. */
        virtual void loericConnectionChanged (bool /*connected*/, const juce::String& /*error*/) {}

        /** /api/status changed. `change` says what differs from the previous status
            (including the list of parameter keys that were added, removed or modified). */
        virtual void loericStatusChanged (const LoericState::Status& /*status*/,
                                          const LoericState::Status::Change& /*change*/) {}

        virtual void loericTunesChanged (const std::vector<LoericState::Tune>& /*tunes*/) {}
        virtual void loericInstrumentsChanged (const std::vector<LoericState::Instrument>& /*instruments*/) {}
        virtual void loericConfigsChanged (const juce::String& /*tuneId*/, const LoericState::Configs& /*configs*/) {}
    };

    void addListener (Listener* l)       { listeners.add (l); }
    void removeListener (Listener* l)    { listeners.remove (l); }

    /** Read-only view of everything. Message thread only. */
    const LoericState& getState() const noexcept    { return state; }

    //==========================================================================
    // Called by LoericController (message thread).
    void applyConnection (bool connected, const juce::String& error)
    {
        JUCE_ASSERT_MESSAGE_THREAD

        state.lastError = connected ? juce::String() : error;

        if (state.connectionKnown && state.connected == connected)
            return;

        state.connected = connected;
        state.connectionKnown = true;
        listeners.call ([&] (Listener& l) { l.loericConnectionChanged (connected, state.lastError); });
    }

    void applyStatus (LoericState::Status status)
    {
        JUCE_ASSERT_MESSAGE_THREAD

        const auto change = status.diff (state.status);

        if (state.statusReceived && ! change.any())
            return;

        state.status = std::move (status);
        state.statusReceived = true;
        listeners.call ([&] (Listener& l) { l.loericStatusChanged (state.status, change); });
    }

    void applyTunes (std::vector<LoericState::Tune> tunes)
    {
        JUCE_ASSERT_MESSAGE_THREAD

        if (state.tunesLoaded && state.tunes == tunes)
            return;

        state.tunes = std::move (tunes);
        state.tunesLoaded = true;
        listeners.call ([&] (Listener& l) { l.loericTunesChanged (state.tunes); });
    }

    void applyInstruments (std::vector<LoericState::Instrument> instruments)
    {
        JUCE_ASSERT_MESSAGE_THREAD

        if (state.instrumentsLoaded && state.instruments == instruments)
            return;

        state.instruments = std::move (instruments);
        state.instrumentsLoaded = true;
        listeners.call ([&] (Listener& l) { l.loericInstrumentsChanged (state.instruments); });
    }

    void applyConfigs (const juce::String& tuneId, LoericState::Configs configs)
    {
        JUCE_ASSERT_MESSAGE_THREAD

        const auto it = state.configsByTune.find (tuneId);

        if (it != state.configsByTune.end() && it->second == configs)
            return;

        state.configsByTune[tuneId] = std::move (configs);
        listeners.call ([&] (Listener& l) { l.loericConfigsChanged (tuneId, state.configsByTune[tuneId]); });
    }

private:
    LoericState state;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_WEAK_REFERENCEABLE (LoericStore)
};

// Helper class to print state changes
struct PrintingListener : public LoericStore::Listener
{
    std::function<void (const std::vector<LoericState::Tune>&)> onTunes;

    void loericConnectionChanged (bool connected, const juce::String& error) override
    {
        juce::Logger::writeToLog(connected ? "[connection] CONNECTED" : "[connection] DISCONNECTED (" + error + ")");
    }

    void loericStatusChanged (const LoericState::Status& s, const LoericState::Status::Change& c) override
    {
        juce::Logger::writeToLog("[status] running=" + juce::String (s.running ? "yes" : "no")
             + " tune='" + s.currentTune + "' config='" + s.currentConfig + "' instrument='" + s.currentInstrument + "'"
             + "  changed: run=" + juce::String ((int) c.running) + " tune=" + juce::String ((int) c.tune)
             + " params=[" + c.parameters.joinIntoString (", ") + "]");

        for (const auto& [key, value] : s.parameters)
            if (c.parameters.contains (key))
                juce::Logger::writeToLog("            " + key + " = " + juce::JSON::toString (value, true));
    }

    void loericTunesChanged (const std::vector<LoericState::Tune>& t) override
    {
        for (const auto& x : t) juce::Logger::writeToLog("[tune] " + x.id + " | " + x.name + " | " + x.path);

        if (onTunes)
            onTunes (t);
    }

    void loericInstrumentsChanged (const std::vector<LoericState::Instrument>& i) override
    {
        for (const auto& x : i) juce::Logger::writeToLog("[instrument] " + x.id + " | " + x.name);
    }

    void loericConfigsChanged (const juce::String& tuneId, const LoericState::Configs& c) override
    {
        juce::StringArray ids;
        for (const auto& x : c.list) ids.add (x.id);

        juce::Logger::writeToLog("[configs '" + tuneId + "'] default=" + (c.defaultConfig ? c.defaultConfig->id : juce::String ("(none)"))
             + " list=[" + ids.joinIntoString (", ") + "]");
    }
};