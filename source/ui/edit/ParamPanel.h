#pragma once

#include "engine/Block.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace rig
{
    class RigModel;

    // ParamBar 왼쪽에 들어가는, 블록 종류별 파라미터 화면
    class ParamPanel : public juce::Component
    {
    public:
        virtual void refresh() = 0;   // 모델 값 -> 화면 (패치가 바뀌면 파라미터 객체도 다시 얻는다)
    };

    // 블록 종류에 맞는 패널. 파라미터가 없는 종류는 nullptr.
    // 블록 종류를 추가할 때는 여기에 한 경우를 더한다
    std::unique_ptr<ParamPanel> createParamPanel (BlockType type, RigModel& model);
}
