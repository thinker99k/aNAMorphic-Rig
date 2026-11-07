#include "GainKnob.h"

#include <cmath>

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
        if (minIsInf)
        {
            // 출력: 진폭 기준 오디오 테이퍼. gain(p) = Gmax * p^k, k = log2(Gmax) 이라 p = 0.5(12시) 에서 정확히 0 dB.
            //  dB(p) = max + 20k*log10(p).  p=0 -> -inf (min 이하로 붙임), p=1 -> max.
            //  체감 크기 ~ gain^0.6 이므로 돌린 만큼 거의 일정하게 커진다 (min 근처에서 튀지 않음)
            const double k = (double) max / (20.0 * std::log10 (2.0));   // +10 dB 면 약 1.66
            juce::NormalisableRange<double> range (
                min, max,
                [k] (double start, double end, double p)            // 0..1 -> dB
                {
                    if (p <= 0.0) return start;
                    return juce::jlimit (start, end, end + 20.0 * k * std::log10 (p));
                },
                [k] (double start, double end, double db)           // dB -> 0..1
                {
                    if (db <= start) return 0.0;
                    return juce::jlimit (0.0, 1.0, std::pow (10.0, (db - end) / (20.0 * k)));
                },
                [] (double start, double end, double db)            // 0.1 dB 단위
                {
                    return juce::jlimit (start, end, std::round (db * 10.0) / 10.0);
                });
            slider.setNormalisableRange (range);
        }
        else
        {
            // 입력: 범위를 정한 뒤에 skew 를 불러야 한다.
            // 대칭 범위라 skew 1 = 선형, 12시 = 0 dB
            slider.setRange (min, max, 0.1);
            slider.setSkewFactorFromMidPoint (0.0);
        }

        // 12시 기준 좌우 135도씩: 7시 30분(최소) ~ 4시 30분(최대)
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f,
                                    true);
        slider.setLookAndFeel (&knobLnf);
        slider.setValue (0.0, juce::dontSendNotification);
        // 휠로 바꾸면 드래그 끝(onDragEnd)이 오지 않아 저장이 안 되므로 휠은 막는다
        slider.setScrollWheelEnabled (false);
        slider.onValueChange = [this]
        {
            updateValueLabel();

            if (onValueChanged)
                onValueChanged ((float) slider.getValue());
        };
        slider.onDragEnd = [this] { if (onDragEnded) onDragEnded(); };

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
