#include "ParamBar.h"

namespace rig
{
    namespace
    {
        // channelBox 항목 id: 단일 채널은 1 + 채널(0부터), 출력 스테레오 쌍은 pairIdBase + 첫 채널(0부터)
        constexpr int pairIdBase = 1000;
    }

    ParamBar::ParamBar (RigModel& m, juce::AudioDeviceManager& dm) : model (m), deviceManager (dm)
    {
        powerButton.setClickingTogglesState (true);
        powerButton.onClick = [this] { model.setOn (model.getSelectedSlotId(), powerButton.getToggleState()); };
        addAndMakeVisible (powerButton);

        channelLabel.setJustificationType (juce::Justification::centredRight);
        addChildComponent (channelLabel);
        channelBox.onChange = [this] { channelChosen(); };
        addChildComponent (channelBox);

        model.addChangeListener (this);
        deviceManager.addChangeListener (this);
        updateFromModel();
    }

    ParamBar::~ParamBar()
    {
        deviceManager.removeChangeListener (this);
        model.removeChangeListener (this);
    }

    void ParamBar::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        updateFromModel();
    }

    void ParamBar::updateFromModel()
    {
        const auto endpoint = model.getSelectedEndpoint();
        const bool isEndpoint = endpoint != RigModel::Endpoint::none;

        powerButton.setVisible (! isEndpoint);
        channelLabel.setVisible (isEndpoint);
        channelBox.setVisible (isEndpoint);

        if (isEndpoint)
        {
            fillChannelBox (endpoint);
            return;
        }

        const auto* slot = model.getSelectedSlot();

        // 미지정 블록은 지정할 때까지 끌 수 없다
        powerButton.setEnabled (slot != nullptr && slot->type != BlockType::None);
        powerButton.setToggleState (slot == nullptr || model.isOn (*slot), juce::dontSendNotification);
    }

    void ParamBar::fillChannelBox (RigModel::Endpoint endpoint)
    {
        const bool isInput = endpoint == RigModel::Endpoint::input;
        const auto& routing = model.getRouting();

        channelLabel.setText (isInput ? "Input channel" : "Output channel", juce::dontSendNotification);
        channelBox.clear (juce::dontSendNotification);

        juce::StringArray names;

        if (auto* device = deviceManager.getCurrentAudioDevice())
            names = isInput ? device->getInputChannelNames() : device->getOutputChannelNames();

        // 패치의 다른 곳이 쓰는 채널은 보이되 고를 수 없다
        const auto used = isInput ? model.getUsedInputChannels (endpoint) : model.getUsedOutputChannels (endpoint);

        if (isInput)
        {
            for (int i = 0; i < names.size(); ++i)
            {
                channelBox.addItem (channelText (i, 1) + ": " + names[i], i + 1);
                channelBox.setItemEnabled (i + 1, ! used[i]);
            }
        }
        else
        {
            // 엔진은 모노 결과를 고른 채널에 복사하므로, 스테레오 쌍을 먼저 보이고 그다음 단일 채널
            for (int i = 0; i + 1 < names.size(); i += 2)
            {
                channelBox.addItem (channelText (i, 2) + ": " + names[i] + " / " + names[i + 1], pairIdBase + i);
                channelBox.setItemEnabled (pairIdBase + i, ! used[i] && ! used[i + 1]);
            }

            for (int i = 0; i < names.size(); ++i)
            {
                channelBox.addItem (channelText (i, 1) + ": " + names[i], i + 1);
                channelBox.setItemEnabled (i + 1, ! used[i]);
            }
        }

        // 장치에 없는 채널이면 아무것도 선택하지 않고 패치에 저장된 번호만 보인다 (IN / OUT 노드에는 빗금)
        const int selectedId = isInput ? routing.input + 1
                                       : (routing.stereoOutput ? pairIdBase + routing.output : routing.output + 1);
        channelBox.setTextWhenNothingSelected (isInput ? channelText (routing.input, 1)
                                                       : channelText (routing.output, routing.stereoOutput ? 2 : 1));
        channelBox.setSelectedId (channelBox.indexOfItemId (selectedId) >= 0 ? selectedId : 0, juce::dontSendNotification);
        channelBox.setEnabled (! names.isEmpty());
    }

    void ParamBar::channelChosen()
    {
        const int id = channelBox.getSelectedId();

        if (id <= 0)
            return;

        if (model.getSelectedEndpoint() == RigModel::Endpoint::input)
            model.setInputChannel (id - 1);
        else if (id >= pairIdBase)
            model.setOutputChannel (id - pairIdBase, true);
        else
            model.setOutputChannel (id - 1, false);
    }

    void ParamBar::PowerButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.5f);
        const bool on = getToggleState();

        auto colour = on ? juce::Colours::limegreen : juce::Colours::grey.darker (0.3f);
        if (! isEnabled())
            colour = juce::Colours::grey.darker (0.8f);
        else if (down)
            colour = colour.darker (0.3f);
        else if (highlighted)
            colour = colour.brighter (0.3f);

        g.setColour (juce::Colours::black.withAlpha (on && isEnabled() ? 0.35f : 0.2f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (colour.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        // 전원 기호: 위가 열린 원 + 세로선
        const auto icon = bounds.reduced (bounds.getHeight() * 0.25f);
        const auto centre = icon.getCentre();
        const float radius = icon.getHeight() * 0.5f;
        const float gapAngle = juce::MathConstants<float>::pi / 5.0f;

        juce::Path path;
        path.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                            gapAngle, juce::MathConstants<float>::twoPi - gapAngle, true);
        path.startNewSubPath (centre.x, icon.getY() - radius * 0.15f);
        path.lineTo (centre.x, centre.y);

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void ParamBar::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.05f));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    }

    void ParamBar::resized()
    {
        // 오른쪽 끝 = 전원 버튼. 나머지(왼쪽)는 나중에 파라미터 노브 자리
        auto area = getLocalBounds().reduced (12, 0);
        const int size = juce::jmin (50, getHeight() - 8);   // 전원 버튼 50 px (예전 40 px 의 1.25 배)
        powerButton.setBounds (area.removeFromRight (size).withSizeKeepingCentre (size, size));

        // IN / OUT 선택 시: 왼쪽부터 [이름표 110] [채널 목록 320], 높이 28 (설정창 줄과 같은 크기)
        auto row = getLocalBounds().reduced (12, 0).withSizeKeepingCentre (getWidth() - 24, 28);
        channelLabel.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (8);
        channelBox.setBounds (row.removeFromLeft (320));
    }
}
