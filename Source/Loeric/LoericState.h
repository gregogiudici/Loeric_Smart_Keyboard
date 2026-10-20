#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <optional>
#include <vector>

namespace loeric
{
    // Compare two JSON values. Containers are compared through their JSON text, because
    // juce::var compares arrays/objects by identity, which would always report "changed".
    inline bool sameValue (const juce::var& a, const juce::var& b)
    {
        if (a.isVoid() != b.isVoid() || a.isBool() != b.isBool())
            return false;

        if (a.isArray() || b.isArray() || a.isObject() || b.isObject())
            return juce::JSON::toString (a, true) == juce::JSON::toString (b, true);

        return a == b;
    }
}

//==============================================================================
/** Everything the C++ side knows about the LOERIC web server, as plain values.

    Mirrors models.py:
      Status       <- StatusResponse        GET /api/status
      Tune         <- TuneInfo              GET /api/tunes            ("tunes")
      Configs      <- ConfigsResponse       GET /api/configs/{tune}
      Instrument   <- InstrumentModel       GET /api/instruments      ("models")

    Empty strings mean "null" in the JSON (no tune selected, no default config, ...). */
struct LoericState
{
    struct Tune
    {
        juce::String id, name, path;

        bool operator== (const Tune& o) const noexcept { return id == o.id && name == o.name && path == o.path; }
        bool operator!= (const Tune& o) const noexcept { return ! (*this == o); }
    };

    struct Config
    {
        juce::String id, name;

        bool operator== (const Config& o) const noexcept { return id == o.id && name == o.name; }
        bool operator!= (const Config& o) const noexcept { return ! (*this == o); }
    };

    struct Configs
    {
        std::optional<Config> defaultConfig;
        std::vector<Config> list;

        bool operator== (const Configs& o) const { return defaultConfig == o.defaultConfig && list == o.list; }
        bool operator!= (const Configs& o) const { return ! (*this == o); }
    };

    struct Instrument
    {
        juce::String id, name;

        bool operator== (const Instrument& o) const noexcept { return id == o.id && name == o.name; }
        bool operator!= (const Instrument& o) const noexcept { return ! (*this == o); }
    };

    /** StatusResponse.parameters, flattened: nested objects become "a/b/c" keys (the same
        syntax /api/change/{path} uses). Arrays and scalars are kept as juce::var. */
    using Parameters = std::map<juce::String, juce::var>;

    struct Status
    {
        /** What differs from a previous status (see Status::diff). */
        struct Change
        {
            bool running = false, tune = false, config = false, instrument = false;
            juce::StringArray parameters;       // keys added, removed or modified

            bool any() const noexcept { return running || tune || config || instrument || ! parameters.isEmpty(); }
        };

        bool running = false;
        juce::String currentTune, currentConfig, currentInstrument;
        Parameters parameters;

        /** Returns the parameter's value, or `fallback` if the server doesn't report it. */
        juce::var getParameter (const juce::String& key, const juce::var& fallback = {}) const
        {
            const auto it = parameters.find (key);
            return it != parameters.end() ? it->second : fallback;
        }

        Change diff (const Status& previous) const
        {
            Change c;
            c.running    = running != previous.running;
            c.tune       = currentTune != previous.currentTune;
            c.config     = currentConfig != previous.currentConfig;
            c.instrument = currentInstrument != previous.currentInstrument;

            for (const auto& entry : parameters)
            {
                const auto it = previous.parameters.find (entry.first);

                if (it == previous.parameters.end() || ! loeric::sameValue (entry.second, it->second))
                    c.parameters.add (entry.first);
            }

            for (const auto& entry : previous.parameters)
                if (parameters.find (entry.first) == parameters.end())
                    c.parameters.add (entry.first);

            return c;
        }

        bool operator== (const Status& o) const { return ! diff (o).any(); }
        bool operator!= (const Status& o) const { return ! (*this == o); }
    };

