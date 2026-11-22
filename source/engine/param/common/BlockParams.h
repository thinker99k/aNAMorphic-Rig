#pragma once

#include "engine/Block.h"

#include <juce_core/juce_core.h>

#include <memory>

namespace rig
{
    // 블록 하나의 파라미터 묶음 (블록마다 한 벌). 씬과 무관하고 패치 파일의 블록 항목 "params" 에 저장된다.
    //  - 값은 원자 변수로 둔다: UI(메시지) 스레드가 쓰고 오디오 스레드(ParamStage)가 읽는다
    //  - toVar / fromVar 는 메시지 스레드에서만. 만든 직후의 값 = 기본값
    class BlockParams
    {
    public:
        virtual ~BlockParams() = default;

        virtual juce::var toVar() const = 0;                 // 저장용
        virtual void fromVar (const juce::var&) = 0;         // 불러오기: 없거나 이상한 값은 기본값, 범위 안으로 제한

        static std::shared_ptr<BlockParams> create (BlockType);   // 기본값으로. 파라미터가 없는 종류는 nullptr

    protected:
        // fromVar 공용: 숫자가 아니면 def, 숫자면 [lo, hi] 로 제한
        static double readNumber (const juce::var& obj, const juce::Identifier& key, double def, double lo, double hi);
        // bool 이 아니면 def
        static bool readBool (const juce::var& obj, const juce::Identifier& key, bool def);
    };
}
