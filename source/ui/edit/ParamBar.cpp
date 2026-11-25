#include "ParamBar.h"

#include "ValueSteps.h"

namespace rig
{
    namespace
    {
        // 합류 레벨: 오디오 테이퍼. 위치 p 에서 dB = 12 * log2 (p) → 절반마다 -12 dB, -3 dB ≈ 84 %,
        // p ≤ 2^-5 (= -60 dB) 는 -inf (mergeMinDb 에 붙임). 0.1 dB 단위
        constexpr double levelDbPerHalf = 12.0;

        juce::NormalisableRange<double> levelRange()
        {
            return { RigModel::mergeMinDb, RigModel::mergeMaxDb,
                     [] (double start, double end, double p)        // 0..1 -> dB
                     {
                         return p <= 0.0 ? start : juce::jlimit (start, end, levelDbPerHalf * std::log2 (p));
                     },
                     [] (double start, double end, double db)       // dB -> 0..1
                     {
                         return db <= start ? 0.0 : juce::jlimit (0.0, 1.0, std::pow (2.0, (db - end) / levelDbPerHalf));
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
        powerButton.onClick = [this] { model.setOn (model.getSelectedSlotId(), powerButton.getToggleState()); };
        addAndMakeVisible (powerButton);

        channelLabel.setJustificationType (juce::Justification::centredRight);
        addChildComponent (channelLabel);
        channelBox.onChange = [this] { channelChosen(); };
        addChildComponent (channelBox);


        for (int b = 0; b < 2; ++b)
        {
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

        paramResetButton.onClick = [this] { model.resetBlockParams (model.getSelectedSlotId()); };
        addChildComponent (paramResetButton);

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
        paramResetButton.setVisible (false);   // 블록이면 아래에서 패널이 있을 때만

        if (isMerge)
        {
            updateMerge (*parallel);
            return;
        }

        if (isEndpoint)
        {
            fillChannelBox (endpoint);
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

        paramResetButton.setVisible (panel != nullptr);
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

    void ParamBar::fillChannelBox (RigModel::Endpoint endpoint)
    {
        const bool isInput = endpoint == RigModel::Endpoint::input;
        const auto& routing = model.getRouting();

        channelLabel.setText (isInput ? "Input channel" : "Output channel", juce::dontSendNotification);

        juce::StringArray names;

        if (auto* device = deviceManager.getCurrentAudioDevice())
            names = isInput ? device->getInputChannelNames() : device->getOutputChannelNames();

        // 패치의 다른 곳이 쓰는 채널은 보이되 고를 수 없다. 장치에 없는 채널이면 IN / OUT 노드에는 빗금
        fillChannelList (channelBox, names, isInput,
                         isInput ? routing.input : routing.output, routing.stereoOutput,
                         isInput ? model.getUsedInputChannels (endpoint) : model.getUsedOutputChannels (endpoint));
    }

    void ParamBar::channelChosen()
    {
        const auto chosen = chosenChannel (channelBox);

        if (! chosen)
            return;

        if (model.getSelectedEndpoint() == RigModel::Endpoint::input)
            model.setInputChannel (chosen->first);
        else
            model.setOutputChannel (chosen->first, chosen->second);
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

    void ParamBar::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.05f));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    }

    void ParamBar::resized()
    {
        // 14칸 격자 (bottomBarCell, 아래 씬 바와 같은 격자). 바깥 여백 6 px
        //   블록 : [Param Reset 0] [ 1 ] [패널: 종류별 칸 2 ~ 12] [전원 13]
        //   IN / OUT : [채널 0 ~ 5]
        //   합류 : [Level Reset 0] [ 1 ] [위 갈래 2 ~ 5] [ 6 ] [아래 갈래 7 ~ 10] [ 11 ~ 13 ]
        // 리셋은 왼쪽 끝 (리셋은 왼쪽). 중간 리셋이라 높이 = 큰 리셋(씬 바 Scene Reset, 같은 0번 칸)의 2/3, 세로 가운데
        const auto grid = getLocalBounds().reduced (bottomBarGap);
        const auto bigReset = bottomBarCell (grid, 0);
        const auto resetArea = bigReset.withSizeKeepingCentre (bigReset.getWidth(), bigReset.getHeight() * 2 / 3);
        paramResetButton.setBounds (resetArea);
        levelResetButton.setBounds (resetArea);

        // 블록 전원: 마지막 칸 가운데, 크기는 그대로
        const int size = juce::jmin (PowerButton::size, getHeight() - 8);
        powerButton.setBounds (bottomBarCell (grid, bottomBarCells - 1).withSizeKeepingCentre (size, size));

        if (panel != nullptr)
            panel->setBounds (grid);   // 패널이 격자 칸을 직접 고른다 (bottomBarCell)

        // IN / OUT 선택 시: 0 ~ 5 칸에 [이름표 channelLabelWidth] 8 [채널 목록 = 나머지], 높이 28 (Live 아래 줄 채널과 같다)
        auto row = bottomBarCell (grid, 0, channelCells);
        row = row.withSizeKeepingCentre (row.getWidth(), channelRowHeight);
        channelLabel.setBounds (row.removeFromLeft (channelLabelWidth));
        row.removeFromLeft (channelLabelGap);
        channelBox.setBounds (row);

        // 합류 선택 시: 갈래마다 4칸 [빨간 전원 40] 8 [나머지: 윗줄 = 이름(왼쪽) + dB(오른쪽) 18, 아랫줄 = 슬라이더 24]
        constexpr int branchCells = 4, textHeight = 18, sliderHeight = FaderLookAndFeel::capHeight;
        const int subSize = juce::jmin (SubPowerButton::size, getHeight() - 8);

        for (size_t b = 0; b < 2; ++b)
        {
            auto merge = bottomBarCell (grid, b == 0 ? 2 : 7, branchCells);
            branchPower[b].setBounds (merge.removeFromLeft (subSize).withSizeKeepingCentre (subSize, subSize));
            merge.removeFromLeft (8);

            auto column = merge.withSizeKeepingCentre (merge.getWidth(), textHeight + sliderHeight);
            auto text = column.removeFromTop (textHeight).reduced (FaderLookAndFeel::capWidth / 2, 0);   // 홈 양 끝에 맞춘다
            branchLabel[b].setBounds (text.removeFromLeft (text.getWidth() / 2));
            levelValue[b].setBounds (text);
            levelSlider[b].setBounds (column);
        }
    }
}
