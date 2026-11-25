#pragma once

#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 파라미터 노브 묶음: [노브] 오른쪽에 두 줄 (위 이름, 아래 값), 묶음은 세로 가운데. 폭은 getIdealWidth (글자 칸 = 실제 글꼴로 잰 폭).
    // 이름과 값은 항상 보인다. 비활성(setEnabled false)이면 흐리고 조작 안 됨.
    // 왼쪽 클릭 드래그만, 더블클릭 = 기본값, 휠·글자 입력 없음 (볼륨 노브와 같은 규칙)
    class ParamKnob final : public juce::Component
    {
    public:
        enum class ValueType
        {
            Db,      // 세기: 선형 매핑, "+3.0 dB"
            HzLog,   // 주파수: 로그 매핑(노브 가운데 = 기하 평균), "250 Hz" / "1.234 kHz"
            QLog     // 폭 Q: 로그 매핑, "0.80"
        };

        // 크기: 노브 지름 44 (상태바 볼륨 노브 56 보다 작게) + 간격 4 + 글자 칸. 보조 노브(M 의 Q 등)는 조금 작게 36
        static constexpr int knobDiameter = 44, smallKnobDiameter = 36, textGap = 4;

        // 노브 + 간격 + 글자 칸. 글자 칸 = 이름과 이 노브에 나올 수 있는 가장 긴 값 중 넓은 쪽 (OS 글꼴이 달라도 맞는다)
        int getIdealWidth() const { return diameter + textGap + textWidth; }
        void setDiameter (int newDiameter) { diameter = newDiameter; resized(); }   // 기본 knobDiameter

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
        int textWidth = 0;   // 생성자에서 한 번 잰다
        int diameter = knobDiameter;

        // slider 보다 먼저 선언: 멤버는 역순으로 소멸하므로 slider 가 먼저 사라진다
        KnobLookAndFeel knobLnf;
        LeftClickOnly<juce::Slider> slider;
        juce::Label nameLabel, valueLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamKnob)
    };
}
