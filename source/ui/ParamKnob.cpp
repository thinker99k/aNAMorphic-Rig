#include "ParamKnob.h"

#include "ValueSteps.h"

#include <cmath>

namespace rig
{
    namespace
    {
        constexpr float dimmedAlpha = 0.4f;    // 비활성일 때
        constexpr float textSize = 13.0f;      // 이름(굵게) / 값 글자 (상태바 IN / OUT 과 같은 크기)
        constexpr int lineHeight = 16;         // 이름 / 값 한 줄 (상태바 이름 줄과 같다)
    }

    ParamKnob::ParamKnob (const juce::String& name, double minValue, double maxValue, double defaultValue, ValueType t) : type (t)
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
                [] (double start, double end, double v) { return juce::jlimit (start, end, steps::roundTo (v, steps::hzDecimals)); }));   // 1 Hz 단위
        }
        else
        {
            slider.setRange (minValue, maxValue, std::pow (10.0, -steps::dbDecimals));   // 선형, 0.1 dB 단위
        }

        // 12시 기준 좌우 135도씩 (볼륨 노브와 같다)
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f,
                                    true);
        slider.setLookAndFeel (&knobLnf);
        slider.setDoubleClickReturnValue (true, defaultValue);   // JUCE 가 드래그 시작/끝도 보내 저장된다 (합류 레벨과 같다)
        slider.setScrollWheelEnabled (false);   // 볼륨 노브와 같이: 휠로는 드래그 끝(저장)이 오지 않으므로 막는다

        slider.onValueChange = [this]
        {
            updateValueLabel();

            if (onValueChanged)
                onValueChanged (slider.getValue());
        };
        slider.onDragEnd = [this] { if (onDragEnded) onDragEnded(); };

        nameLabel.setText (name, juce::dontSendNotification);
        nameLabel.setFont (juce::FontOptions (textSize, juce::Font::bold));
        valueLabel.setFont (juce::FontOptions (textSize));
        valueLabel.setColour (juce::Label::textColourId, juce::Colours::white);

        for (auto* label : { &nameLabel, &valueLabel })
        {
            label->setJustificationType (juce::Justification::centredLeft);
            label->setBorderSize ({});
            label->setInterceptsMouseClicks (false, false);
            label->setMinimumHorizontalScale (1.0f);   // 찌그러뜨리지 않고 말줄임
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

    void ParamKnob::enablementChanged()
    {
        setAlpha (isEnabled() ? 1.0f : dimmedAlpha);   // 비활성이면 마우스는 JUCE 가 막는다
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

        // kHz: 정수 Hz 를 그대로 보이도록 소수 셋째 자리까지, 끝의 0 과 점은 지운다 (1.234 kHz, 2.5 kHz, 20 kHz)
        auto khz = juce::String (hz / 1000.0, 3);
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
        // [노브] [이름 / 값] : 노브와 두 줄 글자 모두 세로 가운데
        auto area = getLocalBounds();
        const int diameter = juce::jmin (knobDiameter, getHeight());
        slider.setBounds (area.removeFromLeft (diameter).withSizeKeepingCentre (diameter, diameter));
        area.removeFromLeft (textGap);

        auto text = area.withSizeKeepingCentre (area.getWidth(), lineHeight * 2);
        nameLabel.setBounds (text.removeFromTop (lineHeight));
        valueLabel.setBounds (text);
    }
}
