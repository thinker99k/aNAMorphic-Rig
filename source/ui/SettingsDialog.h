#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Widgets.h"

#include <functional>

namespace rig
{
    // 설정 모달 다이얼로그 내용물.
    //  - 입력 소스(드라이버 종류 + 입력 장치), 출력 장치, 샘플레이트와 버퍼 크기 선택
    //  - 채널은 장치의 모든 채널을 열고, 어느 채널을 쓸지는 패치(IN / OUT 노드)에서 고른다
    //  - [Save] 를 누르면 장치에 적용하고 ~/Documents/aNAMorphic-Rig/audio-device.xml 에 저장
    //  - [Refresh Library] 는 즉시 라이브러리만 갱신 (오디오 설정 저장과 독립)
    //  - [Cancel] 을 누르면 오디오 설정을 적용하지 않고 닫힘
    //  - 창 닫기(X)와 ESC 는 없앰: 저장/취소 중 하나로만 나갈 수 있음
    class SettingsDialog final : public juce::Component
    {
    public:
        explicit SettingsDialog (juce::AudioDeviceManager& deviceManager);

        // 모달 창으로 띄운다. 닫힐 때 saved=true/false 로 onClosed 호출
        static void show (juce::AudioDeviceManager& deviceManager,
                          juce::Component* parent,
                          std::function<void (bool saved)> onClosed,
                          std::function<void()> onRefreshLibrary);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void refreshDeviceTypes();
        void refreshInputDevices();
        void refreshOutputDevices();
        void refreshFormatOptions();
        juce::String selectedInputName() const;
        juce::AudioIODeviceType* selectedType() const;
        void save();
        void close (bool saved);

        juce::AudioDeviceManager& deviceManager;
        std::function<void (bool)> onClosed;

        juce::Label typeLabel    { {}, "Audio driver" };
        juce::Label deviceLabel  { {}, "Input source" };
        LeftClickOnly<juce::ComboBox> typeBox, deviceBox;
        juce::Label outputDeviceLabel  { {}, "Output device" };
        LeftClickOnly<juce::ComboBox> outputDeviceBox;

        juce::Label rateLabel { {}, "Sample rate" };
        juce::Label bufferLabel { {}, "Buffer size" };
        LeftClickOnly<juce::ComboBox> rateBox, bufferBox;
        juce::Array<double> sampleRates;
        juce::Array<int> bufferSizes;

        LeftClickOnly<juce::TextButton> saveButton   { "Save" };
        LeftClickOnly<juce::TextButton> cancelButton { "Cancel" };
        LeftClickOnly<juce::TextButton> refreshButton { "Refresh Library" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsDialog)
    };
}
