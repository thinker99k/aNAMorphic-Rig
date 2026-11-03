#include "GainKnob.h"

namespace rig
{
    namespace
    {
        constexpr int labelHeight = 18;

        const juce::Colour arcColour (0xffd9822b);   // ChainView 의 NAM 타일과 같은 주황
        constexpr float arcThickness = 3.0f;
        constexpr float arcGap = 3.0f;               // 호와 몸통 사이 여백
    }

    void GainKnob::KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                      float sliderPos, float rotaryStartAngle,
                                                      float rotaryEndAngle, juce::Slider&)
    {
        const auto cx = (float) x + (float) width * 0.5f;
        const auto cy = (float) y + (float) height * 0.5f;
        const auto arcRadius = (float) juce::jmin (width, height) * 0.5f - arcThickness;
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

        // 4. 지시선 (각도는 12시 기준 시계 방향)
        auto pointAt = [&] (float r) { return juce::Point<float> (cx + r * std::sin (toAngle), cy - r * std::cos (toAngle)); };

        juce::Path pointer;
        pointer.startNewSubPath (pointAt (bodyRadius * 0.3f));
        pointer.lineTo (pointAt (bodyRadius));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.strokePath (pointer, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    //==============================================================================

    GainKnob::GainKnob (const juce::String& name, float min, float max, bool minShowsInf)
        : minDb (min), minIsInf (minShowsInf)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange (min, max, 0.1);
        // 범위를 정한 뒤에 불러야 한다: 0 dB 가 회전 범위 정중앙 (대칭 범위면 skew 1 = 선형)
        slider.setSkewFactorFromMidPoint (0.0);

        // 12시 기준 좌우 135도씩: 7시 30분(최소) ~ 4시 30분(최대)
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f,
                                    true);
        slider.setLookAndFeel (&knobLnf);
        slider.setValue (0.0, juce::dontSendNotification);
        slider.setDoubleClickReturnValue (true, 0.0);
        slider.onValueChange = [this]
        {
            updateValueLabel();

            if (onValueChanged)
                onValueChanged ((float) slider.getValue());
        };

        for (auto* label : { &valueLabel, &nameLabel })
        {
            label->setJustificationType (juce::Justification::centred);
            label->setColour (juce::Label::textColourId, juce::Colours::lightgrey);
            label->setInterceptsMouseClicks (false, false);
        }

        valueLabel.setFont (juce::FontOptions (12.0f));
        nameLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        nameLabel.setText (name, juce::dontSendNotification);

        addAndMakeVisible (valueLabel);
        addAndMakeVisible (slider);
        addAndMakeVisible (nameLabel);

        updateValueLabel();
    }

    GainKnob::~GainKnob()
    {
        slider.setLookAndFeel (nullptr);
    }

    void GainKnob::setValueDb (float db)
    {
        slider.setValue (db, juce::dontSendNotification);
        updateValueLabel();
    }

    juce::String GainKnob::toText (double db) const
    {
        if (minIsInf && db <= minDb)
            return "-inf dB";

        // 0.1 단위로 맞춘 뒤 판단해야 -0.04 같은 값이 "-0.0" 으로 보이지 않는다
        const int tenths = juce::roundToInt (db * 10.0);
        const auto text = juce::String (tenths / 10.0, 1) + " dB";

        if (tenths > 0)
            return "+" + text;

        if (tenths == 0)
            return "0.0 dB";

        return text;
    }

    void GainKnob::updateValueLabel()
    {
        valueLabel.setText (toText (slider.getValue()), juce::dontSendNotification);
    }

    void GainKnob::resized()
    {
        auto area = getLocalBounds();

        valueLabel.setBounds (area.removeFromTop (labelHeight));
        nameLabel.setBounds (area.removeFromBottom (labelHeight));

        // 남는 높이에 맞춰 정사각형 노브
        const int size = juce::jmin (area.getWidth(), area.getHeight());
        slider.setBounds (area.withSizeKeepingCentre (size, size));
    }
}
