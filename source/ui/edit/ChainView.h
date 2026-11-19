#pragma once

#include "model/RigModel.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 화면 상단: 패치 맵 (직렬 체인).
    //   [IN] -+- [블록1] -+- [블록2] -+- [OUT]
    // IN / OUT 에는 오디오 장치에서 쓰고 있는 채널 번호를 표시한다.
    // 이웃한 두 노드 사이 연결선 가운데의 [+] 를 누르면 그 자리에 미지정 블록이 들어간다.
    // 블록 수 제한 없음. 넘치면 가로 스크롤.
    // 선택된 블록의 파일명이 블록보다 길면 이름 칸 안에서 왼쪽으로 흘러간다 (멈춤 → 흐름 → 멈춤 → 처음으로, 반복).
    class ChainView final : public juce::Component,
                            private juce::ChangeListener,
                            private juce::Timer
    {
    public:
        ChainView (RigModel& model, juce::AudioDeviceManager& deviceManager);
        ~ChainView() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class Tile;
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
        juce::AudioDeviceManager& deviceManager;   // 장치(채널)가 바뀌면 IN / OUT 표시를 다시 그린다
        juce::Viewport viewport;
        std::unique_ptr<Strip> strip;
        juce::uint64 shownSelectedId = 0;   // 선택이 바뀌었을 때만 그 블록이 보이도록 스크롤
        MarqueeKey marqueeKey;
        juce::uint32 marqueeStartMs = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainView)
    };
}
