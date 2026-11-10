#include "SettingsDialog.h"

#include "AppPaths.h"

namespace rig
{
    namespace
    {
        constexpr int noDeviceId = 1;   // deviceBox 의 "(none)" 항목 id

        // outputChannelBox 항목 id: 스테레오 쌍은 pairIdBase + 첫 채널(0부터), 단일 채널은 1 + 채널(0부터)
        constexpr int pairIdBase = 1000;
    }

    SettingsDialog::SettingsDialog (juce::AudioDeviceManager& dm) : deviceManager (dm)
    {
        for (auto* l : { &typeLabel, &deviceLabel, &channelLabel, &outputDeviceLabel, &outputChannelLabel })
        {
            l->setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (l);
        }

        addAndMakeVisible (typeBox);
        addAndMakeVisible (deviceBox);
        addAndMakeVisible (channelBox);
        addAndMakeVisible (outputDeviceBox);
        addAndMakeVisible (outputChannelBox);

        typeBox.onChange   = [this] { refreshInputDevices(); refreshOutputDevices(); };
        deviceBox.onChange = [this]
        {
            refreshInputChannels();

            // 입출력이 한 장치인 드라이버(JACK/ASIO 등)는 출력 장치가 입력 장치를 따라간다
            if (auto* type = selectedType(); type != nullptr && ! type->hasSeparateInputsAndOutputs())
                refreshOutputDevices();
        };
        outputDeviceBox.onChange = [this] { refreshOutputChannels(); };

        saveButton.onClick   = [this] { save(); };
        cancelButton.onClick = [this] { close (false); };
        addAndMakeVisible (saveButton);
        addAndMakeVisible (cancelButton);

        refreshDeviceTypes();
        setSize (460, 260);
    }

    void SettingsDialog::show (juce::AudioDeviceManager& dm,
                               juce::Component* parent,
                               std::function<void (bool)> onClosed)
    {
        auto content = std::make_unique<SettingsDialog> (dm);
        content->onClosed = std::move (onClosed);

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (content.release());
        options.dialogTitle = "Settings";
        options.componentToCentreAround = parent;
        options.escapeKeyTriggersCloseButton = false;   // ESC 로 빠져나가지 못하게
        options.useNativeTitleBar = false;              // JUCE 타이틀바를 써야 닫기 버튼을 없앨 수 있음
        options.resizable = false;
        options.dialogBackgroundColour = parent != nullptr
            ? parent->getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId)
            : juce::Colours::darkgrey;

        auto* window = options.create();

        // X(닫기) 버튼 제거 -> Save/Cancel 로만 종료.
        // 주의: 이 호출은 창(peer)을 다시 만들기 때문에 반드시 모달로 만들기 "전에" 해야 한다.
        //       launchAsync() 뒤에 호출하면 모달 상태가 풀리고 창이 바로 삭제된다.
        window->setTitleBarButtonsRequired (0, false);

