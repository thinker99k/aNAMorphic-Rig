#pragma once

#include <juce_core/juce_core.h>

#include <optional>

namespace rig
{
    struct NamMetadata
    {
        juce::String gearType;
        juce::String name;
        juce::String gearModel;
        juce::String toneType;
        juce::String modeledBy;
    };

    // DSP 생성 없이 metadata 의 gear_type, name, gear_model, tone_type, modeled_by 를 읽는다.
    // 필드별로 검사해서 문자열이 아닌(누락/null 포함) 필드만 빈 문자열이 되고 나머지는 그대로 둔다.
    // 파일/JSON 오류나 metadata 가 객체가 아니면 nullopt와 error로 반환한다.
    // 파일 전체를 파싱하므로 오디오 스레드나 paint 함수에서 호출하지 말 것.
    std::optional<NamMetadata> readNamMetadata (const juce::File& file, juce::String& error);
}
