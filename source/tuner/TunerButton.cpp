#include "TunerButton.h"

#include <cmath>

namespace rig
{
    namespace
    {
        const juce::Colour activeColour = indicator::amber;   // 앱 강조색 주황
        constexpr float iconSize = 14.0f;
        constexpr float iconGap  = 6.0f;
    }

    TunerButton::TunerButton() : LeftClickOnly ("Tuner")
    {
        setWantsKeyboardFocus (false);
        setTooltip ("Tuner");
    }

    void TunerButton::setActive (bool shouldBeActive)
    {
        if (active == shouldBeActive)
            return;

        active = shouldBeActive;
        repaint();
    }

    void TunerButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        auto& lf = getLookAndFeel();

        // 배경과 테두리: 꺼짐 = 같은 줄의 TextButton(Settings 등)과 같은 그리기, 켜짐 = 켜짐 표시 (Live 튜너 스위치와 같은 모양)
        if (active)
        {
            auto fill = indicator::amberFill;
            if (isButtonDown)
                fill = fill.brighter (0.2f);
            else if (isMouseOver)
                fill = fill.brighter (0.1f);

            indicator::drawBox (g, getLocalBounds().toFloat(), fill, activeColour);
        }
        else
        {
            lf.drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId), isMouseOver, isButtonDown);
        }

        const auto colour = active ? activeColour
                                   : findColour (juce::TextButton::textColourOffId).withMultipliedAlpha (isEnabled() ? 1.0f : 0.5f);

        // 아이콘 + 글자를 한 덩어리로 가운데에
        const juce::Font font (juce::FontOptions (juce::jmin (16.0f, (float) getHeight() * 0.6f),
                                                  active ? juce::Font::bold : juce::Font::plain));   // 튜너 모드 중이면 굵게
        const juce::String text = getButtonText();
        const float textWidth = juce::GlyphArrangement::getStringWidth (font, text);
        const float left = ((float) getWidth() - (iconSize + iconGap + textWidth)) * 0.5f;
        const auto icon = juce::Rectangle<float> (left, ((float) getHeight() - iconSize) * 0.5f, iconSize, iconSize);

        drawIcon (g, icon, colour, 1.5f);

        g.setColour (colour);
        g.setFont (font);
        g.drawText (text, juce::Rectangle<float> (icon.getRight() + iconGap, 0.0f, textWidth + 1.0f, (float) getHeight()),
                    juce::Justification::centredLeft, false);
    }

    void TunerButton::drawIcon (juce::Graphics& g, juce::Rectangle<float> icon, juce::Colour colour, float thickness)
    {
        // 아래 가운데를 중심으로 한 반원 호 + 오른쪽으로 기운 바늘 (14 x 14 에서 호 아래 여백 2, 반지름 = 절반 - 1)
        const float scale = icon.getWidth() / iconSize;
        const auto centre = juce::Point<float> (icon.getCentreX(), icon.getBottom() - 2.0f * scale);
        const float radius = icon.getWidth() * 0.5f - scale;

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, -juce::MathConstants<float>::halfPi,
                           juce::MathConstants<float>::halfPi, true);
        g.setColour (colour);
        g.strokePath (arc, juce::PathStrokeType (thickness));

        const float needleAngle = juce::degreesToRadians (25.0f);
        g.drawLine (juce::Line<float> (centre, centre.translated (radius * std::sin (needleAngle), -radius * std::cos (needleAngle))), thickness);
    }
}
