#include "ParamBar.h"

#include "ValueSteps.h"

namespace rig
{
    namespace
    {
        // channelBox 항목 id: 단일 채널은 1 + 채널(0부터), 출력 스테레오 쌍은 pairIdBase + 첫 채널(0부터)
        constexpr int pairIdBase = 1000;

        const juce::Colour errorColour (juce::Colours::red.brighter (0.2f));
        const juce::Colour warningColour (juce::Colours::yellow);

        // 합류 레벨: 오디오 테이퍼. 위치 p 에서 dB = 10 * log2 (p) → 절반마다 -10 dB, -3 dB ≈ 81 %,
        // p ≤ 2^-6 (= -60 dB) 는 -inf (mergeMinDb 에 붙임). 0.1 dB 단위
        juce::NormalisableRange<double> levelRange()
        {
            return { RigModel::mergeMinDb, RigModel::mergeMaxDb,
                     [] (double start, double end, double p)        // 0..1 -> dB
                     {
                         return p <= 0.0 ? start : juce::jlimit (start, end, 10.0 * std::log2 (p));
                     },
                     [] (double start, double end, double db)       // dB -> 0..1
                     {
                         return db <= start ? 0.0 : juce::jlimit (0.0, 1.0, std::pow (2.0, (db - end) / 10.0));
                     },
                     [] (double start, double end, double db)       // 0.1 dB 단위
                     {
                         return juce::jlimit (start, end, steps::roundTo (db, steps::dbDecimals));
                     } };
        }

        constexpr float levelTextSize = 14.0f;   // 합류 화면의 갈래 이름(굵게) / dB 글자

        juce::String levelText (double db)
        {
            return db <= RigModel::mergeMinDb ? juce::String ("-inf dB") : juce::String (db, 1) + " dB";
        }
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

        errorLabel.setColour (juce::Label::textColourId, errorColour);
        warningLabel.setColour (juce::Label::textColourId, warningColour);

        for (auto* label : { &errorLabel, &warningLabel })
        {
            label->setJustificationType (juce::Justification::topRight);
            label->setBorderSize ({});
            label->setMinimumHorizontalScale (1.0f);   // 찌그러뜨리지 않고 말줄임
            addChildComponent (label);
        }

        for (int b = 0; b < 2; ++b)
        {
            branchPower[(size_t) b].setClickingTogglesState (true);
            branchPower[(size_t) b].onClick = [this, b]
            {
                model.setBranchOn (model.getSelectedParallelId(), b, branchPower[(size_t) b].getToggleState());
            };

            branchLabel[(size_t) b].setText (b == 0 ? "Upper" : "Lower", juce::dontSendNotification);
            branchLabel[(size_t) b].setFont (juce::FontOptions (levelTextSize, juce::Font::bold));
            branchLabel[(size_t) b].setJustificationType (juce::Justification::centredLeft);
            branchLabel[(size_t) b].setBorderSize ({});   // 글자 끝 = 홈 끝 (Label 기본 여백 없이)

            auto& slider = levelSlider[(size_t) b];
            slider.setLookAndFeel (&faderLnf);
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setNormalisableRange (levelRange());
            slider.setScrollWheelEnabled (false);   // 휠은 드래그 끝(저장)이 오지 않으므로 막는다
            slider.setSliderSnapsToMousePosition (false);   // 클릭만으로는 값이 그대로, 끈 거리만큼 지금 값에서 움직인다 (잘못 눌러 레벨이 튀지 않게)
            slider.setDoubleClickReturnValue (true, RigModel::mergeDefaultDb);   // JUCE 가 드래그 시작/끝도 보내 저장된다

            // 드래그 중에는 소리만, 드래그 끝에 저장
            slider.onValueChange = [this, b]
            {
                model.setBranchLevel (model.getSelectedParallelId(), b, (float) levelSlider[(size_t) b].getValue(), false);
                levelValue[(size_t) b].setText (levelText (levelSlider[(size_t) b].getValue()), juce::dontSendNotification);
            };
            slider.onDragEnd = [this, b]
            {
                model.setBranchLevel (model.getSelectedParallelId(), b, (float) levelSlider[(size_t) b].getValue(), true);
            };

            levelValue[(size_t) b].setFont (juce::FontOptions (levelTextSize));
            levelValue[(size_t) b].setJustificationType (juce::Justification::centredRight);
            levelValue[(size_t) b].setBorderSize ({});

            for (auto* c : std::initializer_list<juce::Component*> { &branchPower[(size_t) b], &branchLabel[(size_t) b],
                                                                     &slider, &levelValue[(size_t) b] })
                addChildComponent (c);
        }

