#pragma once

#include <juce_core/juce_core.h>

class LoericClient
{
public:
    struct Response
    {
        int httpStatus = 0;
        juce::String body;
        juce::String error;
        juce::var json;

        bool succeeded() const noexcept
        {
            return error.isEmpty()
                && httpStatus >= 200
                && httpStatus < 300;
        }
    };

    explicit LoericClient (juce::String serverAddress)
        : baseUrl (serverAddress.trimCharactersAtEnd ("/"))
    {
    }

    void setBaseUrl (const juce::String& serverAddress)
    {
        const auto normalised =
            serverAddress.trim().trimCharactersAtEnd ("/");

        const juce::ScopedLock lock (baseUrlLock);
        baseUrl = normalised;
    }

    juce::String getBaseUrl() const
    {
        const juce::ScopedLock lock (baseUrlLock);
        return baseUrl;
    }

    Response status() const
    {
        return request ("GET", "/api/status");
    }

    Response tunes() const
    {
        return request ("GET", "/api/tunes");
    }

    Response instruments() const
    {
        return request ("GET", "/api/instruments");
    }

    // NEW: GET /api/configs/{tune_id}
    Response configs (const juce::String& tuneId) const
    {
        return request ("GET", "/api/configs/" + escapePath (tuneId));
    }

    Response startPerformance (const juce::var& settings) const
    {
        return request (
            "POST",
            "/api/start",
            juce::JSON::toString (settings));
    }

    Response stopPerformance() const
    {
        return request ("POST", "/api/stop");
    }

    // CHANGED: sends an integer, like the web UI does (/api/tempo/120).
    // StartRequest.tempo is an int in models.py; a "120.000000" path would be
    // rejected with a 422 if the route is declared as int.
    Response setTempo (double qpm) const
    {
        return request (
            "POST", "/api/tempo/" + juce::String (juce::roundToInt (qpm)));
    }

    Response setVolume (double value) const
    {
        return request (
            "POST", "/api/volume/" + juce::String (value, 6));
    }

    Response setResponsiveness (double value) const
    {
        return request (
            "POST",
            "/api/responsiveness/" + juce::String (value, 6));
    }

    // NEW: POST /api/change/{path}?value={value}, e.g. change ("groover/modules/transpose/steps", 2)
    // bool -> "true"/"false", int -> "2", double -> "0.500000", string as it is.
    Response change (const juce::String& path, const juce::var& value) const
    {
        return request ("POST",
                        "/api/change/" + escapePath (path)
                          + "?value=" + juce::URL::addEscapeChars (valueToString (value), true));
    }

private:
    // Escapes every path segment but keeps the "/" separators.
    static juce::String escapePath (const juce::String& path)
    {
        juce::StringArray parts;
        parts.addTokens (path, "/", {});

        for (auto& part : parts)
            part = juce::URL::addEscapeChars (part, false);

        return parts.joinIntoString ("/");
    }

    static juce::String valueToString (const juce::var& v)
    {
        if (v.isBool())
            return (bool) v ? "true" : "false";

        if (v.isDouble())
            return juce::String ((double) v, 6);

        return v.toString();
    }

    Response request (const juce::String& method,
                      const juce::String& path,
                      const juce::String& jsonBody = {}) const
    {
        Response result;

        juce::URL url (getBaseUrl() + path);

        const bool hasBody = jsonBody.isNotEmpty();

        if (hasBody)
            url = url.withPOSTData (jsonBody);

        // FIXED: InputStreamOptions is not assignable in JUCE 9 (const member), so the
        // "options = options.withExtraHeaders (...)" form does not compile. Build it in one expression.
        const auto options =
            juce::URL::InputStreamOptions (
                juce::URL::ParameterHandling::inAddress)
                .withHttpRequestCmd (method)
                .withConnectionTimeoutMs (1500)
                .withNumRedirectsToFollow (0)
                .withExtraHeaders (hasBody ? juce::String ("Content-Type: application/json\r\n")
                                           : juce::String())
                .withStatusCode (&result.httpStatus);

        auto stream = url.createInputStream (options);

        if (stream == nullptr)
        {
            result.error = "Cannot open HTTP response";
            return result;
        }

        result.body = stream->readEntireStreamAsString();

        if (result.body.isNotEmpty())
        {
            const auto parsed =
                juce::JSON::parse (result.body, result.json);

            if (parsed.failed())
                result.error = parsed.getErrorMessage();
        }

        return result;
    }

    mutable juce::CriticalSection baseUrlLock;
    juce::String baseUrl;
};
