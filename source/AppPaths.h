#pragma once

#include <juce_core/juce_core.h>

#include <filesystem>

// 사용자 데이터 위치. OS에 상관없이 "문서 폴더/aNAMorphic-Rig" 아래에 둔다.
//   Linux   : /home/<user>/Documents/aNAMorphic-Rig
//   macOS   : /Users/<user>/Documents/aNAMorphic-Rig
//   Windows : C:\Users\<user>\Documents\aNAMorphic-Rig
//
//   aNAMorphic-Rig/
//   ├── settings.json        앱 전역 설정: 입력/출력 볼륨
//   ├── audio-device.xml     오디오 장치 설정
//   ├── nam/                 *.nam 캡처 (BlockEditor 목록)
//   ├── ir/                  *.wav IR   (BlockEditor 목록)
//   └── patch/config01~32    패치별 체인 (JSON)
//
// 처음 실행할 때 빠진 폴더/파일은 defaults::ensureAll() 이 만든다.
namespace rig::paths
{
    juce::File root();                    // ~/Documents/aNAMorphic-Rig (없으면 생성)
    juce::File namDir();                  // .../nam    (*.nam 검색 위치)
    juce::File irDir();                   // .../ir     (*.wav 검색 위치)
    juce::File patchDir();                // .../patch
    juce::File patchFile (int number);    // .../patch/config<NN>  (패치 N의 체인, 두 자리: config01)
    juce::File deviceSettingsFile();      // .../audio-device.xml
    juce::File settingsFile();            // .../settings.json (앱 전역 설정)

    // juce::File -> std::filesystem::path. Windows에서 한글 경로가 깨지지 않도록 wide 문자열 사용.
    std::filesystem::path toStdPath (const juce::File& file);
}