        levelResetButton.onClick = [this] { model.resetLevels (model.getSelectedParallelId()); };
        addChildComponent (levelResetButton);

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
        const auto* parallel = model.findParallel (model.getSelectedParallelId());
        const bool isMerge = parallel != nullptr;

        powerButton.setVisible (! isEndpoint && ! isMerge);
        channelLabel.setVisible (isEndpoint);
        channelBox.setVisible (isEndpoint);

        if (panel != nullptr)
            panel->setVisible (! isEndpoint && ! isMerge);   // IN / OUT, 합류를 고르면 숨긴다

        for (int b = 0; b < 2; ++b)
            for (auto* c : std::initializer_list<juce::Component*> { &branchPower[(size_t) b], &branchLabel[(size_t) b],
                                                                     &levelSlider[(size_t) b], &levelValue[(size_t) b] })
                c->setVisible (isMerge);

        levelResetButton.setVisible (isMerge);

        if (isMerge)
        {
            updateMerge (*parallel);
            setMessages ({}, {});
            return;
        }

        if (isEndpoint)
        {
            fillChannelBox (endpoint);
            setMessages (model.isChannelMissing (endpoint, deviceManager.getCurrentAudioDevice())
                             ? "Channel not available on the current device" : juce::String(),
                         {});
            resized();
            return;
        }

        const auto* slot = model.getSelectedSlot();

        // 미지정 블록은 지정할 때까지 끌 수 없다
        powerButton.setEnabled (slot != nullptr && slot->type != BlockType::None);
        powerButton.setToggleState (slot == nullptr || model.isOn (*slot), juce::dontSendNotification);

        // 블록 종류가 바뀌면 패널을 다시 만들고, 같으면 값만 갱신 (패치가 바뀐 경우 포함)
        const auto type = slot != nullptr ? slot->type : BlockType::None;

        if (type != panelType || (panel == nullptr && type != BlockType::None))
        {
            panelType = type;
            panel = createParamPanel (type, model);   // 이전 패널은 여기서 지워진다

            if (panel != nullptr)
            {
                addAndMakeVisible (*panel);
                resized();
            }
        }
        else if (panel != nullptr)
        {
            panel->refresh();
        }

        // 모델 레이트와 장치 레이트가 다르면 소리가 틀어진다 (리샘플 미구현)
        juce::String warning;
        const auto deviceRate = model.getDeviceSampleRate();

        if (slot != nullptr && slot->dsp != nullptr)
            if (const auto modelRate = slot->dsp->expectedSampleRate(); modelRate > 0.0 && std::abs (modelRate - deviceRate) > 0.5)
                warning = "Sample rate mismatch:\nmodel " + juce::String (juce::roundToInt (modelRate)) + " Hz\ndevice "
                        + juce::String (juce::roundToInt (deviceRate)) + " Hz";

        setMessages (slot != nullptr ? slot->error : juce::String(), warning);
        resized();
    }

    void ParamBar::updateMerge (const RigModel::Parallel& parallel)
    {
        for (int b = 0; b < 2; ++b)
        {
            const auto level = model.getBranchLevel (parallel, b);
            branchPower[(size_t) b].setToggleState (model.isBranchOn (parallel, b), juce::dontSendNotification);
            levelSlider[(size_t) b].setValue (level, juce::dontSendNotification);
            levelValue[(size_t) b].setText (levelText (level), juce::dontSendNotification);
        }
    }

