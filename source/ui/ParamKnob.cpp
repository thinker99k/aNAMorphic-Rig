#include "ParamKnob.h"

#include <cmath>

namespace rig
{
    namespace
    {
        constexpr float dimmedAlpha = 0.4f;
    }

    ParamKnob::ParamKnob (const juce::String& name, double minValue, double maxValue, ValueType t) : type (t)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

        if (type == ValueType::HzLog)
        {
            // 로그 매핑: value = min * (max / min)^t  (노브 가운데 = 기하 평균)
            const double ratio = maxValue / minValue;
            slider.setNormalisableRange (juce::NormalisableRange<double> (
                minValue, maxValue,
                [ratio] (double start, double, double t) { return start * std::pow (ratio, t); },
                [ratio] (double start, double, double v) { return juce::jlimit (0.0, 1.0, std::log (v / start) / std::log (ratio)); },
                [] (double start, double end, double v) { return juce::jlimit (start, end, v); }));
        }
        else
        {
            slider.setRange (minValue, maxValue, 0.1);   // 선형
        }

        // 12시 기준 좌우 135도씩 (볼륨 노브와 같다)
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f,
                                    true);
        slider.setLookAndFeel (&knobLnf);
        slider.setScrollWheelEnabled (false);   // 볼륨 노브와 같이: 휠로는 드래그 끝(저장)이 오지 않으므로 막는다

        slider.onValueChange = [this]
        {
            updateValueLabel();

            if (onValueChanged)
                onValueChanged (slider.getValue());
        };
        slider.onDragEnd = [this] { if (onDragEnded) onDragEnded(); };

        nameLabel.setText (name, juce::dontSendNotification);
        nameLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
        valueLabel.setColour (juce::Label::textColourId, juce::Colours::white);

        for (auto* label : { &nameLabel, &valueLabel })
        {
            label->setJustificationType (juce::Justification::centredLeft);
            label->setBorderSize ({});
            label->setInterceptsMouseClicks (false, false);
            label->setMinimumHorizontalScale (0.7f);
            addAndMakeVisible (label);
        }

        addAndMakeVisible (slider);
        updateValueLabel();
    }

    ParamKnob::~ParamKnob()
    {
        slider.setLookAndFeel (nullptr);
    }

    void ParamKnob::setValue (double value)
    {
        slider.setValue (value, juce::dontSendNotification);   // onValueChange 가 불리지 않으므로 글자만 직접 갱신
        updateValueLabel();
    }

    void ParamKnob::setDimmed (bool shouldBeDimmed)
    {
        setAlpha (shouldBeDimmed ? dimmedAlpha : 1.0f);   // 투명도만 바꾼다: 마우스 조작은 그대로
    }

    juce::String ParamKnob::toText (double value) const
    {
        if (type == ValueType::Db)
        {
            // 0.1 단위로 맞춘 뒤 판단해야 -0.04 같은 값이 "-0.0" 으로 보이지 않는다
            const int tenths = juce::roundToInt (value * 10.0);
            const auto text = juce::String (tenths / 10.0, 1) + " dB";
            return tenths > 0 ? "+" + text : tenths == 0 ? juce::String ("0.0 dB") : text;
        }

        const int hz = juce::roundToInt (value);

        if (hz < 1000)
            return juce::String (hz) + " Hz";

        // kHz: 소수 둘째 자리까지, 끝의 0 과 점은 지운다 (1.25 kHz, 3.15 kHz, 20 kHz)
        auto khz = juce::String (value / 1000.0, 2);
        if (khz.containsChar ('.'))
            khz = khz.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return khz + " kHz";
    }

    void ParamKnob::updateValueLabel()
    {
        valueLabel.setText (toText (slider.getValue()), juce::dontSendNotification);
    }

    void ParamKnob::resized()
    {
        // [노브] [이름 / 값] : 노브 지름은 높이와 폭에서 정한다
        auto area = getLocalBounds();
        const int diameter = juce::jmin (area.getHeight(), area.getWidth() / 2);
        slider.setBounds (area.removeFromLeft (diameter).withSizeKeepingCentre (diameter, diameter));
        area.removeFromLeft (getWidth() / 20);

        const int lineHeight = juce::jmax (1, getHeight() / 5);
        auto text = area.withSizeKeepingCentre (area.getWidth(), lineHeight * 2);

        nameLabel.setFont (juce::FontOptions ((float) lineHeight * 0.8f));
        valueLabel.setFont (juce::FontOptions ((float) lineHeight * 0.9f));
        nameLabel.setBounds (text.removeFromTop (lineHeight));
        valueLabel.setBounds (text);
    }
}
