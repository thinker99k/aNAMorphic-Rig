#include "StatusBar.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace rig
{
    namespace
    {
        constexpr float meterFloorDb = -60.0f;
        constexpr float decayDbPerTick = 1.5f;   // 30Hz 기준 약 45dB/s

        constexpr int captionW = 34, valueW = 64;
        constexpr int gaugeW = 200;           // IN / OUT 게이지
        constexpr int cpuW = 160;             // CPU 게이지
        constexpr int gap = 6;                   // 구성 요소 사이 간격
        constexpr int knobW = 56, knobH = 56;    // 노브 크기는 고정, 상태바가 커지면 위아래 여백만 늘어난다
        constexpr int muteW = 56, muteH = 26;
        constexpr int gaugeH = 30;               // 상태바가 60 이어도 막대가 두꺼워지지 않게
    }

    Gauge::Gauge (juce::String c, juce::Colour col) : caption (std::move (c)), colour (col) {}

    void Gauge::setValue (float f, const juce::String& text)
    {
        f = juce::jlimit (0.0f, 1.0f, f);

        if (! juce::approximatelyEqual (f, fraction) || text != valueText)
        {
            fraction = f;
            valueText = text;
            repaint();
        }
    }

    void Gauge::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds();

        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (caption, area.removeFromLeft (captionW), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (12.0f));
        // 수치는 막대 바로 오른쪽에 붙인다 (왼쪽 정렬, 막대와 4 px 간격)
        auto valueArea = area.removeFromRight (valueW);
        g.drawText (valueText, valueArea.withTrimmedLeft (4), juce::Justification::centredLeft);

        auto bar = area.reduced (2, 6).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (bar, 3.0f);

        // 끝부분(위험 구간)은 빨간색
        const auto c = fraction > 0.9f ? juce::Colours::red : colour;
        g.setColour (c);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * fraction), 3.0f);
    }

    //==============================================================================
    StatusBar::StatusBar()
    {
        addAndMakeVisible (input);
        addAndMakeVisible (cpu);
        addAndMakeVisible (output);

        // 바로 옆에 IN / OUT 글자가 있으므로 노브 아래 이름은 숨긴다
        inputKnob.setNameVisible (false);
        outputKnob.setNameVisible (false);

        addAndMakeVisible (inputKnob);
        addAndMakeVisible (outputKnob);
        addAndMakeVisible (muteButton);
    }

    float StatusBar::toMeterFraction (float dB)
    {
        return juce::jmap (juce::jlimit (meterFloorDb, 0.0f, dB), meterFloorDb, 0.0f, 0.0f, 1.0f);
    }

    void StatusBar::update (float inputPeak, double cpuUsage, float outputPeak)
    {
        auto follow = [] (float& shown, float peak)
        {
            const auto db = juce::Decibels::gainToDecibels (peak, -100.0f);
            shown = db > shown ? db : juce::jmax (db, shown - decayDbPerTick);
        };

        follow (inputDb, inputPeak);
        follow (outputDb, outputPeak);

        auto dbText = [] (float db) { return db <= meterFloorDb ? juce::String ("-inf dB") : juce::String (db, 1) + " dB"; };

        input.setValue (toMeterFraction (inputDb), dbText (inputDb));
        output.setValue (toMeterFraction (outputDb), dbText (outputDb));
        cpu.setValue ((float) cpuUsage, juce::String (cpuUsage * 100.0, 1) + " %");
    }

    void StatusBar::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.3f));
    }

    void StatusBar::resized()
    {
        auto area = getLocalBounds().reduced (8, 2);
        auto centredY = [] (juce::Rectangle<int> r, int h) { return r.withSizeKeepingCentre (r.getWidth(), h); };

        // IN (왼쪽 끝 고정): [노브] [IN 게이지]
        auto in = area.removeFromLeft (knobW + gap + gaugeW);
        inputKnob.setBounds (in.removeFromLeft (knobW).withSizeKeepingCentre (knobW, knobH));
        in.removeFromLeft (gap);
        input.setBounds (centredY (in, gaugeH));

        // OUT (오른쪽 끝 고정): [OUT 게이지] [노브] [MUTE]
        auto out = area.removeFromRight (gaugeW + gap + knobW + gap + muteW);
        muteButton.setBounds (out.removeFromRight (muteW).withSizeKeepingCentre (muteW, muteH));
        out.removeFromRight (gap);
        outputKnob.setBounds (out.removeFromRight (knobW).withSizeKeepingCentre (knobW, knobH));
        out.removeFromRight (gap);
        output.setBounds (centredY (out, gaugeH));

        dividerX[0] = input.getRight() + gap;
        dividerX[1] = output.getX() - gap;

        // CPU: 창 정중앙, 고정 길이
        cpu.setBounds (getLocalBounds().withSizeKeepingCentre (cpuW, gaugeH));
    }
}
