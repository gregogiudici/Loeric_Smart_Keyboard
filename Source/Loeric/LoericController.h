#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <deque>
#include <functional>
#include <set>
#include <utility>

#include "LoericClient.h"
#include "LoericState.h"
#include "LoericStore.h"

/** HTTP worker thread. Sends commands, polls /api/status every second, loads the catalogue
    (tunes + instruments) and publishes everything to a LoericStore on the message thread.

    Declare the store before the controller so the controller is destroyed first. (Updates that are
    still queued hold a WeakReference, so the opposite order does not crash either.) */
class LoericController final : private juce::Thread
{
public:
    LoericController(juce::String serverAddress, LoericStore &storeToFill)
        : juce::Thread("LOERIC HTTP controller"),
          store(storeToFill),
          client(std::move(serverAddress))
    {
    }

    ~LoericController() override
    {
        stop();
    }

    bool start()
    {
        return startThread();
    }

    void stop()
    {
        signalThreadShouldExit();
        notify();

        // Finish the current HTTP request before destroying client.
        // See the shutdown caveat below.
        waitForThreadToExit(-1);

        const juce::ScopedLock lock(queueLock);
        commands.clear();
    }

    //==========================================================================
    // Commands (any thread). Each one triggers an immediate /api/status refresh.

    void startPerformance(juce::var settings)
    {
        // Transfer ownership of this settings object to the queue:
        // do not modify its DynamicObject after calling this.
        enqueue(
            [settings = std::move(settings)](LoericClient &http)
            {
                return http.startPerformance(settings);
            });
    }

    void stopPerformance()
    {
        enqueue([](LoericClient &http)
                { return http.stopPerformance(); });
    }

    void setTempo(double qpm)
    {
        enqueue([qpm](LoericClient &http)
                { return http.setTempo(qpm); });
    }

    void setVolume(double value)
    {
        enqueue([value](LoericClient &http)
                { return http.setVolume(value); });
    }

    void setResponsiveness(double value)
    {
        enqueue([value](LoericClient &http)
                { return http.setResponsiveness(value); });
    }

    /** POST /api/change/{path}?value=... */
    void change(const juce::String &path, const juce::var &value)
    {
        enqueue([path, value](LoericClient &http)
                { return http.change(path, value); });
    }

    void setUrl(const juce::String &newUrl)
    {
        juce::ScopedLock lock(queueLock);
        client.setBaseUrl(newUrl);
    }

    //==========================================================================
    // Data requests (any thread). Results arrive through the LoericStore listeners.

    /** Loads GET /api/configs/{tuneId}. Remembered, so it is reloaded automatically
        after the server comes back from a restart. */
    void requestConfigs(const juce::String &tuneId)
    {
        enqueue([this, tuneId](LoericClient &http)
                {
            configTunes.insert (tuneId);
            return fetchConfigs (http, tuneId); });
    }

    /** Forces a reload of tunes + instruments (e.g. after adding files on the server). */
    void refreshCatalog()
    {
        catalogRequested = true;
        notify();
    }

    // Optional raw hook, kept from the previous version.
    // Called on the controller thread, NOT the message/audio thread.
    std::function<void(const LoericClient::Response &)> onStatus;

private:
    using Command =
        std::function<LoericClient::Response(LoericClient &)>;

    void enqueue(Command command)
    {
        {
            const juce::ScopedLock lock(queueLock);
            commands.push_back(std::move(command));
        }

        notify();
    }

    void run() override
    {
        double nextPoll = 0.0;

        while (!threadShouldExit())
        {
            Command command;

            {
                const juce::ScopedLock lock(queueLock);

                if (!commands.empty())
                {
                    command = std::move(commands.front());
                    commands.pop_front();
                }
            }

            if (threadShouldExit())
                break;

            if (command)
            {
                logFailure(command(client));
                nextPoll = 0.0; // show the effect of the command right away
            }

            if (threadShouldExit())
                break;

            const auto now = juce::Time::getMillisecondCounterHiRes();

            if (now >= nextPoll)
            {
                pollStatus();

                nextPoll =
                    juce::Time::getMillisecondCounterHiRes() + 1000.0;
            }

            const bool forced = catalogRequested.exchange(false);

            if (connected && (!catalogLoaded || forced))
                loadCatalog(forced);

            // Commands wake the thread immediately.
            // A short wait also keeps shutdown responsive between requests.
            if (!hasQueuedCommands())
                wait(100);
        }
    }

