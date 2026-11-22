#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Widgets.h"

#include <functional>

namespace rig
{
    // 볼륨 노브 하나: 회전 노브 / 아래쪽 이름 (INPUT, OUTPUT). dB 값은 노브 옆(상태바 게이지 오른쪽)에 늘 표시
    class GainKnob final : public juce::Component
    {
    public:
        // minIsInf: true 면 최솟값을 "-inf dB" 로 표시 (출력), false 면 실제 dB 로 표시 (입력)
        GainKnob (const juce::String& name, float minDb, float maxDb, bool minIsInf);
        ~GainKnob() override;

        // 사용자가 노브를 돌렸을 때만 호출 (dB)
        std::function<void (float)> onValueChanged;

        // 사용자가 드래그를 끝냈을 때. 저장은 여기서
        std::function<void()> onDragEnded;

        // 외부에서 값만 바꾼다 (onValueChanged 는 부르지 않음)
        void setValueDb (float db);

        // 지금 값 글자 ("+1.5 dB", "0.0 dB", 출력 최솟값은 "-inf dB")
        juce::String getValueText() const { return toText (slider.getValue()); }

        // 아래쪽 이름 글자 표시 여부 (기본 true). 상태바처럼 옆에 이름이 있는 곳에서는 숨긴다
        void setNameVisible (bool shouldBeVisible);

        void resized() override;

    private:
        juce::String toText (double db) const;

        const float minDb;
        const bool minIsInf;
        bool nameVisible = true;

        // slider 보다 먼저 선언: 멤버는 역순으로 소멸하므로 slider 가 먼저 사라진다
        KnobLookAndFeel knobLnf;
        LeftClickOnly<juce::Slider> slider;
        juce::Label nameLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainKnob)
    };
}
