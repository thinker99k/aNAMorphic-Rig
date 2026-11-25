#include "LiveStatusBar.h"

#include "tuner/TunerButton.h"
#include "ui/live/LiveStyle.h"

namespace rig
{
    namespace
    {
        // 칸: 편집 화면 씬 바와 같은 14칸 격자 (bottomBarCell). Live 는 씬 바 Live 와 같은 칸
        //   [채널 0~5] [빈칸 6] [튜너 스위치 7] [간이 튜너 8~9] [Patch 10] [Stomp 11] [빈칸 12] [Live 13]
        constexpr int tunerCell = 7, stripCell = 8, stripCells = 2, patchCell = 10, stompCell = 11;

        // 채널 두 줄 사이 (줄 모양은 파라미터 바 IN / OUT 과 같다: Widgets.h channelCells 등)
        constexpr int rowGap = 6;

        constexpr float tunerIconSize = 24.0f, tunerIconThickness = 2.0f;
        constexpr float tunerTextGap = 4.0f;   // 아이콘과 "Tuner" 사이
        constexpr float offAlpha = 0.35f;      // 튜너가 꺼져 있을 때 간이 튜너 칸
    }

    LiveStatusBar::LiveStatusBar (RigModel& m, AudioEngine& e, juce::AudioDeviceManager& dm)
        : model (m), engine (e), deviceManager (dm)
    {
        for (auto* label : { &inputLabel, &outputLabel })
        {
            label->setJustificationType (juce::Justification::centredRight);
            label->setColour (juce::Label::textColourId, live::textLight);
            addAndMakeVisible (label);
        }

        inputBox.onChange = [this]
        {
            if (const auto chosen = chosenChannel (inputBox))
            {
                auto r = liveRouting();
                r.input = chosen->first;
                model.setLiveRouting (r);
            }
        };

        outputBox.onChange = [this]
        {
            if (const auto chosen = chosenChannel (outputBox))
            {
                auto r = liveRouting();
                r.output = chosen->first;
                r.stereoOutput = chosen->second;
                model.setLiveRouting (r);
            }
        };

        addAndMakeVisible (inputBox);
        addAndMakeVisible (outputBox);
        tunerStrip.setAlpha (offAlpha);
        addAndMakeVisible (tunerStrip);

        model.addChangeListener (this);
        deviceManager.addChangeListener (this);
        updateChannels();
    }

    LiveStatusBar::~LiveStatusBar()
    {
        setTunerOn (false);
        deviceManager.removeChangeListener (this);
        model.removeChangeListener (this);
    }

