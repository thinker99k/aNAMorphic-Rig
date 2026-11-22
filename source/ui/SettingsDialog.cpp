#include "SettingsDialog.h"

#include "AppPaths.h"

namespace rig
{
    namespace
    {
        // DialogWindow::LaunchOptions::create 가 만드는 창과 같지만, 제목 표시줄 끌기도 왼쪽 클릭만 (마우스 규칙, Widgets.h)
        struct SettingsWindow final : public LeftClickOnly<juce::DialogWindow>
        {
            SettingsWindow (std::unique_ptr<SettingsDialog> content, juce::Component* parent)
                : LeftClickOnly ("Settings",
                                 parent != nullptr ? parent->getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId)
                                                   : juce::Colours::darkgrey,
                                 false,   // ESC 로 빠져나가지 못하게
                                 true,
                                 parent != nullptr ? juce::Component::getApproximateScaleFactorForComponent (parent) : 1.0f)
            {
                setContentOwned (content.release(), true);
                centreAroundComponent (parent, getWidth(), getHeight());
                setResizable (false, false);
                setUsingNativeTitleBar (false);   // JUCE 타이틀바를 써야 닫기 버튼을 없앨 수 있음
            }

            void closeButtonPressed() override { setVisible (false); }
        };
    }

    namespace
    {
        constexpr int noDeviceId = 1;   // deviceBox 의 "(none)" 항목 id
    }

    SettingsDialog::SettingsDialog (juce::AudioDeviceManager& dm) : deviceManager (dm)
    {
        for (auto* l : { &typeLabel, &deviceLabel, &outputDeviceLabel, &rateLabel, &bufferLabel })
        {
            l->setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (l);
        }

        addAndMakeVisible (typeBox);
        addAndMakeVisible (deviceBox);
        addAndMakeVisible (outputDeviceBox);

        addAndMakeVisible (rateBox);
        addAndMakeVisible (bufferBox);

        typeBox.onChange = [this]
        {
            refreshInputDevices();
            refreshOutputDevices();
            refreshFormatOptions();
        };
        outputDeviceBox.onChange = [this] { refreshFormatOptions(); };
        deviceBox.onChange = [this]
        {
            // 입출력이 한 장치인 드라이버(JACK/ASIO 등)는 출력 장치가 입력 장치를 따라간다
            if (auto* type = selectedType(); type != nullptr && ! type->hasSeparateInputsAndOutputs())
                refreshOutputDevices();
            refreshFormatOptions();
        };

        saveButton.onClick   = [this] { save(); };
        cancelButton.onClick = [this] { close (false); };
        addAndMakeVisible (saveButton);
        addAndMakeVisible (cancelButton);

        refreshDeviceTypes();
        refreshFormatOptions();
        setSize (500, 224);   // 16 + 장치 줄 3개·샘플레이트/버퍼 줄 1개 (28 + 8) + 여백 20 + 버튼 28 + 16
    }

    void SettingsDialog::show (juce::AudioDeviceManager& dm,
                               juce::Component* parent,
                               std::function<void (bool)> onClosed)
    {
        auto content = std::make_unique<SettingsDialog> (dm);
        content->onClosed = std::move (onClosed);

        auto* window = new SettingsWindow (std::move (content), parent);

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

            if (isCurrentType && setup.inputDeviceName.isEmpty())
                deviceBox.setSelectedId (noDeviceId, juce::dontSendNotification);

            if (deviceBox.getSelectedId() == 0)
                deviceBox.setSelectedId (names.isEmpty() ? noDeviceId
                                                         : noDeviceId + 1 + type->getDefaultDeviceIndex (true),
                                         juce::dontSendNotification);
        }
    }

    void SettingsDialog::refreshOutputDevices()
    {
        outputDeviceBox.clear (juce::dontSendNotification);

        auto* type = selectedType();

        if (type == nullptr)
        {
            outputDeviceBox.setEnabled (false);
            return;
        }

        if (! type->hasSeparateInputsAndOutputs())
        {
            // 입출력이 한 장치인 드라이버: 입력 장치를 그대로 보여주고 고를 수 없게 한다
            const auto inputName = selectedInputName();

            if (inputName.isNotEmpty())
            {
                outputDeviceBox.addItem (inputName, 1);
                outputDeviceBox.setSelectedId (1, juce::dontSendNotification);
            }

            outputDeviceBox.setEnabled (false);
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
    }

    //==============================================================================
    juce::String SettingsDialog::selectedInputName() const
    {
        return deviceBox.getSelectedId() > noDeviceId ? deviceBox.getText() : juce::String();
    }

    void SettingsDialog::refreshFormatOptions()
    {
        auto* type = selectedType();
        const auto setup = deviceManager.getAudioDeviceSetup();
        const auto input = selectedInputName();
        const auto output = outputDeviceBox.getText();
        const bool current = type != nullptr && type->getTypeName() == deviceManager.getCurrentAudioDeviceType()
                          && input == setup.inputDeviceName && output == setup.outputDeviceName;
        auto* device = current ? deviceManager.getCurrentAudioDevice() : nullptr;
        std::unique_ptr<juce::AudioIODevice> preview;
        if (device == nullptr && type != nullptr && (input.isNotEmpty() || output.isNotEmpty()))
        {
            preview.reset (type->createDevice (output, input));
            if (preview != nullptr && preview->getLastError().isEmpty())
                device = preview.get();
        }

        sampleRates = device != nullptr ? device->getAvailableSampleRates() : juce::Array<double>();
        bufferSizes = device != nullptr ? device->getAvailableBufferSizes() : juce::Array<int>();

        const auto fill = [] (juce::ComboBox& box, const auto& values, double preferred, auto format)
        {
            box.clear (juce::dontSendNotification);
            box.addItem ("Device default", 1);
            int selected = 1;
            for (int i = 0; i < values.size(); ++i)
            {
                box.addItem (format (values[i]), i + 2);
                if (std::abs (values[i] - preferred) < 0.5)
                    selected = i + 2;
            }
            box.setSelectedId (selected, juce::dontSendNotification);
            box.setEnabled (! values.isEmpty());
        };

        fill (rateBox, sampleRates, current ? setup.sampleRate : 0.0,
              [] (double hz) { return juce::String (juce::roundToInt (hz)); });
        fill (bufferBox, bufferSizes, current ? setup.bufferSize : 0,
              [] (int samples) { return juce::String (samples); });
    }

    void SettingsDialog::save()
    {
        auto* type = selectedType();

        if (type == nullptr)
        {
            close (false);
            return;
        }

        const auto previousType = deviceManager.getCurrentAudioDeviceType();
        const auto previousSetup = deviceManager.getAudioDeviceSetup();
        const bool hadDevice = deviceManager.getCurrentAudioDevice() != nullptr;
        if (previousType != type->getTypeName())
            deviceManager.setCurrentAudioDeviceType (type->getTypeName(), true);

        auto setup = deviceManager.getAudioDeviceSetup();
        const auto inputName = selectedInputName();

        setup.inputDeviceName = inputName;

        const auto outputName = outputDeviceBox.getText();   // 비활성/빈 경우 빈 문자열
        setup.outputDeviceName = outputName;

        if (! type->hasSeparateInputsAndOutputs() && inputName.isNotEmpty())
            setup.outputDeviceName = inputName;   // JACK/ASIO 처럼 입출력이 한 장치인 드라이버: 입력 장치가 우선

        // 채널은 전부 연다 (AudioEngine::openAllChannels 로 초기화했으므로 "기본 채널" = 전부)
        setup.sampleRate = rateBox.getSelectedId() > 1 ? sampleRates[rateBox.getSelectedId() - 2] : 0.0;
        setup.bufferSize = bufferBox.getSelectedId() > 1 ? bufferSizes[bufferBox.getSelectedId() - 2] : 0;
        setup.useDefaultInputChannels = true;
        setup.useDefaultOutputChannels = true;

        const auto error = deviceManager.setAudioDeviceSetup (setup, true);

        if (error.isNotEmpty())
        {
            // 실패하면 창을 닫지 않고 다시 고르게 한다
            if (deviceManager.getCurrentAudioDeviceType() != previousType)
                deviceManager.setCurrentAudioDeviceType (previousType, true);

            auto message = error;
            if (hadDevice)
            {
                const auto restoreError = deviceManager.setAudioDeviceSetup (previousSetup, true);
                if (restoreError.isNotEmpty())
                    message += "\nCould not restore previous settings: " + restoreError;
            }
            else
                deviceManager.closeAudioDevice();

            showAlert (*this, "Could not open audio device", message, { { "OK", 0 } }, true);
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
                                   std::pair { &outputDeviceLabel, &outputDeviceBox } })
        {
            auto r = row();
            label->setBounds (r.removeFromLeft (110));
            r.removeFromLeft (8);
            box->setBounds (r);
        }

        auto formats = row();
        rateLabel.setBounds (formats.removeFromLeft (110));
        formats.removeFromLeft (8);
        const int boxWidth = (formats.getWidth() - 80 - 8 * 2) / 2;   // 버퍼 라벨 80 + 간격 8 두 번을 빼고 두 콤보가 나눠 가짐
        rateBox.setBounds (formats.removeFromLeft (boxWidth));
        formats.removeFromLeft (8);
        bufferLabel.setBounds (formats.removeFromLeft (80));
        formats.removeFromLeft (8);
        bufferBox.setBounds (formats);

        auto buttons = area.removeFromBottom (28);
        cancelButton.setBounds (buttons.removeFromRight (90));
        buttons.removeFromRight (8);
        saveButton.setBounds (buttons.removeFromRight (90));
    }
}
