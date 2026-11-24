#pragma once

#include <juce_core/juce_core.h>

#include <functional>

namespace rig
{
    struct LibraryImportResult
    {
        juce::Array<juce::File> imported;   // 저장한 라이브러리 파일 (nam/ 또는 ir/ 바로 아래)
        juce::StringArray failed;           // 가져오지 못한 항목의 이름 (이유는 따로 두지 않는다)
        bool cancelled = false;             // 앱 종료로 중단
    };

    // 드롭한 항목마다 성공 / 실패만 있다.
    //  - .nam 은 namDirectory, .wav 는 irDirectory 바로 아래에 같은 이름으로 저장 (확장자는 소문자). 같은 이름이 있으면 덮어쓴다.
    //    라이브러리 안의 파일(자기 자신 포함)도 예외 없이 다시 쓴다
    //  - 폴더, 다른 확장자, 없는 파일, 복사 오류는 실패
    //  - 숨은 임시 파일에 다 쓴 뒤에만 교체하므로 실패·취소해도 기존 파일은 그대로
    // 오디오/UI 에 접근하지 않는다. 취소는 항목 사이와 복사 버퍼마다 확인한다.
    LibraryImportResult importLibraryFiles (const juce::StringArray& files,
                                            const juce::File& namDirectory,
                                            const juce::File& irDirectory,
                                            const std::function<bool()>& shouldCancel);
}
