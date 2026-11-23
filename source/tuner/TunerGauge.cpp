#include "TunerGauge.h"

#include <cmath>

namespace rig
{
    namespace
    {
        // 도면 좌표 (게이지 패널 404 x 254 안, Figma 29:215 / 29:331 / 29:391)
        constexpr float panelWidth  = 404.0f;
        constexpr float panelHeight = 254.0f;
        constexpr float cx = 202.0f, cy = 246.0f;   // 게이지 중심

        constexpr float arcRadius        = 200.0f;   // 기본 호, 정확 구간, 부채꼴, 굵은 호
        constexpr float tickOuterRadius  = 195.0f;
        constexpr float tickMajorInner   = 182.0f;   // 10 cents 마다 (길이 13)
        constexpr float tickMinorInner   = 189.0f;   // 그 사이 5 cents (길이 6)
        constexpr float numberRadius     = 218.0f;
        constexpr float needleRadius     = 206.0f;
        constexpr float plateRadius      = 58.0f;    // 음 이름 반원 판

        constexpr float degreesPerCent = 1.2f;       // ±50 cents = ±60°
        constexpr float maxCents       = 50.0f;

        constexpr float baseArcThickness = 1.5f;
        constexpr float zoneThickness    = 3.0f;
        constexpr float boldArcThickness = 4.0f;     // Figma 29:225
        constexpr float needleThickness  = 2.0f;     // Figma 29:247
        constexpr float tickThickness    = 1.5f;
        constexpr float fillAlpha        = 0.22f;    // 부채꼴 투명도

        const juce::Rectangle<float> numberBox   { 32.0f, 16.0f };   // 숫자 하나 (가운데가 반지름 218 위)
        // 반원 판 안: 음 이름 위, 그 바로 밑에 cents (판 아래쪽 y 246 까지)
        const juce::Rectangle<float> noteArea    { 146.0f, 190.0f, 112.0f, 36.0f };
        const juce::Rectangle<float> centsArea   { 146.0f, 226.0f, 112.0f, 18.0f };
        const juce::Rectangle<float> noSignalArea { 0.0f, 150.0f, 404.0f, 22.0f };

        constexpr float noteFontSize  = 32.0f;
        constexpr float centsFontSize = 13.0f;
        constexpr float smallFontSize = 14.0f;
        constexpr float numberFontSize = 11.0f;
        const juce::Colour inTuneColour = juce::Colour (0xff32cd32);   // 전원 버튼과 같은 초록
        const juce::Colour offColour    = juce::Colour (0xffffa500);   // 앱 강조색 주황
        const juce::Colour idleColour   = juce::Colours::grey;

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

    TunerGauge::TunerGauge()
    {
        setInterceptsMouseClicks (false, false);
    }

    void TunerGauge::setState (const TunerState& newState)
    {
        if (newState == state)
            return;

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

            // 부채꼴, 굵은 호, 바늘
            juce::Path fan;
            fan.addPieSegment (cx - arcRadius, cy - arcRadius, arcRadius * 2.0f, arcRadius * 2.0f, from, to, 0.0f);
            g.setColour (colour.withAlpha (fillAlpha));
            g.fillPath (fan);

            g.setColour (colour);
            strokeArc (g, arcRadius, from, to, boldArcThickness);
            g.drawLine (juce::Line<float> ({ cx, cy }, pointAt (needleRadius, angle)), needleThickness);
        }

        // 반원 판과 음 이름 (바늘 뿌리를 덮는다)
        juce::Path plate;
        plate.addPieSegment (cx - plateRadius, cy - plateRadius, plateRadius * 2.0f, plateRadius * 2.0f,
                             -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, 0.0f);
        g.setColour (background);
        g.fillPath (plate);
        g.setColour (idleColour);
        g.strokePath (plate, juce::PathStrokeType (1.0f));

        g.setColour (colour);
        g.setFont (juce::FontOptions (noteFontSize, juce::Font::bold));
        g.drawText (state.hasSignal ? juce::String (state.note) : juce::String ("-"), noteArea, juce::Justification::centred, false);

        // 오차: 음 이름 밑 (주파수는 표시하지 않는다)
        g.setFont (juce::FontOptions (centsFontSize));
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