    //==========================================================================
    bool connected = false;             // last /api/status poll succeeded
    bool connectionKnown = false;       // at least one poll has completed
    juce::String lastError;             // why the last poll failed (empty when connected)

    bool statusReceived = false;
    Status status;

    bool tunesLoaded = false;
    std::vector<Tune> tunes;

    bool instrumentsLoaded = false;
    std::vector<Instrument> instruments;

    std::map<juce::String, Configs> configsByTune;      // filled on demand, see LoericController::requestConfigs

    const Tune* findTune (const juce::String& id) const
    {
        for (const auto& t : tunes)
            if (t.id == id)
                return &t;

        return nullptr;
    }

    const Instrument* findInstrument (const juce::String& id) const
    {
        for (const auto& i : instruments)
            if (i.id == id)
                return &i;

        return nullptr;
    }

    const Configs* findConfigs (const juce::String& tuneId) const
    {
        const auto it = configsByTune.find (tuneId);
        return it != configsByTune.end() ? &it->second : nullptr;
    }
};

//==============================================================================
/** JSON (juce::var) -> LoericState. Pure functions, safe to call from any thread.
    Every parser returns false (and leaves `out` untouched) if the payload doesn't have the expected shape. */
struct LoericParser
{
    static bool parseStatus (const juce::var& json, LoericState::Status& out)
    {
        if (! json.isObject() || ! json.hasProperty ("running"))
            return false;

        LoericState::Status s;
        s.running           = (bool) json.getProperty ("running", false);
        s.currentTune       = text (json, "current_tune");
        s.currentConfig     = text (json, "current_config");
        s.currentInstrument = text (json, "current_instrument");
        flatten ({}, json.getProperty ("parameters", juce::var()), s.parameters);

        out = std::move (s);
        return true;
    }

    static bool parseTunes (const juce::var& json, std::vector<LoericState::Tune>& out)
    {
        const auto* array = json.getProperty ("tunes", juce::var()).getArray();

        if (array == nullptr)
            return false;

        std::vector<LoericState::Tune> result;

        for (const auto& item : *array)
            result.push_back ({ text (item, "id"), text (item, "name"), text (item, "path") });

        out = std::move (result);
        return true;
    }

    static bool parseInstruments (const juce::var& json, std::vector<LoericState::Instrument>& out)
    {
        const auto* array = json.getProperty ("models", juce::var()).getArray();

        if (array == nullptr)
            return false;

        std::vector<LoericState::Instrument> result;

        for (const auto& item : *array)
            result.push_back ({ text (item, "id"), text (item, "name") });

        out = std::move (result);
        return true;
    }

    static bool parseConfigs (const juce::var& json, LoericState::Configs& out)
    {
        if (! json.isObject())
            return false;

        LoericState::Configs result;

        const auto def = json.getProperty ("default_config", juce::var());

        if (def.isObject())
            result.defaultConfig = LoericState::Config { text (def, "id"), text (def, "name") };

        if (const auto* array = json.getProperty ("configs", juce::var()).getArray())
            for (const auto& item : *array)
                result.list.push_back ({ text (item, "id"), text (item, "name") });

        out = std::move (result);
        return true;
    }

    /** Nested objects -> "a/b/c" keys. Anything else is stored as it is. */
    static void flatten (const juce::String& prefix, const juce::var& value, LoericState::Parameters& out)
    {
        if (auto* object = value.getDynamicObject())
        {
            for (const auto& property : object->getProperties())
            {
                const auto key = prefix.isEmpty() ? property.name.toString()
                                                  : prefix + "/" + property.name.toString();

                if (property.value.getDynamicObject() != nullptr)
                    flatten (key, property.value, out);
                else
                    out[key] = property.value;
            }
        }
    }

private:
    static juce::String text (const juce::var& object, const char* key)
    {
        const auto v = object.getProperty (juce::Identifier (key), juce::var());
        return v.isVoid() ? juce::String() : v.toString();
    }
};