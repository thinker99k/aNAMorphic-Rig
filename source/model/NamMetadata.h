#pragma once

#include <juce_core/juce_core.h>

#include <optional>

namespace rig
{
    struct NamMetadata
    {
        juce::String gearType;
        juce::String name;
    };

    // DSP 생성 없이 metadata.gear_type과 metadata.name을 읽는다.
    // 누락/null 값은 빈 문자열, 파일/JSON/타입 오류는 nullopt와 error로 반환한다.
    // 파일 전체를 파싱하므로 오디오 스레드나 paint 함수에서 호출하지 말 것.
    std::optional<NamMetadata> readNamMetadata (const juce::File& file, juce::String& error);
}
