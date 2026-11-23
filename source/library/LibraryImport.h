#pragma once

#include <juce_core/juce_core.h>

#include <functional>

namespace rig
{
    struct LibraryImportResult
    {
        int imported = 0;
        int alreadyPresent = 0;
        juce::StringArray skipped, failed;
        bool cancelled = false;
    };

    bool isLibraryFile (const juce::File& file);

    // 폴더는 건너뛴다. 파일은 복사하고, 같은 이름은 (2), (3)... 을 붙인다.
    // 오디오/UI 에 접근하지 않는다. 취소는 파일 사이와 복사 버퍼마다 확인한다.
    LibraryImportResult importLibraryFiles (const juce::StringArray& files,
                                            const juce::File& namDirectory,
                                            const juce::File& irDirectory,
                                            const std::function<bool()>& shouldCancel);
}
