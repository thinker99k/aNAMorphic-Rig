#pragma once

#include "model/RigModel.h"
#include "tuner/TunerAnalyzer.h"
#include "tuner/TunerGauge.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 튜너 창 내용물 (420 x 270, 제목 표시줄 22 를 더해 창 420 x 292). 창은 아래 TunerWindow (모달 아님)
    //  - 게이지 패널 404 x 254 하나. 아래 줄 [440 Hz] [음 이름] [오차 cents] (Logic 튜너 배치)
    //  - 440 Hz = 기준음 값 상자 (게이지 위에 겹침. 클릭 = 직접 입력, 위아래로 끌기 = 조절, 1 Hz 단위)
    //  - 열려 있는 동안 엔진을 튜너 모드로 (출력 무음), 엔진이 기록한 입력(지금 패치의 입력 채널)을 20 Hz 로 분석한다
    //  - 닫기: 창의 X, ESC (튜너 창에 포커스가 있을 때), 메인 창 Tuner 버튼. 기준음은 RigModel 에 바로 넣고, 저장은 끌기 끝·입력 적용·앱 종료 때
    class TunerDialog final : public juce::Component,
                              private juce::Timer
    {
    public:
        TunerDialog (AudioEngine& engine, RigModel& model);
        ~TunerDialog() override;

        void close();   // 튜너를 끄고 (무음 해제) onClosed 호출. 두 번 불러도 한 번만
        std::function<void()> onClosed;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;   // 값 상자 끌기 (pitchLabel 의 마우스 이벤트)
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;     // 끌기가 끝나면 settings.json 저장 (볼륨 노브와 같이)

    private:
        void timerCallback() override;
        void pitchTyped();   // 값 상자에 직접 입력한 Hz 적용
        void setPitchHz (int hz);   // RigModel 에 넣고 값 상자 표시
        void updatePitchLabel();

        AudioEngine& engine;
        RigModel& model;
        bool closed = false;

        TunerAnalyzer analyzer;

        int dragStartHz = 0;   // 값 상자를 끌기 시작할 때의 기준음

        TunerGauge gauge;
        LeftClickOnly<juce::Label> pitchLabel;   // 클릭하면 직접 입력

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerDialog)
    };

    // 튜너 창 (모달 아님): 열려 있는 동안에도 메인 창을 조작할 수 있다. 만들 때 메인 창 가운데에 띄우고 포커스를 가져온다
    //  - X 나 ESC 를 누르면 튜너를 끄고 숨는다 (창 객체는 주인이 다음에 열 때 새로 만들거나 함께 지운다)
    //  - 제목 표시줄 끌기도 왼쪽 클릭만 (SettingsDialog 와 같은 방식)
    class TunerWindow final : public LeftClickOnly<juce::DialogWindow>
    {
    public:
        TunerWindow (AudioEngine& engine, RigModel& model, juce::Component* parent, std::function<void()> onClosed);

        void close();   // 메인 창 Tuner 버튼, X, ESC

        void closeButtonPressed() override;
        bool escapeKeyPressed() override;

    private:
        TunerDialog& dialog;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerWindow)
    };
}