    void LiveStatusBar::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        updateChannels();
    }

    void LiveStatusBar::updateChannels()
    {
        juce::StringArray inputs, outputs;

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            inputs = device->getInputChannelNames();
            outputs = device->getOutputChannelNames();
        }

        const auto r = liveRouting();
        fillChannelList (inputBox, inputs, true, r.input, false, {});
        fillChannelList (outputBox, outputs, false, r.output, r.stereoOutput, {});
    }

    void LiveStatusBar::setTunerOn (bool shouldBeOn)
    {
        if (tunerOn == shouldBeOn)
            return;

        tunerOn = shouldBeOn;
        engine.setTunerActive (tunerOn);   // 켜져 있는 동안 출력 무음. 켜고 끌 때 페이드는 엔진이 한다
        analyzer.reset();
        tunerStrip.setState ({});
        tunerStrip.setAlpha (tunerOn ? 1.0f : offAlpha);   // 꺼져 있으면 튜너 칸 전체를 흐리게

        if (tunerOn)
            startTimerHz (TunerAnalyzer::refreshHz);
        else
            stopTimer();

        repaint (tunerBounds);
    }

    void LiveStatusBar::timerCallback()
    {
        tunerStrip.setState (analyzer.update (engine, model.getTunerPitchHz()));
    }

    void LiveStatusBar::resized()
    {
        const auto area = getLocalBounds().reduced (live::gap);
        tunerBounds = bottomBarCell (area, tunerCell);
        tunerStrip.setBounds (bottomBarCell (area, stripCell, stripCells));
        patchBounds = bottomBarCell (area, patchCell);
        stompBounds = bottomBarCell (area, stompCell);
        liveBounds = bottomBarCell (area, bottomBarCells - 1);

        // 채널 (0 ~ 5 칸, 파라미터 바 IN / OUT 과 같은 자리·크기), 세로 가운데에 두 줄
        auto channels = bottomBarCell (area, 0, channelCells);
        channels = channels.withSizeKeepingCentre (channels.getWidth(), 2 * channelRowHeight + rowGap);

        auto inputRow = channels.removeFromTop (channelRowHeight);
        channels.removeFromTop (rowGap);
        auto outputRow = channels;

        inputLabel.setBounds (inputRow.removeFromLeft (channelLabelWidth));
        inputRow.removeFromLeft (channelLabelGap);
        inputBox.setBounds (inputRow);
        outputLabel.setBounds (outputRow.removeFromLeft (channelLabelWidth));
        outputRow.removeFromLeft (channelLabelGap);
        outputBox.setBounds (outputRow);
    }

    void LiveStatusBar::paint (juce::Graphics& g)
    {
        g.fillAll (live::barFill);
        g.setColour (live::border);
        g.drawRect (getLocalBounds(), 1);

        // 튜너 스위치: 켜짐 = Live 버튼처럼 바탕 + 2 px 테두리 (주황), 꺼짐 = 보통 버튼
        const auto tunerArea = tunerBounds.toFloat();
        const auto tunerColour = tunerOn ? TunerGauge::offColour : live::textMuted;

        if (tunerOn)
            live::drawIndicatorBox (g, tunerArea, live::tunerFill, TunerGauge::offColour);
        else
            live::drawButtonBox (g, tunerArea, live::stepBorder);

        const auto font = live::boldFont (live::buttonFontSize (tunerArea.getHeight()));   // Patch 글자와 Live 도 같은 글꼴
        const auto tunerFont = tunerOn ? font : juce::Font (juce::FontOptions (font.getHeight()));   // 켜져 있으면(모드 중) 굵게
        const float blockHeight = tunerIconSize + tunerTextGap + tunerFont.getHeight();
        const auto icon = juce::Rectangle<float> (tunerIconSize, tunerIconSize)
                              .withCentre ({ tunerArea.getCentreX(), tunerArea.getCentreY() - blockHeight * 0.5f + tunerIconSize * 0.5f });
        TunerButton::drawIcon (g, icon, tunerColour, tunerIconThickness);

        g.setColour (tunerColour);
        g.setFont (tunerFont);
        g.drawText ("Tuner", tunerArea.withY (icon.getBottom() + tunerTextGap).withHeight (tunerFont.getHeight()),
                    juce::Justification::centredTop, false);

        // Patch / Stomp: 칸 하나씩, 글자는 가운데. 지금 모드는 Live 처럼 켜짐 표시 (Patch 빨강, Stomp 파랑) + 굵게,
        // 다른 쪽은 보통 버튼에 흐린 글자. 지금은 Patch 모드뿐
        struct ModeCell { juce::Rectangle<int> bounds; const char* name; juce::Colour fill, colour; bool current; };
        const ModeCell modeCells[] { { patchBounds, "Patch", live::patchFill, live::patchRed, true },
                                     { stompBounds, "Stomp", live::stompFill, live::stompBlue, false } };

        for (const auto& m : modeCells)
        {
            if (m.current)
            {
                live::drawIndicatorBox (g, m.bounds.toFloat(), m.fill, m.colour);
                g.setColour (m.colour);
                g.setFont (font);
            }
            else
            {
                live::drawButtonBox (g, m.bounds.toFloat(), live::border);
                g.setColour (live::textMuted.withAlpha (0.5f));
                g.setFont (juce::FontOptions (font.getHeight()));
            }

            g.drawText (m.name, m.bounds, juce::Justification::centred, false);
        }

        // Live: 켜짐 표시 (편집 화면 Live 버튼과 같은 글자 크기를 굵게)
        const auto liveArea = liveBounds.toFloat();
        live::drawIndicatorBox (g, liveArea, live::liveFill, live::liveGreen);
        g.setColour (live::liveGreen);
        g.setFont (live::boldFont (live::buttonFontSize (liveArea.getHeight())));
        g.drawText ("Live", liveArea, juce::Justification::centred, false);
    }

    void LiveStatusBar::mouseDown (const juce::MouseEvent& e)
    {
        if (! isLeftClick (e))
            return;

        const auto pos = e.getPosition();

        if (tunerBounds.contains (pos))
            setTunerOn (! tunerOn);
        else if (liveBounds.contains (pos) && onLiveOff)
            onLiveOff();
        // Patch / Stomp: Stomp 모드가 생기기 전까지 반응 없음
    }
}
