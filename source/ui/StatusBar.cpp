#include "StatusBar.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace rig
{
    namespace
    {
        constexpr float meterFloorDb = -60.0f;
        constexpr float decayDbPerTick = 1.5f;   // 30Hz 기준 약 45dB/s

        constexpr float captionFontSize = 13.0f; // 게이지 이름(IN / CPU / OUT) 글자 크기
        constexpr int barH = 14;                 // 막대 두께
        constexpr int captionH = 16;             // 이름 줄 높이
        constexpr int captionGap = 6;            // 이름 / MUTE 와 막대 사이 간격
        constexpr int gaugeW = 120;              // IN / CPU / OUT 게이지 길이 (셋 모두 같다)
        constexpr int gap = 6;                   // 구성 요소 사이 간격
        constexpr int knobW = 56, knobH = 56;    // 노브 크기는 고정, 상태바가 커지면 위아래 여백만 늘어난다
        constexpr int muteW = 56;                // MUTE 는 "OUT" 글자 왼쪽, 높이는 이름 줄 위 ~ 막대 아래
        // 위쪽 여백 추가: 노브 호 맨 위~상태바 위 끝 9 px, 노브 몸통 아래~상태바 아래 끝 7 px (상태바 62 기준)
        constexpr int topExtra = 8;
        constexpr int gaugeLift = 4;             // 게이지 묶음(이름·수치·막대·MUTE)만 노브보다 4 px 위로
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

        // 이름은 세로 가운데 선 위, 막대는 아래. 둘 사이 간격의 가운데 = 노브 중심 높이
        const int midY = area.getCentreY();
        const juce::Rectangle<int> captionArea (area.getX(), midY - captionGap / 2 - captionH, area.getWidth(), captionH);
        const auto bar = juce::Rectangle<int> (area.getX(), midY + captionGap / 2, area.getWidth(), barH).toFloat();

        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (captionFontSize, juce::Font::bold));
        g.drawText (caption, captionArea, juce::Justification::bottomLeft);

        // 수치는 이름 줄 오른쪽 끝 (막대 오른쪽 끝에 맞춘다)
        g.setFont (juce::FontOptions (captionFontSize));
        g.drawText (valueText, captionArea, juce::Justification::bottomRight);

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

        input.setValue (toMeterFraction (inputDb));
        output.setValue (toMeterFraction (outputDb));
        cpu.setValue ((float) cpuUsage, juce::String (cpuUsage * 100.0, 1) + " %");   // 성능 측정용 수치
    }

    void StatusBar::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.3f));
    }

    void StatusBar::resized()
    {
        // 노브와 게이지는 위쪽 topExtra 를 뺀 영역의 세로 가운데를 공유한다
        auto area = getLocalBounds().withTrimmedTop (topExtra).reduced (8, 0);

        // IN (왼쪽 끝 고정): [노브] [IN 게이지]
        auto in = area.removeFromLeft (knobW + gap + gaugeW);
        inputKnob.setBounds (in.removeFromLeft (knobW).withSizeKeepingCentre (knobW, knobH));
        in.removeFromLeft (gap);
        input.setBounds (in.translated (0, -gaugeLift));

        // OUT (오른쪽 끝 고정): [MUTE] [OUT 게이지] [노브]
        auto out = area.removeFromRight (muteW + gap + gaugeW + gap + knobW);
        outputKnob.setBounds (out.removeFromRight (knobW).withSizeKeepingCentre (knobW, knobH));
        out.removeFromRight (gap);
        output.setBounds (out.removeFromRight (gaugeW).translated (0, -gaugeLift));
        out.removeFromRight (gap);

        // MUTE: "OUT" 글자 바로 왼쪽, 높이는 이름 줄 위 ~ 막대 아래 (위쪽 4 px 덜어냄)
        const int midY = area.getCentreY() - gaugeLift;
        muteButton.setBounds (juce::Rectangle<int> (out.getCentreX() - muteW / 2, midY - captionGap / 2 - captionH,
                                                    muteW, captionH + captionGap + barH).withTrimmedTop (4));

        // CPU: 창 정중앙, 고정 길이
        cpu.setBounds (area.withSizeKeepingCentre (gaugeW, area.getHeight()).withX ((getWidth() - gaugeW) / 2).translated (0, -gaugeLift));
    }
}
