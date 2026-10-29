#pragma once

#include <juce_core/juce_core.h>

#include <filesystem>

// 사용자 데이터 위치. OS에 상관없이 "홈 폴더/aNAMorphic-Rig" 아래에 둔다.
//   Linux   : /home/<user>/aNAMorphic-Rig
//   macOS   : /Users/<user>/aNAMorphic-Rig
//   Windows : C:\Users\<user>\aNAMorphic-Rig
namespace rig::paths
{
    juce::File root();                    // ~/aNAMorphic-Rig (없으면 생성)
    juce::File patchFile (int number);    // ~/aNAMorphic-Rig/config<N>  (패치 N의 체인)
    juce::File deviceSettingsFile();      // ~/aNAMorphic-Rig/audio-device.xml
    juce::File capturesDir();             // ~/aNAMorphic-Rig/captures  (*.nam 검색 위치)
    juce::File irsDir();                  // ~/aNAMorphic-Rig/irs       (*.wav 검색 위치)

    // juce::File -> std::filesystem::path. Windows에서 한글 경로가 깨지지 않도록 wide 문자열 사용.
    std::filesystem::path toStdPath (const juce::File& file);
}
