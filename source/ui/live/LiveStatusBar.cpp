#include "LiveStatusBar.h"

#include "tuner/TunerButton.h"
#include "ui/live/LiveStyle.h"

namespace rig
{
    namespace
    {
        constexpr int units = 10;   // 폭 비율: 채널 5, 튜너 스위치 1, 간이 튜너 2, Patch / Stomp 1, Live 1
        constexpr int channelUnits = 5, tunerUnits = 1, stripUnits = 2, modeUnits = 1;

        // 채널 줄: 파라미터 바 IN / OUT 과 같은 크기 ([이름표 110] 8 [목록 320], 높이 28), 두 줄 사이 rowGap
        constexpr int labelWidth = 110, boxGap = 8, boxWidth = 320, rowHeight = 28, rowGap = 6;

        constexpr float tunerIconSize = 24.0f, tunerIconThickness = 2.0f;
        constexpr float tunerTextGap = 4.0f;   // 아이콘과 "Tuner" 사이
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
        auto area = getLocalBounds().reduced (live::gap);
        const int unit = (area.getWidth() - (units - 1) * live::gap) / units;
        const auto take = [&] (int n)
        {
            auto part = area.removeFromLeft (n * unit + (n - 1) * live::gap);
            area.removeFromLeft (live::gap);
            return part;
        };

        auto channels = take (channelUnits).withSizeKeepingCentre (labelWidth + boxGap + boxWidth, 2 * rowHeight + rowGap);
        channels.setX (live::gap);

        auto inputRow = channels.removeFromTop (rowHeight);
        channels.removeFromTop (rowGap);
        auto outputRow = channels;

        inputLabel.setBounds (inputRow.removeFromLeft (labelWidth));
        inputRow.removeFromLeft (boxGap);
        inputBox.setBounds (inputRow);
        outputLabel.setBounds (outputRow.removeFromLeft (labelWidth));
        outputRow.removeFromLeft (boxGap);
        outputBox.setBounds (outputRow);

        tunerBounds = take (tunerUnits);
        tunerStrip.setBounds (take (stripUnits));
        modeBounds = take (modeUnits);
        liveBounds = area;   // 나머지 (나눗셈 나머지 포함), 오른쪽 끝 = 편집 화면 Live 버튼과 같은 끝
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

        const auto font = live::boldFont (live::buttonFontSize (tunerArea.getHeight()));
        const float blockHeight = tunerIconSize + tunerTextGap + font.getHeight();
        const auto icon = juce::Rectangle<float> (tunerIconSize, tunerIconSize)
                              .withCentre ({ tunerArea.getCentreX(), tunerArea.getCentreY() - blockHeight * 0.5f + tunerIconSize * 0.5f });
        TunerButton::drawIcon (g, icon, tunerColour, tunerIconThickness);

        g.setColour (tunerColour);
        g.setFont (font);
        g.drawText ("Tuner", tunerArea.withY (icon.getBottom() + tunerTextGap).withHeight (font.getHeight()),
                    juce::Justification::centredTop, false);

        // Patch / Stomp: 두 줄, 지금 모드(Patch)만 흰색 굵게
        const auto modeArea = modeBounds.toFloat();
        live::drawButtonBox (g, modeArea, live::border);

        const float lineHeight = font.getHeight();
        auto lines = modeArea.withSizeKeepingCentre (modeArea.getWidth(), lineHeight * 2.0f + tunerTextGap);
        g.setColour (live::textLight);
        g.setFont (font);
        g.drawText ("Patch", lines.removeFromTop (lineHeight), juce::Justification::centred, false);
        lines.removeFromTop (tunerTextGap);
        g.setColour (live::textMuted.withAlpha (0.5f));
        g.setFont (juce::FontOptions (font.getHeight()));
        g.drawText ("Stomp", lines, juce::Justification::centred, false);

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