    // 오류(빨강)와 경고(노랑)는 동시에 보일 수 있다. 하나뿐이면 그 줄만 세로 가운데에
    void ParamBar::setMessages (const juce::String& error, const juce::String& warning)
    {
        errorLabel.setText (error, juce::dontSendNotification);
        warningLabel.setText (warning, juce::dontSendNotification);
        errorLabel.setVisible (error.isNotEmpty());
        warningLabel.setVisible (warning.isNotEmpty());
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

    void ParamBar::FaderLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                                       float, float, juce::Slider::SliderStyle, juce::Slider&)
    {
        // x ~ x + width 는 JUCE 가 손잡이 반지름(capWidth / 2)만큼 들인 범위. 값 0 / 1 = 홈 양 끝
        const float cy = (float) y + (float) height * 0.5f;
        constexpr float grooveHeight = 4.0f;

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle ({ (float) x, cy - grooveHeight * 0.5f, (float) width, grooveHeight }, grooveHeight * 0.5f);

        // 손잡이: 짙은 회색 사각 + 밝은 테두리 + 가운데 흰 선 (움직이는 방향에 수직)
        const auto cap = juce::Rectangle<float> ((float) capWidth, (float) juce::jmin (capHeight, height)).withCentre ({ sliderPos, cy });
        g.setColour (juce::Colour (0xff3c3c3c));
        g.fillRoundedRectangle (cap, 2.0f);
        g.setColour (juce::Colours::grey);
        g.drawRoundedRectangle (cap.reduced (0.5f), 2.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.drawLine (sliderPos, cap.getY() + 3.0f, sliderPos, cap.getBottom() - 3.0f, 2.0f);
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
        // 오른쪽 끝 = 전원 버튼. 나머지(왼쪽)는 블록 종류별 파라미터 패널
        auto area = getLocalBounds().reduced (12, 0);
        const int size = juce::jmin (50, getHeight() - 8);   // 전원 버튼 50 px (예전 40 px 의 1.25 배)
        powerButton.setBounds (area.removeFromRight (size).withSizeKeepingCentre (size, size));

        if (panel != nullptr)
        {
            area.removeFromRight (12);
            panel->setBounds (area);
        }

        // IN / OUT 선택 시: 왼쪽부터 [이름표 110] [채널 목록 320], 높이 28 (설정창 줄과 같은 크기)
        auto row = getLocalBounds().reduced (12, 0).withSizeKeepingCentre (getWidth() - 24, 28);
        channelLabel.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (8);
        channelBox.setBounds (row.removeFromLeft (320));

        // 오류 / 경고 문구: 전원 버튼 자리 왼쪽에 오른쪽 정렬, 윗변 = 전원 버튼 윗변 (IN / OUT 일 때도 같은 자리).
        // 오류(빨강) 아래 경고(노랑), 글자의 줄바꿈(\n)만큼 줄 수
        constexpr int messageWidth = 420, messageLineHeight = 18;
        auto messages = area.withTrimmedRight (12).removeFromRight (messageWidth).withTop (powerButton.getY());

        for (auto* label : { &errorLabel, &warningLabel })
        {
            const int lines = label->isVisible() ? label->getText().length() - label->getText().removeCharacters ("\n").length() + 1 : 0;
            label->setBounds (messages.removeFromTop (messageLineHeight * lines));
        }

        // 합류 선택 시: [전원 50] 8 [칸 260: 윗줄 = 이름(왼쪽) + dB(오른쪽) 18, 아랫줄 = 슬라이더 24]  간격 64  [아래 갈래] ... [Level Reset 64 x 50]
        constexpr int columnWidth = 260, textHeight = 18, sliderHeight = FaderLookAndFeel::capHeight, branchGap = 64;
        auto merge = getLocalBounds().reduced (12, 0);
        // Level Reset: 전원 버튼과 같은 높이, 좁게 (글자 "Level\nReset" 두 줄)
        constexpr int resetWidth = 64;
        levelResetButton.setBounds (merge.removeFromRight (resetWidth).withSizeKeepingCentre (resetWidth, size));

        for (size_t b = 0; b < 2; ++b)
        {
            branchPower[b].setBounds (merge.removeFromLeft (size).withSizeKeepingCentre (size, size));
            merge.removeFromLeft (8);

            auto column = merge.removeFromLeft (columnWidth).withSizeKeepingCentre (columnWidth, textHeight + sliderHeight);
            auto text = column.removeFromTop (textHeight).reduced (FaderLookAndFeel::capWidth / 2, 0);   // 홈 양 끝에 맞춘다
            branchLabel[b].setBounds (text.removeFromLeft (columnWidth / 2));
            levelValue[b].setBounds (text);
            levelSlider[b].setBounds (column);
            merge.removeFromLeft (branchGap);
        }
    }
}
