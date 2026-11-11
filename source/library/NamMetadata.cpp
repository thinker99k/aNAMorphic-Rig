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

        const auto gearType = metadata.getProperty ("gear_type", {});
        const auto name = metadata.getProperty ("name", {});
        if ((! gearType.isVoid() && ! gearType.isString()) || (! name.isVoid() && ! name.isString()))
        {
            error = "NAM metadata gear_type and name must be strings";
            return std::nullopt;
        }

        return NamMetadata { gearType.toString(), name.toString() };
    }
}
