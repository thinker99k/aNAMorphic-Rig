#pragma once

#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 파라미터 노브: [노브] 아래 [이름], 둘 다 가로 가운데, 묶음은 세로 가운데. 폭은 받은 영역 전체 (getIdealWidth = 노브와 이름 중 넓은 쪽).
    // 값은 노브에 마우스를 올려 두거나 끄는 동안만 노브 바로 위 팝업으로 보인다 (비활성이면 안 뜬다).
    // 팝업은 파라미터 바 밖(패치 맵 아래쪽)까지 걸치므로 창 맨 위 컴포넌트의 자식으로 띄운다 (자식은 부모 영역 밖으로 그려지지 않음).
    // 비활성(setEnabled false)이면 흐리고 조작 안 됨.
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

        // 크기: 노브 지름 50 (상태바 볼륨 노브 56 보다 작게) + 간격 2 + 이름 한 줄 16
        static constexpr int knobDiameter = 50, nameGap = 2;

        int getIdealWidth() const { return juce::jmax (knobDiameter, nameWidth); }

        ParamKnob (const juce::String& name, double minValue, double maxValue, double defaultValue, ValueType type);
        ~ParamKnob() override;

        std::function<void (double)> onValueChanged;   // 사용자가 돌릴 때만
        std::function<void()> onDragEnded;             // 드래그가 끝났을 때 (저장은 여기서)

        void setValue (double value);                  // 콜백을 부르지 않는다
        double getValue() const { return slider.getValue(); }

        void resized() override;
        void enablementChanged() override;

        // 노브(slider)의 마우스 이벤트: 팝업 보이기 / 숨기기
        void mouseEnter (const juce::MouseEvent&) override  { updatePopup(); }
        void mouseExit (const juce::MouseEvent&) override   { updatePopup(); }
        void mouseDown (const juce::MouseEvent&) override   { updatePopup(); }
        void mouseUp (const juce::MouseEvent&) override     { updatePopup(); }

    private:
        // 값 팝업: 어두운 둥근 사각형 + 흰 값 글자. 클릭을 받지 않는다 (호버가 끊기지 않게)
        struct ValuePopup final : public juce::Component
        {
            ValuePopup() { setInterceptsMouseClicks (false, false); }
            void paint (juce::Graphics&) override;

            juce::String text;
        };

        juce::String toText (double value) const;
        void updatePopup();

        const ValueType type;
        int nameWidth = 0, popupWidth = 0;   // 생성자에서 한 번 잰다 (팝업 폭 = 가장 긴 값, 값이 바뀌어도 흔들리지 않게)

        // slider 보다 먼저 선언: 멤버는 역순으로 소멸하므로 slider 가 먼저 사라진다
        KnobLookAndFeel knobLnf;
        LeftClickOnly<juce::Slider> slider;
        juce::Label nameLabel;
        ValuePopup popup;   // 소멸 때 창 맨 위 컴포넌트에서 스스로 빠진다

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamKnob)
    };
}
