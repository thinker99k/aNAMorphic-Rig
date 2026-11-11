#include "NamMetadata.h"

namespace rig
{
    std::optional<NamMetadata> readNamMetadata (const juce::File& file, juce::String& error)
    {
        error.clear();

        juce::FileInputStream stream (file);
        if (stream.failedToOpen())
        {
            error = stream.getStatus().getErrorMessage();
            return std::nullopt;
        }

        const auto contents = stream.readEntireStreamAsString();
        if (stream.getStatus().failed())
        {
            error = stream.getStatus().getErrorMessage();
            return std::nullopt;
        }

        juce::var json;
        const auto result = juce::JSON::parse (contents, json);
        if (result.failed())
        {
            error = result.getErrorMessage();
            return std::nullopt;
        }

        if (! json.isObject())
        {
            error = "NAM JSON must be an object";
            return std::nullopt;
        }

        const auto metadata = json.getProperty ("metadata", {});
        if (metadata.isVoid())
            return NamMetadata {};

        if (! metadata.isObject())
        {
            error = "NAM metadata must be an object";
            return std::nullopt;
        }

        const auto text = [&metadata] (const char* key)
        {
            const auto value = metadata.getProperty (key, {});
            return value.isString() ? value.toString().trim() : juce::String();
        };

        return NamMetadata { text ("gear_type"), text ("name"),
                             text ("gear_model"), text ("tone_type"), text ("modeled_by") };
    }
}
