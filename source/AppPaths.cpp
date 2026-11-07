#include "AppPaths.h"

namespace rig::paths
{
    juce::File root()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("aNAMorphic-Rig");
        if (! dir.isDirectory())
            dir.createDirectory();
        return dir;
    }

    juce::File namDir()
    {
        return root().getChildFile ("nam");
    }

    juce::File irDir()
    {
        return root().getChildFile ("ir");
    }

    juce::File patchDir()
    {
        return root().getChildFile ("patch");
    }

    juce::File patchFile (int number)
    {
        return patchDir().getChildFile ("config" + juce::String (number).paddedLeft ('0', 2));
    }

    juce::File deviceSettingsFile()
    {
        return root().getChildFile ("audio-device.xml");
    }

    std::filesystem::path toStdPath (const juce::File& file)
    {
       #if JUCE_WINDOWS
        return std::filesystem::path (file.getFullPathName().toWideCharPointer());
       #else
        return std::filesystem::path (file.getFullPathName().toStdString());
       #endif
    }
}
