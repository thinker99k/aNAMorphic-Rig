#include "TunerGauge.h"

#include <cmath>

namespace rig
{
    namespace
    {
        // 도면 좌표 (게이지 패널 TunerGauge::panelWidth x panelHeight = 404 x 254 안)
        constexpr float cx = 202.0f, cy = 246.0f;   // 게이지 중심 (바늘 뿌리, 보이지 않음)

        constexpr float arcRadius        = 200.0f;   // 기본 호, 정확 구간, 부채꼴, 굵은 호
        constexpr float tickOuterRadius  = 195.0f;
        constexpr float tickMajorInner   = 182.0f;   // 10 cents 마다 (길이 13)
        constexpr float tickMinorInner   = 189.0f;   // 그 사이 5 cents (길이 6)
        constexpr float numberRadius     = 218.0f;
        constexpr float needleRadius     = 206.0f;
        constexpr float innerRadius      = 70.0f;    // 음 이름 위 가는 호. 바늘·부채꼴은 여기서 시작
        constexpr float innerArcDegrees  = 60.0f;    // 가는 호는 ±60°

        constexpr float degreesPerCent = 1.2f;       // ±50 cents = ±60°
        constexpr float maxCents       = 50.0f;

        constexpr float baseArcThickness = 1.5f;
        constexpr float zoneThickness    = 3.0f;
        constexpr float boldArcThickness = 4.0f;     // Figma 29:225
        constexpr float needleThickness  = 2.0f;     // Figma 29:247
        constexpr float tickThickness    = 1.5f;
        constexpr float fillAlpha        = 0.22f;    // 부채꼴 투명도

        const juce::Rectangle<float> numberBox   { 32.0f, 16.0f };   // 숫자 하나 (가운데가 반지름 218 위)
        // 아래 줄: 가는 호 안에 음 이름, 양옆에 기준음 Hz(값 상자) / 오차 cents (가운데에서 같은 거리)
        const juce::Rectangle<float> noteArea    { 152.0f, 198.0f, 100.0f, 44.0f };
        const juce::Rectangle<int>   pitchBox    { 36, 208, 96, 24 };
        const juce::Rectangle<float> centsArea   { 272.0f, 208.0f, 96.0f, 24.0f };
        const juce::Rectangle<float> noSignalArea { 0.0f, 150.0f, 404.0f, 22.0f };

        constexpr float noteFontSize  = 40.0f;
        constexpr float smallFontSize = 14.0f;
        constexpr float numberFontSize = 11.0f;

        float toRadians (double cents)
        {
            return juce::degreesToRadians ((float) juce::jlimit (-(double) maxCents, (double) maxCents, cents) * degreesPerCent);
        }

        // 위쪽이 0, 시계 방향이 + 인 각도의 점
        juce::Point<float> pointAt (float radius, float angle)
        {
            return { cx + radius * std::sin (angle), cy - radius * std::cos (angle) };
        }

        void strokeArc (juce::Graphics& g, float radius, float from, float to, float thickness)
        {
            juce::Path p;
            p.addCentredArc (cx, cy, radius, radius, 0.0f, from, to, true);
            g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
    }

    juce::Rectangle<int> TunerGauge::pitchArea()
    {
        return pitchBox;
    }

    TunerGauge::TunerGauge()
    {
        setInterceptsMouseClicks (false, false);
    }

    void TunerGauge::setState (const TunerState& newState)
    {
        state = newState;
        repaint();
    }

    void TunerGauge::paint (juce::Graphics& g)
    {
        g.addTransform (juce::AffineTransform::scale ((float) getWidth() / panelWidth, (float) getHeight() / panelHeight));

        const auto background = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
        g.setColour (background.darker (0.3f));
        g.fillRoundedRectangle (0.0f, 0.0f, panelWidth, panelHeight, 6.0f);

        const bool inTune = state.hasSignal && std::abs (state.cents) <= inTuneCents;
        const auto colour = ! state.hasSignal ? idleColour : (inTune ? inTuneColour : offColour);
        const auto scaleColour = state.hasSignal ? juce::Colours::white.withAlpha (0.7f) : idleColour;
        const float edge = toRadians (maxCents);

        // 기본 호
        g.setColour (idleColour);
        strokeArc (g, arcRadius, -edge, edge, baseArcThickness);

        // 눈금 (5 cents 마다 21개)
        g.setColour (scaleColour);
        for (int k = -10; k <= 10; ++k)
        {
            const float angle = toRadians (k * 5.0);
            const float inner = (k % 2 == 0) ? tickMajorInner : tickMinorInner;
            g.drawLine (juce::Line<float> (pointAt (inner, angle), pointAt (tickOuterRadius, angle)), tickThickness);
        }

        // 숫자 (10 cents 마다)
        g.setFont (juce::FontOptions (numberFontSize));
        for (int cents = -50; cents <= 50; cents += 10)
            g.drawText (juce::String (cents), numberBox.withCentre (pointAt (numberRadius, toRadians (cents))),
                        juce::Justification::centred, false);

        // 정확 구간 (모든 상태)
        g.setColour (inTuneColour);
        strokeArc (g, arcRadius, toRadians (-inTuneCents), toRadians (inTuneCents), zoneThickness);

        if (state.hasSignal)
        {
            const float angle = toRadians (state.cents);
            const float from = juce::jmin (0.0f, angle), to = juce::jmax (0.0f, angle);

            // 부채꼴(가는 호 바깥만), 굵은 호, 바늘
            juce::Path fan;
            fan.addPieSegment (cx - arcRadius, cy - arcRadius, arcRadius * 2.0f, arcRadius * 2.0f, from, to, innerRadius / arcRadius);
            g.setColour (colour.withAlpha (fillAlpha));
            g.fillPath (fan);

            g.setColour (colour);
            strokeArc (g, arcRadius, from, to, boldArcThickness);
            g.drawLine (juce::Line<float> (pointAt (innerRadius, angle), pointAt (needleRadius, angle)), needleThickness);
        }

        // 음 이름 위 가는 호와 음 이름
        g.setColour (idleColour);
        strokeArc (g, innerRadius, -juce::degreesToRadians (innerArcDegrees), juce::degreesToRadians (innerArcDegrees), 1.0f);

        g.setColour (colour);
        g.setFont (juce::FontOptions (noteFontSize, juce::Font::bold));
        g.drawText (state.hasSignal ? juce::String (state.note) : juce::String ("-"), noteArea, juce::Justification::centred, false);

        // 오차: 음 이름 오른쪽 (주파수는 표시하지 않는다)
        g.setFont (juce::FontOptions (valueFontSize));
        g.setColour (state.hasSignal ? juce::Colours::white : idleColour);

        const int cents = juce::roundToInt (state.cents);
        g.drawText (state.hasSignal ? (cents >= 0 ? "+" : "") + juce::String (cents) + " cents" : juce::String ("- cents"),
                    centsArea, juce::Justification::centred, false);

        if (! state.hasSignal)
        {
            g.setColour (idleColour);
            g.setFont (juce::FontOptions (smallFontSize));
            g.drawText ("No input signal", noSignalArea, juce::Justification::centred, false);
        }
    }
}
