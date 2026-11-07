#include "AppPaths.h"

namespace rig::paths
{
    juce::File root()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                       .getChildFile ("aNAMorphic-Rig");
        if (! dir.isDirectory())
            dir.createDirectory();
        return dir;
    }

    juce::File patchFile (int number)
    {
        return root().getChildFile ("config" + juce::String (number));
    }

    juce::File deviceSettingsFile()
    {
        return root().getChildFile ("audio-device.xml");
    }

    juce::File capturesDir()
    {
        return root().getChildFile ("captures");
    }

    juce::File irsDir()
    {
        return root().getChildFile ("irs");
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
