#include "KnobLookAndFeel.h"

#include <cmath>

namespace rig
{
    namespace
    {
        const juce::Colour arcColour (0xffd9822b);   // ChainView 의 NAM 타일과 같은 주황
    }

    void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider&)
    {
        // 굵기·여백은 지름에 비례 (지름 44 px 정도의 작은 노브에서도 잘리지 않게)
        const auto radius = (float) juce::jmin (width, height) * 0.5f;
        const auto arcThickness = juce::jlimit (2.5f, 4.0f, radius * 0.15f);
        const auto arcGap = juce::jlimit (1.5f, 3.0f, radius * 0.08f);   // 호와 몸통 사이 여백

        const auto cx = (float) x + (float) width * 0.5f;
        const auto cy = (float) y + (float) height * 0.5f;
        const auto arcRadius = radius - arcThickness;
        const auto bodyRadius = arcRadius - arcThickness * 0.5f - arcGap;

        if (bodyRadius <= 0.0f)
            return;

        // sliderPos 는 skew 가 적용된 위치 (0 dB = 0.5 = 12시)
        const auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const juce::PathStrokeType stroke (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        // 1. 몸통
        g.setColour (juce::Colour (0xff2a2a2a));
        g.fillEllipse (cx - bodyRadius, cy - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

        // 2. 호 트랙 (전체 범위)
        juce::Path track;
        track.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colours::darkgrey.darker (0.4f));
        g.strokePath (track, stroke);

        // 3. 값 호 (맨 왼쪽 ~ 현재 값, 단색)
        if (sliderPos > 0.0f)
        {
            juce::Path value;
            value.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour (arcColour);
            g.strokePath (value, stroke);
        }

        // 4. 지시선 (각도는 12시 기준 시계 방향). 길이는 몸통 반지름의 약 0.65 배
        auto pointAt = [&] (float r) { return juce::Point<float> (cx + r * std::sin (toAngle), cy - r * std::cos (toAngle)); };

        juce::Path pointer;
        pointer.startNewSubPath (pointAt (bodyRadius * 0.3f));
        pointer.lineTo (pointAt (bodyRadius * 0.95f));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.strokePath (pointer, juce::PathStrokeType (juce::jlimit (1.5f, 2.5f, radius * 0.1f),
                                                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}
