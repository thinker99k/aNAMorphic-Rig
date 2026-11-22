#include "TunerSettings.h"

#include "AppPaths.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace rig
{
    namespace
    {
        constexpr const char* pitchKey = "tunerPitch";
    }

    int TunerSettings::clampTenths (int tenths) noexcept
    {
        return juce::jlimit (minTenths, maxTenths, tenths);
    }

    int TunerSettings::load()
    {
        const auto json = juce::JSON::parse (paths::settingsFile());

        if (json.getDynamicObject() == nullptr)
            return defaultTenths;

        const auto value = json.getProperty (pitchKey, {});

        if (! (value.isDouble() || value.isInt()))
            return defaultTenths;

        return clampTenths ((int) std::lround ((double) value * 10.0));
    }

    void TunerSettings::save (int tenths)
    {
        auto json = juce::JSON::parse (paths::settingsFile());

        // 파일이 없거나 깨졌으면 쓰지 않는다 (복구는 Defaults 의 몫)
        if (auto* root = json.getDynamicObject())
        {
            root->setProperty (pitchKey, juce::var (clampTenths (tenths) / 10.0));
            paths::settingsFile().replaceWithText (juce::JSON::toString (json));
        }
    }
}