        // 모달로 띄운다: 이 창이 닫히기 전에는 메인 창을 조작할 수 없음 (닫히면 창은 스스로 삭제)
        window->enterModalState (true, nullptr, true);
    }

    //==============================================================================
    void SettingsDialog::refreshDeviceTypes()
    {
        typeBox.clear (juce::dontSendNotification);

        const auto& types = deviceManager.getAvailableDeviceTypes();
        const auto current = deviceManager.getCurrentAudioDeviceType();

        for (int i = 0; i < types.size(); ++i)
        {
            typeBox.addItem (types[i]->getTypeName(), i + 1);

            if (types[i]->getTypeName() == current)
                typeBox.setSelectedId (i + 1, juce::dontSendNotification);
        }

        if (typeBox.getSelectedId() == 0 && typeBox.getNumItems() > 0)
            typeBox.setSelectedItemIndex (0, juce::dontSendNotification);

        refreshInputDevices();
        refreshOutputDevices();
    }

    juce::AudioIODeviceType* SettingsDialog::selectedType() const
    {
        const auto& types = deviceManager.getAvailableDeviceTypes();
        const int index = typeBox.getSelectedId() - 1;
        return juce::isPositiveAndBelow (index, types.size()) ? types[index] : nullptr;
    }

    void SettingsDialog::refreshInputDevices()
    {
        deviceBox.clear (juce::dontSendNotification);
        deviceBox.addItem ("(none)", noDeviceId);

        if (auto* type = selectedType())
        {
            type->scanForDevices();
            const auto names = type->getDeviceNames (true);

            const auto setup = deviceManager.getAudioDeviceSetup();
            const bool isCurrentType = type->getTypeName() == deviceManager.getCurrentAudioDeviceType();

            for (int i = 0; i < names.size(); ++i)
            {
                deviceBox.addItem (names[i], noDeviceId + 1 + i);

                if (isCurrentType && names[i] == setup.inputDeviceName)
                    deviceBox.setSelectedId (noDeviceId + 1 + i, juce::dontSendNotification);
            }

            if (deviceBox.getSelectedId() == 0)
                deviceBox.setSelectedId (names.isEmpty() ? noDeviceId
                                                         : noDeviceId + 1 + type->getDefaultDeviceIndex (true),
                                         juce::dontSendNotification);
        }

        refreshInputChannels();
    }

    void SettingsDialog::refreshInputChannels()
    {
        channelBox.clear (juce::dontSendNotification);

        auto* type = selectedType();
        const auto deviceName = deviceBox.getSelectedId() == noDeviceId ? juce::String() : deviceBox.getText();

        if (type == nullptr || deviceName.isEmpty())
        {
            channelBox.setEnabled (false);
            return;
        }

        channelBox.setEnabled (true);

        // 지금 열려 있는 장치면 그대로 쓰고, 아니면 열지 않은 임시 장치 객체에서 채널 이름만 얻는다
        juce::StringArray channelNames;
        auto setup = deviceManager.getAudioDeviceSetup();
        auto* current = deviceManager.getCurrentAudioDevice();
        const bool isCurrent = current != nullptr
                               && type->getTypeName() == deviceManager.getCurrentAudioDeviceType()
                               && deviceName == setup.inputDeviceName;

        if (isCurrent)
        {
            channelNames = current->getInputChannelNames();
        }
        else
        {
            const auto outputName = type->hasSeparateInputsAndOutputs() ? juce::String() : deviceName;
            std::unique_ptr<juce::AudioIODevice> probe (type->createDevice (outputName, deviceName));

            if (probe != nullptr)
                channelNames = probe->getInputChannelNames();
        }

        for (int i = 0; i < channelNames.size(); ++i)
            channelBox.addItem (juce::String (i + 1) + ": " + channelNames[i], i + 1);

        const int currentChannel = isCurrent ? setup.inputChannels.findNextSetBit (0) : -1;
        channelBox.setSelectedId (currentChannel >= 0 ? currentChannel + 1 : 1, juce::dontSendNotification);
    }

    void SettingsDialog::refreshOutputDevices()
    {
        outputDeviceBox.clear (juce::dontSendNotification);

        auto* type = selectedType();

        if (type == nullptr)
        {
            outputDeviceBox.setEnabled (false);
            refreshOutputChannels();
            return;
        }

        if (! type->hasSeparateInputsAndOutputs())
        {
            // 입출력이 한 장치인 드라이버: 입력 장치를 그대로 보여주고 고를 수 없게 한다
            const auto inputName = deviceBox.getSelectedId() == noDeviceId ? juce::String() : deviceBox.getText();

            if (inputName.isNotEmpty())
            {
                outputDeviceBox.addItem (inputName, 1);
                outputDeviceBox.setSelectedId (1, juce::dontSendNotification);
            }

            outputDeviceBox.setEnabled (false);
            refreshOutputChannels();
            return;
        }

        // 장치 목록은 refreshInputDevices() 에서 이미 scanForDevices() 로 갱신됨
        const auto names = type->getDeviceNames (false);

        const auto setup = deviceManager.getAudioDeviceSetup();
        const bool isCurrentType = type->getTypeName() == deviceManager.getCurrentAudioDeviceType();

        // "(none)" 은 넣지 않는다: 출력이 없으면 소리가 안 나므로
        for (int i = 0; i < names.size(); ++i)
        {
            outputDeviceBox.addItem (names[i], i + 1);

            if (isCurrentType && names[i] == setup.outputDeviceName)
                outputDeviceBox.setSelectedId (i + 1, juce::dontSendNotification);
        }

        if (outputDeviceBox.getSelectedId() == 0 && ! names.isEmpty())
            outputDeviceBox.setSelectedId (juce::jmax (0, type->getDefaultDeviceIndex (false)) + 1,
                                           juce::dontSendNotification);

        outputDeviceBox.setEnabled (! names.isEmpty());
        refreshOutputChannels();
    }

    void SettingsDialog::refreshOutputChannels()
    {
        outputChannelBox.clear (juce::dontSendNotification);

        auto* type = selectedType();
        const auto deviceName = outputDeviceBox.getText();

        if (type == nullptr || deviceName.isEmpty())
        {
            outputChannelBox.setEnabled (false);
            return;
        }

        outputChannelBox.setEnabled (true);

        // 지금 열려 있는 장치면 그대로 쓰고, 아니면 열지 않은 임시 장치 객체에서 채널 이름만 얻는다
        juce::StringArray channelNames;
        auto setup = deviceManager.getAudioDeviceSetup();
        auto* current = deviceManager.getCurrentAudioDevice();
        const bool isCurrent = current != nullptr
                               && type->getTypeName() == deviceManager.getCurrentAudioDeviceType()
                               && deviceName == setup.outputDeviceName;

        if (isCurrent)
        {
            channelNames = current->getOutputChannelNames();
        }
        else
        {
            const auto inputName = type->hasSeparateInputsAndOutputs() ? juce::String() : deviceName;
            std::unique_ptr<juce::AudioIODevice> probe (type->createDevice (deviceName, inputName));

            if (probe != nullptr)
                channelNames = probe->getOutputChannelNames();
        }

        // 엔진은 모노 결과를 열린 채널 전부에 복사하므로, 스테레오 쌍을 먼저 보이고 그다음 단일 채널
        for (int i = 0; i + 1 < channelNames.size(); i += 2)
            outputChannelBox.addItem (juce::String (i + 1) + "+" + juce::String (i + 2) + ": "
                                          + channelNames[i] + " / " + channelNames[i + 1],
                                      pairIdBase + i);

        for (int i = 0; i < channelNames.size(); ++i)
            outputChannelBox.addItem (juce::String (i + 1) + ": " + channelNames[i], i + 1);

        // 기본 선택: 지금 열린 장치면 켜진 채널에 맞는 항목, 아니면 "1+2" (채널이 하나뿐이면 "1")
        int selectedId = 0;

        if (isCurrent)
        {
            const int first = setup.outputChannels.findNextSetBit (0);
            const int count = setup.outputChannels.countNumberOfSetBits();

            if (count == 2 && first >= 0 && first % 2 == 0 && setup.outputChannels[first + 1])
                selectedId = pairIdBase + first;
            else if (count == 1 && first >= 0)
                selectedId = first + 1;
        }

        if (selectedId == 0 || outputChannelBox.indexOfItemId (selectedId) < 0)
            selectedId = channelNames.size() >= 2 ? pairIdBase : 1;

        outputChannelBox.setSelectedId (selectedId, juce::dontSendNotification);
    }

    //==============================================================================
    void SettingsDialog::save()
    {
        auto* type = selectedType();

        if (type == nullptr)
        {
            close (false);
            return;
        }

        if (deviceManager.getCurrentAudioDeviceType() != type->getTypeName())
            deviceManager.setCurrentAudioDeviceType (type->getTypeName(), true);

        auto setup = deviceManager.getAudioDeviceSetup();
        const auto inputName = deviceBox.getSelectedId() == noDeviceId ? juce::String() : deviceBox.getText();

        setup.inputDeviceName = inputName;

        const auto outputName = outputDeviceBox.getText();   // 비활성/빈 경우 빈 문자열
        setup.outputDeviceName = outputName;

        if (! type->hasSeparateInputsAndOutputs() && inputName.isNotEmpty())
            setup.outputDeviceName = inputName;   // JACK/ASIO 처럼 입출력이 한 장치인 드라이버: 입력 장치가 우선

        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();

        if (inputName.isNotEmpty() && channelBox.getSelectedId() > 0)
            setup.inputChannels.setBit (channelBox.getSelectedId() - 1);

        setup.useDefaultOutputChannels = false;
        setup.outputChannels.clear();

        if (const int outId = outputChannelBox.getSelectedId(); setup.outputDeviceName.isNotEmpty() && outId > 0)
        {
            if (outId >= pairIdBase)
            {
                setup.outputChannels.setBit (outId - pairIdBase);       // 스테레오 쌍
                setup.outputChannels.setBit (outId - pairIdBase + 1);
            }
            else
            {
                setup.outputChannels.setBit (outId - 1);                 // 단일 채널
            }
        }

        const auto error = deviceManager.setAudioDeviceSetup (setup, true);

        if (error.isNotEmpty())
        {
            // 실패하면 창을 닫지 않고 다시 고르게 한다
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Could not open audio device", error);
            return;
        }

        if (auto xml = deviceManager.createStateXml())
            xml->writeTo (paths::deviceSettingsFile());

        close (true);
    }

    void SettingsDialog::close (bool saved)
    {
        auto callback = onClosed;

        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState (saved ? 1 : 0);   // launchAsync 창은 이후 스스로 삭제됨

        if (callback)
            callback (saved);
    }

    //==============================================================================
    void SettingsDialog::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void SettingsDialog::resized()
    {
        auto area = getLocalBounds().reduced (16);

        auto row = [&area] { auto r = area.removeFromTop (28); area.removeFromTop (8); return r; };

        for (auto [label, box] : { std::pair { &typeLabel, &typeBox },
                                   std::pair { &deviceLabel, &deviceBox },
                                   std::pair { &channelLabel, &channelBox },
                                   std::pair { &outputDeviceLabel, &outputDeviceBox },
                                   std::pair { &outputChannelLabel, &outputChannelBox } })
        {
            auto r = row();
            label->setBounds (r.removeFromLeft (110));
            r.removeFromLeft (8);
            box->setBounds (r);
        }

        auto buttons = area.removeFromBottom (28);
        cancelButton.setBounds (buttons.removeFromRight (90));
        buttons.removeFromRight (8);
        saveButton.setBounds (buttons.removeFromRight (90));
    }
}
