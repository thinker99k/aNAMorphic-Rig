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
        // 배치 공용 (노브 묶음 폭은 ParamKnob::width, 높이는 패널 전체라 묶음이 세로 가운데에 온다)
        static constexpr int pairGap = 12;     // 같은 영역을 다루는 노브 사이 (예: B 와 B. Freq)
        static constexpr int powerGap = 8;     // 전원 버튼과 노브 사이 (합류 화면과 같다)
        static constexpr int groupGap = 64;    // 영역 사이 (Bass / Middle / Treble, L. Cut / H. Cut). 합류 화면의 갈래 사이와 같다
    };

    // 블록 종류에 맞는 패널. 파라미터가 없는 종류는 nullptr.
    // 블록 종류를 추가할 때는 여기에 한 경우를 더한다
    std::unique_ptr<ParamPanel> createParamPanel (BlockType type, RigModel& model);
}
