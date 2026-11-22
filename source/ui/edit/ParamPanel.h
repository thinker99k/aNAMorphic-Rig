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
        virtual void refresh() = 0;   // 모델 값(선택된 블록의 현재 씬) -> 화면

    protected:
        // 배치 공용: 노브 칸 폭 = 패널 폭 / knobColumns (NAM 노브 6개가 한 줄을 채우고, IR 노브도 같은 크기)
        static constexpr int knobColumns = 6;
        static constexpr int knobMargin = 4;   // 노브 칸 위아래 여백
        static constexpr int powerGap = 8;     // 전원 버튼과 노브 사이 (합류 화면과 같다)
        static constexpr int groupGap = 64;    // [전원][노브] 묶음 사이 (합류 화면의 갈래 사이와 같다)

        int knobWidth() const { return getWidth() / knobColumns; }
    };

    // 블록 종류에 맞는 패널. 파라미터가 없는 종류는 nullptr.
    // 블록 종류를 추가할 때는 여기에 한 경우를 더한다
    std::unique_ptr<ParamPanel> createParamPanel (BlockType type, RigModel& model);
}
