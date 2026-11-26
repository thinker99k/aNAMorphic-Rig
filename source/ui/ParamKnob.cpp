#include "ParamKnob.h"

#include "ValueSteps.h"

#include <cmath>

namespace rig
{
    namespace
    {
        constexpr float dimmedAlpha = 0.4f;    // 비활성일 때
        constexpr float textSize = 13.0f;      // 이름(굵게) / 팝업 값 글자 (상태바 IN / OUT 과 같은 크기)
        constexpr int lineHeight = 16;         // 이름 한 줄 (상태바 이름 줄과 같다)
        constexpr int knobDrop = 6;            // 호의 아래쪽이 열려 있어(약 8 px) 이름과 멀어 보이므로 노브만 이만큼 내린다 (이름 자리는 그대로)
        constexpr int popupHeight = 20, popupPadding = 6, popupGap = 2;   // 팝업 높이, 글자 양옆 여백, 노브 위 간격
    }

    ParamKnob::ParamKnob (const juce::String& name, double minValue, double maxValue, double defaultValue, ValueType valueType) : type (valueType)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

        if (type != ValueType::Db)
        {
            // 로그 매핑: 값 = 최솟값 * (최댓값 / 최솟값)^위치  (위치 0~1, 노브 가운데 = 기하 평균). Hz 는 1 Hz, Q 는 0.01 단위
            const double ratio = maxValue / minValue;
            const int decimals = type == ValueType::HzLog ? steps::hzDecimals : steps::qDecimals;
            slider.setNormalisableRange (juce::NormalisableRange<double> (
                minValue, maxValue,
                [ratio] (double start, double, double proportion) { return start * std::pow (ratio, proportion); },                      // 위치 -> 값
                [ratio] (double start, double, double value) { return juce::jlimit (0.0, 1.0, std::log (value / start) / std::log (ratio)); },   // 값 -> 위치
                [decimals] (double start, double end, double value) { return juce::jlimit (start, end, steps::roundTo (value, decimals)); })); // 단위로 반올림
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
            updatePopup();

            if (onValueChanged)
                onValueChanged (slider.getValue());
        };
        slider.onEditEnd = [this] { if (onDragEnded) onDragEnded(); };   // 끌었거나 더블클릭했을 때만 (누르고 떼기는 조작 아님)
        slider.addMouseListener (this, false);   // 노브에 올리기 / 내리기 / 누르기 / 떼기 -> 팝업

        const juce::Font nameFont { juce::FontOptions (textSize, juce::Font::bold) }, valueFont { juce::FontOptions (textSize) };
        nameLabel.setText (name, juce::dontSendNotification);
        nameLabel.setFont (nameFont);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setBorderSize ({});
        nameLabel.setInterceptsMouseClicks (false, false);
        nameLabel.setMinimumHorizontalScale (1.0f);   // 찌그러뜨리지 않고 말줄임
        addAndMakeVisible (nameLabel);

        // 팝업 폭: 양 끝 값, 범위 안에서 가장 긴 모양의 값 (숫자 폭은 글꼴마다 같으므로 8 로 잰다)
        juce::StringArray values { toText (minValue), toText (maxValue) };

        if (type == ValueType::HzLog)
        {
            if (minValue < 1000.0 && maxValue >= 100.0)    values.add ("888 Hz");
            if (maxValue >= 1000.0 && minValue < 10000.0)  values.add ("8.888 kHz");
        }

        auto widthOf = [] (const juce::Font& font, const juce::String& text)
        {
            return (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, text)) + 1;
        };

        nameWidth = widthOf (nameFont, name);

        for (const auto& v : values)
            popupWidth = juce::jmax (popupWidth, widthOf (valueFont, v));
        popupWidth += popupPadding * 2;

        addAndMakeVisible (slider);
        updatePopup();
    }

    ParamKnob::~ParamKnob()
    {
        slider.setLookAndFeel (nullptr);
    }

    void ParamKnob::setValue (double value)
    {
        slider.setValue (value, juce::dontSendNotification);   // onValueChange 가 불리지 않으므로 팝업 글자만 직접 갱신
        updatePopup();
    }

    void ParamKnob::enablementChanged()
    {
        setAlpha (isEnabled() ? 1.0f : dimmedAlpha);   // 비활성이면 마우스는 JUCE 가 막는다
        updatePopup();                                  // 비활성이면 팝업도 숨긴다
    }

    juce::String ParamKnob::toText (double value) const
    {
        if (type == ValueType::Db)
            return dbText (value);

        if (type == ValueType::QLog)
            return juce::String (steps::roundTo (value, steps::qDecimals), steps::qDecimals);   // "0.80"

        const int hz = juce::roundToInt (value);

        if (hz < 1000)
            return juce::String (hz) + " Hz";

        // kHz: 정수 Hz 를 그대로 보이도록 소수 셋째 자리까지, 끝의 0 과 점은 지운다 (1.234 kHz, 2.5 kHz, 20 kHz)
        auto khz = juce::String (hz / 1000.0, 3);
        if (khz.containsChar ('.'))
            khz = khz.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return khz + " kHz";
    }

    void ParamKnob::updatePopup()
    {
        // 노브 위에 마우스가 있거나 끄는 중일 때만. 띄울 곳 = 창 맨 위 컴포넌트 (처음 보일 때 붙인다)
        auto* top = getTopLevelComponent();
        const bool show = isEnabled() && isShowing() && top != this && (slider.isMouseOver() || slider.isMouseButtonDown());

        if (! show)
        {
            popup.setVisible (false);
            return;
        }

        if (popup.getParentComponent() != top)
            top->addChildComponent (popup);

        // 노브 가운데 위, 노브와 popupGap 간격
        const auto knob = top->getLocalArea (&slider, slider.getLocalBounds());
        popup.setBounds (knob.getCentreX() - popupWidth / 2, knob.getY() - popupGap - popupHeight, popupWidth, popupHeight);

        popup.text = toText (slider.getValue());
        popup.repaint();
        popup.setVisible (true);
        popup.toFront (false);
    }

    void ParamKnob::ValuePopup::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff1e1e1e));
        g.fillRect (bounds);
        g.setColour (juce::Colours::grey.withAlpha (0.6f));   // 패치 맵 어두운 바탕 위에서도 경계가 보이게
        strokeRect (g, bounds, 1.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (textSize));
        g.drawText (text, getLocalBounds(), juce::Justification::centred, false);
    }

    void ParamKnob::resized()
    {
        // [노브] 아래 [이름]: 둘 다 가로 가운데, 묶음은 세로 가운데 (노브만 knobDrop 내림). 높이가 모자라면 노브만 줄인다
        const int size = juce::jmax (0, juce::jmin (knobDiameter, getWidth(), getHeight() - nameGap - lineHeight));
        auto area = getLocalBounds().withSizeKeepingCentre (getWidth(), size + nameGap + lineHeight);

        slider.setBounds (area.removeFromTop (size).withSizeKeepingCentre (size, size).translated (0, knobDrop));
        area.removeFromTop (nameGap);
        nameLabel.setBounds (area);
    }
}
