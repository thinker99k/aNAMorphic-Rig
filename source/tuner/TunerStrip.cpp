#include "TunerStrip.h"

#include "ui/live/LiveStyle.h"

#include <cmath>

namespace rig
{
    namespace
    {
        constexpr int numBars = 3;                              // 한쪽 | 개수
        constexpr double barCents[numBars - 1] { 10.0, 20.0 };  // 안쪽 | 들이 맡는 오차의 끝 (마지막 | 는 그 너머 전부)

        // 가로 폭 합 = noteWidth + 2 x (barSpacing x numBars + arrowGap + arrowSize) = 124 (칸 2개 = 148 안)
        constexpr float noteWidth = 32.0f;      // 음 이름 칸
        constexpr float barSpacing = 9.0f;      // | 사이 (음 이름 칸 끝에서 첫 | 까지도 같은 값)
        constexpr float barWidth = 3.0f, barHeight = 22.0f;
        constexpr float arrowGap = 7.0f;        // 바깥 | 와 화살표 사이
        constexpr float arrowSize = 12.0f;
        constexpr float topShare = 0.62f;       // 윗줄이 차지하는 높이 비율
        constexpr float noteFontSize = 30.0f, centsFontSize = 14.0f;

        const juce::Colour unlit { 0xff4c5860 };   // 꺼진 | 와 화살표

        // 어긋난 정도에 맞는 | (0 = 음 이름 바로 옆)
        int barIndex (double absCents)
        {
            for (int i = 0; i < numBars - 1; ++i)
                if (absCents <= barCents[i])
                    return i;

            return numBars - 1;
        }
    }

    TunerStrip::TunerStrip()
    {
        setInterceptsMouseClicks (false, false);
    }

    void TunerStrip::setState (const TunerState& newState)
    {
        state = newState;
        repaint();
    }

    void TunerStrip::paint (juce::Graphics& g)
    {
        live::drawDisplayBox (g, getLocalBounds().toFloat());   // 어두운 표시 칸 (꺼져 있으면 LiveStatusBar 가 칸 전체를 흐리게)

        auto area = getLocalBounds().toFloat();
        const auto top = area.removeFromTop (area.getHeight() * topShare);
        const auto bottom = area;
        const float cx = top.getCentreX(), cy = top.getCentreY();

        const double absCents = std::abs (state.cents);
        const bool inTune = state.hasSignal && absCents <= TunerGauge::inTuneCents;
        const bool off = state.hasSignal && ! inTune;
        const bool flat = state.cents < 0.0;
        const int litBar = off ? barIndex (absCents) : -1;
        const auto noteColour = ! state.hasSignal ? TunerGauge::idleColour : (inTune ? TunerGauge::inTuneColour : TunerGauge::offColour);

        // | : 음 이름 칸 양옆으로 barSpacing 마다. 낮으면 왼쪽, 높으면 오른쪽 하나만 칠한다
        for (int side = -1; side <= 1; side += 2)
        {
            const bool litSide = off && (side < 0) == flat;

            for (int i = 0; i < numBars; ++i)
            {
                const float x = cx + (float) side * (noteWidth * 0.5f + barSpacing * (float) (i + 1));
                g.setColour (litSide && i == litBar ? TunerGauge::offColour : unlit);
                g.fillRect (juce::Rectangle<float> (barWidth, barHeight).withCentre ({ x, cy }));
            }

            // 화살표: 왼쪽 ▶ (올려라), 오른쪽 ◀ (내려라). 어긋난 쪽만 켠다
            const float tipDistance = noteWidth * 0.5f + barSpacing * (float) numBars + arrowGap;
            const float tipX = cx + (float) side * tipDistance;
            const float baseX = tipX + (float) side * arrowSize;
            juce::Path arrow;
            arrow.addTriangle (baseX, cy - arrowSize * 0.5f, baseX, cy + arrowSize * 0.5f, tipX, cy);
            g.setColour (litSide ? TunerGauge::offColour : unlit);
            g.fillPath (arrow);
        }

        g.setColour (noteColour);
        g.setFont (juce::FontOptions (noteFontSize, juce::Font::bold));
        g.drawText (state.hasSignal ? juce::String (state.note) : juce::String ("-"),
                    juce::Rectangle<float> (noteWidth, top.getHeight()).withCentre ({ cx, cy }), juce::Justification::centred, false);

        // 오차 (모달 게이지와 같은 형식)
        const int cents = juce::roundToInt (state.cents);
        g.setColour (state.hasSignal ? juce::Colours::white : TunerGauge::idleColour);
        g.setFont (juce::FontOptions (centsFontSize));
        g.drawText (state.hasSignal ? (cents >= 0 ? "+" : "") + juce::String (cents) + " cents" : juce::String ("- cents"),
                    bottom, juce::Justification::centredTop, false);
    }
}
