#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 설정 모달 다이얼로그 내용물.
    //  - 입력 소스(드라이버 종류 + 입력 장치), 입력 채널 선택
    //  - [Save] 를 누르면 장치에 적용하고 ~/aNAMorphic-Rig/audio-device.xml 에 저장
    //  - [Cancel] 을 누르면 아무것도 바꾸지 않고 닫힘
    //  - 창 닫기(X)와 ESC 는 없앰: 저장/취소 중 하나로만 나갈 수 있음
    class SettingsDialog final : public juce::Component
    {
    public:
        explicit SettingsDialog (juce::AudioDeviceManager& deviceManager);

        // 모달 창으로 띄운다. 닫힐 때 saved=true/false 로 onClosed 호출
        static void show (juce::AudioDeviceManager& deviceManager,
                          juce::Component* parent,
                          std::function<void (bool saved)> onClosed);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void refreshDeviceTypes();
        void refreshInputDevices();
        void refreshInputChannels();
        juce::AudioIODeviceType* selectedType() const;
        void save();
        void close (bool saved);

        juce::AudioDeviceManager& deviceManager;
        std::function<void (bool)> onClosed;

        juce::Label typeLabel    { {}, "Audio driver" };
        juce::Label deviceLabel  { {}, "Input source" };
        juce::Label channelLabel { {}, "Input channel" };
        juce::ComboBox typeBox, deviceBox, channelBox;
        juce::Label note;

        juce::TextButton saveButton   { "Save" };
        juce::TextButton cancelButton { "Cancel" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsDialog)
    };
}