    bool hasQueuedCommands()
    {
        const juce::ScopedLock lock(queueLock);
        return !commands.empty();
    }

    //==========================================================================
    void pollStatus()
    {
        const auto response = client.status();

        if (onStatus)
            onStatus(response);

        LoericState::Status status;
        const bool ok = response.succeeded() && LoericParser::parseStatus(response.json, status);

        if (ok)
        {
            if (!connected)
                juce::Logger::writeToLog("LOERIC server connected");

            connected = true;
            postToStore([](LoericStore &s)
                        { s.applyConnection(true, {}); });
            postToStore([status](LoericStore &s)
                        { s.applyStatus(status); });
        }
        else
        {
            const auto error = describeFailure(response, "/api/status");

            if (connected || !reportedDisconnect)
                juce::Logger::writeToLog("LOERIC server unreachable: " + error);

            connected = false;
            catalogLoaded = false; // reload everything after a reconnect: the server may have restarted
            reportedDisconnect = true;
            postToStore([error](LoericStore &s)
                        { s.applyConnection(false, error); });
        }

        if (ok)
            reportedDisconnect = false;
    }

    void loadCatalog(bool force)
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();

        if (!force && now < nextCatalogAttempt)
            return;

        nextCatalogAttempt = now + 5000.0; // retry slowly if a request fails

        bool ok = true;

        {
            const auto r = client.tunes();
            std::vector<LoericState::Tune> tunes;

            if (r.succeeded() && LoericParser::parseTunes(r.json, tunes))
                postToStore([tunes](LoericStore &s)
                            { s.applyTunes(tunes); });
            else
            {
                ok = false;
                logFailure(withError(r, "/api/tunes"));
            }
        }

        {
            const auto r = client.instruments();
            std::vector<LoericState::Instrument> instruments;

            if (r.succeeded() && LoericParser::parseInstruments(r.json, instruments))
                postToStore([instruments](LoericStore &s)
                            { s.applyInstruments(instruments); });
            else
            {
                ok = false;
                logFailure(withError(r, "/api/instruments"));
            }
        }

        for (const auto &tuneId : configTunes)
            logFailure(fetchConfigs(client, tuneId));

        catalogLoaded = ok;
    }

    LoericClient::Response fetchConfigs(LoericClient &http, const juce::String &tuneId)
    {
        auto r = http.configs(tuneId);
        LoericState::Configs configs;

        if (r.succeeded() && LoericParser::parseConfigs(r.json, configs))
        {
            postToStore([tuneId, configs](LoericStore &s)
                        { s.applyConfigs(tuneId, configs); });
            return r;
        }

        return withError(r, "/api/configs/" + tuneId);
    }

    template <typename Fn>
    void postToStore(Fn fn)
    {
        juce::MessageManager::callAsync([weak = juce::WeakReference<LoericStore>(&store), fn = std::move(fn)]
                                        {
            if (auto* s = weak.get())
                fn (*s); });
    }

    // Makes sure a response that was HTTP-OK but had an unexpected payload is reported as a failure.
    static LoericClient::Response withError(LoericClient::Response r, const juce::String &what)
    {
        if (r.error.isEmpty())
            r.error = (r.httpStatus >= 200 && r.httpStatus < 300) ? "unexpected payload from " + what
                                                                  : "HTTP " + juce::String(r.httpStatus) + " from " + what;
        return r;
    }

    static juce::String describeFailure(const LoericClient::Response &r, const juce::String &what)
    {
        if (r.httpStatus == 0)
            return "cannot reach server";

        return withError(r, what).error;
    }

    static void logFailure(const LoericClient::Response &response)
    {
        if (response.succeeded())
            return;

        juce::Logger::writeToLog(
            "LOERIC command failed: HTTP " + juce::String(response.httpStatus) + " " + response.error + " " + response.body);
    }

    LoericStore &store;
    LoericClient client;

    juce::CriticalSection queueLock;
    std::deque<Command> commands;

    // Controller-thread only:
    bool connected = false, catalogLoaded = false, reportedDisconnect = false;
    double nextCatalogAttempt = 0.0;
    std::set<juce::String> configTunes;
    std::atomic<bool> catalogRequested{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoericController)
};
