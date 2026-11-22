#pragma once

#include "ui/KnobLookAndFeel.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 파라미터 노브 묶음: [노브] 오른쪽에 두 줄 (위 이름, 아래 값), 묶음은 세로 가운데. 폭은 width 고정.
    // 이름과 값은 항상 보인다. 비활성(setEnabled false)이면 흐리고 조작 안 됨.
    // 왼쪽 클릭 드래그만, 더블클릭 = 기본값, 휠·글자 입력 없음 (볼륨 노브와 같은 규칙)
    class ParamKnob final : public juce::Component
    {
    public:
        enum class ValueType
        {
            Db,      // 세기: 선형 매핑, "+3.0 dB"
            HzLog    // 주파수: 로그 매핑(노브 가운데 = 기하 평균), "250 Hz" / "1.25 kHz"
        };

        // 크기: 노브 지름 55 (상태바 볼륨 노브 56 과 비슷하게) + 간격 8 + 글자 칸 64
        static constexpr int knobDiameter = 55, textGap = 8, textWidth = 64;
        static constexpr int width = knobDiameter + textGap + textWidth;

        ParamKnob (const juce::String& name, double minValue, double maxValue, double defaultValue, ValueType type);
        ~ParamKnob() override;

        std::function<void (double)> onValueChanged;   // 사용자가 돌릴 때만
        std::function<void()> onDragEnded;             // 드래그가 끝났을 때 (저장은 여기서)

        void setValue (double value);                  // 콜백을 부르지 않는다
        double getValue() const { return slider.getValue(); }

        void resized() override;
        void enablementChanged() override;

    private:
        juce::String toText (double value) const;
        void updateValueLabel();

        const ValueType type;

        // slider 보다 먼저 선언: 멤버는 역순으로 소멸하므로 slider 가 먼저 사라진다
        KnobLookAndFeel knobLnf;
        LeftClickOnly<juce::Slider> slider;
        juce::Label nameLabel, valueLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamKnob)
    };
}
