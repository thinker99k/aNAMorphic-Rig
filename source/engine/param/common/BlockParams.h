#pragma once

#include "engine/Block.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

namespace rig
{
    // 블록 종류(BlockType)별로 패치당 하나 있는 파라미터 묶음. 씬과 무관하고 패치 파일의 "params" 에 저장된다.
    //  - 값은 원자 변수로 둔다: UI(메시지) 스레드가 쓰고 오디오 스레드(ParamStage)가 읽는다
    //  - toVar / fromVar / setDefaults 는 메시지 스레드에서만
    class BlockParams
    {
    public:
        virtual ~BlockParams() = default;

        virtual juce::var toVar() const = 0;                 // 저장용
        virtual void fromVar (const juce::var&) = 0;         // 불러오기: 없거나 이상한 값은 기본값, 범위 안으로 제한
        virtual void setDefaults() = 0;

        static std::shared_ptr<BlockParams> create (BlockType);   // 파라미터가 없는 종류는 nullptr
        static const std::vector<BlockType>& paramTypes();        // 파라미터가 있는 종류 목록 = 여기서만 등록

    protected:
        // fromVar 공용: 숫자가 아니면 def, 숫자면 [lo, hi] 로 제한
        static double readNumber (const juce::var& obj, const juce::Identifier& key, double def, double lo, double hi);
        // bool 이 아니면 def
        static bool readBool (const juce::var& obj, const juce::Identifier& key, bool def);
    };
}
