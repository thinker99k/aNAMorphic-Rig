#pragma once

#include "model/RigModel.h"
#include "ui/Widgets.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 화면 상단: 패치 맵 (직렬/병렬 체인).
    //   [IN] --- [블록1] ---+--- [블록2] ---[⚙]--- [OUT]      갈라지는 곳 = 선만, [⚙] = 합류 버튼 (누르면 선택, 끌면 합류 지점 이동)
    //                       └--- [블록3] ----┘
    // IN / OUT 에는 패치가 쓰는 장치 채널 번호를 표시한다. 누르면 선택되어 파라미터 바에서 채널을 고른다.
    // 지금 장치에 없는 채널이면 블록의 로딩 실패와 같은 빨간 빗금.
    // 블록 추가·교체·이동은 끌어 놓기로 (선택기 항목 → 패치맵, 패치맵 블록 → 다른 자리). 놓을 자리 규칙은 Strip 참고.
    // 블록 수 제한 없음. 넘치면 가로 스크롤.
    // 선택된 블록의 파일명이 블록보다 길면 이름 칸 안에서 왼쪽으로 흘러간다 (멈춤 → 흐름 → 멈춤 → 처음으로, 반복).
    class ChainView final : public juce::Component,
                            private juce::ChangeListener,
                            private juce::Timer
    {
    public:
        ChainView (RigModel& model, juce::AudioDeviceManager& deviceManager);
        ~ChainView() override;

        // 편집 화면이 숨겨진 동안(Live 모드) 변경 알림으로 다시 만들지 않는다. 다시 켜면 한 번 다시 만든다
        void setUpdatesPaused (bool paused);

        // [SPECTRUM] 오른쪽 위 고정 [Spectrum] 버튼을 보이고 누르면 onClick (실행 인자 --spectrum 일 때만 부른다)
        void showSpectrumButton (std::function<void()> onClick);   // [SPECTRUM]

        // Edit Stomp 모드에서 패치맵 블록을 선택기·ssbar 쪽으로 끌면 ssbar 로 가는 것 (EditView 가 SceneBar 와 이어 준다). 화면 좌표.
        //  markAt: 그쪽이면 떼면 일어날 일 (추가 + / 대체 ↔ / none = 아무 일 없음), 아니면 nullopt (패치맵 규칙 그대로).
        //          그쪽에서 떼면 블록은 지워지지 않고 ssbar 가 받는다
        //  image : Stomp 칸 모양 끄는 그림
        struct StompDrag
        {
            std::function<std::optional<block::DragMark> (juce::Point<int>)> markAt;
            std::function<juce::Image (juce::uint64 slotId, block::DragMark)> image;
        };
        StompDrag stompDrag;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class Tile;
        class MergeButton;
        class Strip;

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void timerCallback() override;   // 흘러가는 파일명 다시 그리기 (흘러갈 블록이 있을 때만 돈다)
        void rebuild();

        // 흘러가는 위치를 처음으로 되돌리는 기준: 선택 블록, 그 파일, 패치, 씬
        struct MarqueeKey
        {
            juce::uint64 slotId = 0;
            juce::File file;
            int patch = 0, scene = 0;

            bool operator== (const MarqueeKey& o) const
            {
                return slotId == o.slotId && file == o.file && patch == o.patch && scene == o.scene;
            }
        };

        RigModel& model;
        juce::AudioDeviceManager& deviceManager;   // 장치가 바뀌면 IN / OUT 빗금(없는 채널)을 다시 따진다
        juce::Viewport viewport;
        // 왼쪽 위 고정 문구 (스크롤 안 됨, 클릭 통과). 선택된 블록 / IN·OUT 기준, 위에서부터 오류 → 경고
        //  오류(빨강) = 블록 로딩 실패, 장치에 없는 채널. 경고(노랑) = NAM 모델 샘플레이트 ≠ 장치 샘플레이트
        juce::Label errorLabel, warningLabel;
        LeftClickOnly<juce::TextButton> spectrumButton { "Spectrum" };   // [SPECTRUM] 오른쪽 위 고정, 평소에는 숨김
        std::unique_ptr<Strip> strip;
        juce::uint64 shownSelectedId = 0;   // 선택이 바뀌었을 때만 그 블록이 보이도록 스크롤
        MarqueeKey marqueeKey;
        juce::uint32 marqueeStartMs = 0;
        bool updatesPaused = false;
        HighlightRepaint highlightRepaint { *this, [this] { return model.getHighlightStart(); } };   // Undo / Redo 강조가 사라지는 동안

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainView)
    };
}
