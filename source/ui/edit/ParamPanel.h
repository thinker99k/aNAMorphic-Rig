#pragma once

#include "engine/Block.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace rig
{
    class RigModel;

    // ParamBar 에 들어가는, 블록 종류별 파라미터 화면. 패널 = 파라미터 바의 14칸 격자 영역 전체 (bottomBarCell, 씬 바와 같은 격자).
    // 패널이 bottomBarCell 로 칸을 직접 고르고, 항목 정렬은 패널마다 (NAM = 칸 가운데, IR = 두 칸 왼쪽 끝) (0 = 리셋, 13 = 블록 전원).
    // 패널 자체는 클릭을 받지 않는다 (같은 줄의 리셋·전원 버튼 위에 겹쳐 있으므로), 노브만 받는다
    class ParamPanel : public juce::Component
    {
    public:
        ParamPanel() { setInterceptsMouseClicks (false, true); }

        virtual void refresh() = 0;   // 모델 값(선택된 블록의 현재 씬) -> 화면

        // 파라미터 번호(BlockParams::specs 순서)를 다루는 컨트롤 (Undo / Redo 강조용). 공용 Output 은 ParamBar 가 갖는다
        virtual juce::Component* componentForParam (int) { return nullptr; }
    };

    // 블록 종류에 맞는 패널. 파라미터가 없는 종류는 nullptr.
    // 블록 종류를 추가할 때는 여기에 한 경우를 더한다
    std::unique_ptr<ParamPanel> createParamPanel (BlockType type, RigModel& model);
}
