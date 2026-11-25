#include "GainKnob.h"

#include "ValueSteps.h"

#include <cmath>

namespace rig
{
    namespace
    {
        constexpr int labelHeight = 12;   // 아래쪽 이름 글자 높이
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
            const double k = (double) max / (20.0 * std::log10 (2.0));   // +12 dB 면 약 1.99 (위치 절반마다 -12 dB)
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
                    return juce::jlimit (start, end, steps::roundTo (db, steps::dbDecimals));
                });
            slider.setNormalisableRange (range);
        }
        else
        {
            // 입력: 범위를 정한 뒤에 skew 를 불러야 한다.
            // 대칭 범위라 skew 1 = 선형, 12시 = 0 dB
            slider.setRange (min, max, std::pow (10.0, -steps::dbDecimals));   // 0.1 dB 단위
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
        // 더블클릭하면 0 dB 로. JUCE 가 드래그 시작/끝 알림을 같이 보내서 onDragEnd 저장도 그대로 된다
        slider.setDoubleClickReturnValue (true, 0.0);

        slider.onValueChange = [this]
        {
            if (onValueChanged)
                onValueChanged ((float) slider.getValue());
        };
        slider.onDragEnd = [this] { if (onDragEnded) onDragEnded(); };

        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        nameLabel.setInterceptsMouseClicks (false, false);
        nameLabel.setBorderSize ({});   // 12 px 높이에 12 pt 글자가 들어가도록 안쪽 여백 없음
        nameLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        nameLabel.setText (name, juce::dontSendNotification);

        addAndMakeVisible (slider);
        addAndMakeVisible (nameLabel);
    }

    GainKnob::~GainKnob()
    {
        slider.setLookAndFeel (nullptr);
    }

    void GainKnob::setValueDb (float db)
    {
        slider.setValue (db, juce::dontSendNotification);
    }

    void GainKnob::setNameVisible (bool shouldBeVisible)
    {
        if (nameVisible == shouldBeVisible)
            return;

        nameVisible = shouldBeVisible;
        nameLabel.setVisible (nameVisible);
        resized();
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

    void GainKnob::resized()
    {
        auto area = getLocalBounds();

        nameLabel.setBounds (area.removeFromBottom (nameVisible ? labelHeight : 0));

        // 지름은 상수가 아니라 남은 공간 안의 정사각형으로 정한다
        const int size = juce::jmin (area.getWidth(), area.getHeight());
        slider.setBounds (area.withSizeKeepingCentre (size, size));
    }
}
