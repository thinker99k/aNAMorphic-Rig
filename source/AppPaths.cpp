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

    juce::File settingsFile()
    {
        return root().getChildFile ("settings.json");
    }

    bool writeText (const juce::File& file, const juce::String& text)
    {
        juce::TemporaryFile temp (file, juce::TemporaryFile::useHiddenFile);

        {
            juce::FileOutputStream out (temp.getFile());

            if (! out.openedOk())
                return false;

            // 줄바꿈은 replaceWithText 기본값과 같은 "\r\n"
            const bool written = out.writeText (text, false, false, "\r\n");
            out.flush();

            if (! written || out.getStatus().failed())
                return false;
        }

        return temp.overwriteTargetFileWithTemporary();
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
