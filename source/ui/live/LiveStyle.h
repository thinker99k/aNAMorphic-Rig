#pragma once

#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig::live
{
    // Live 화면 위 줄(PatchBar)과 아래 줄(LiveStatusBar)이 같이 쓰는 색과 글꼴 (시안)
    constexpr int gap = 6;   // 바깥 여백, 칸 사이 (편집 화면 SceneBar 와 같은 값)

    inline const juce::Colour barFill    { 0xff323c44 };
    inline const juce::Colour border     { 0xff4c5860 };
    inline const juce::Colour liveFill   { 0xff1c3b22 };
    inline const juce::Colour liveGreen  { 0xff35c435 };
    inline const juce::Colour tunerFill  { 0xff3b2c12 };   // 튜너 켜짐 바탕 (테두리·글자는 TunerGauge::offColour 주황)
    inline const juce::Colour stepFill   { 0xff2a343b };
    inline const juce::Colour boxFill    { 0xff1c2328 };   // 표시 칸 바탕 (패치 이름, 간이 튜너): 줄 바탕보다 어둡게
    inline const juce::Colour stepBorder { 0xff8a949b };
    inline const juce::Colour textLight  { 0xfff2f4f5 };
    inline const juce::Colour textMuted  { 0xffaeb7bd };

    inline juce::Font boldFont (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }

    // 버튼 글자 크기: 편집 화면 Live 버튼과 같은 값 (JUCE 버튼 글꼴 = min (16, 높이 x 0.6))
    inline float buttonFontSize (float height) { return juce::jmin (16.0f, height * 0.6f); }

    // 칸 모양 버튼 바탕 (사각 + 1 px 테두리)
    inline void drawButtonBox (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour outline)
    {
        g.setColour (stepFill);
        g.fillRect (area);
        g.setColour (outline);
        strokeRect (g, area.reduced (0.5f), 1.0f);
    }

    // 표시 칸 (패치 이름, 간이 튜너): 어두운 바탕 + 1 px 테두리
    inline void drawDisplayBox (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (boxFill);
        g.fillRect (area);
        g.setColour (border);
        strokeRect (g, area.reduced (0.5f), 1.0f);
    }

    // 켜짐 표시 버튼 (Live, 튜너 스위치): 바탕 + 2 px 테두리
    inline void drawIndicatorBox (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour fill, juce::Colour outline)
    {
        indicator::drawBox (g, area, fill, outline);   // 편집 화면 켜짐 표시와 같은 모양 (Widgets.h)
    }
}
