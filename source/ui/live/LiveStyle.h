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
    inline const juce::Colour patchFill  { 0xff3b1c1c };   // Patch 모드 켜짐 (Live 녹색과 같은 밝기의 빨강)
    inline const juce::Colour patchRed   { 0xffd9534f };
    inline const juce::Colour stompFill  { 0xff1c2a3b };   // Stomp 모드 켜짐 (같은 밝기의 파랑)
    inline const juce::Colour stompBlue  { 0xff4a90d9 };
    inline const juce::Colour tunerFill  { 0xff3b2c12 };   // 튜너 켜짐 바탕 (테두리·글자는 TunerGauge::offColour 주황)
    inline const juce::Colour stepFill   { 0xff2a343b };
    inline const juce::Colour boxFill    { 0xff1c2328 };   // 표시 칸 바탕 (패치 이름, 간이 튜너): 줄 바탕보다 어둡게
    inline const juce::Colour stepBorder { 0xff8a949b };
    inline const juce::Colour textLight  { 0xfff2f4f5 };
    inline const juce::Colour textMuted  { 0xffaeb7bd };

    inline juce::Font boldFont (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }

    // 앱 기본 글꼴에 SemiBold 가 있으면 쓰고, 없으면 보통 굵기
    inline juce::Font semiBoldFont (float size)
    {
        static const juce::String style = []
        {
            const auto family = juce::Font (juce::FontOptions()).getTypefacePtr()->getName();
            for (const auto& s : juce::Font::findAllTypefaceStyles (family))
                if (s.removeCharacters (" -").equalsIgnoreCase ("SemiBold"))
                    return s;
            return juce::String ("Regular");
        }();

        return juce::Font (juce::FontOptions (size).withStyle (style));
    }

    // 버튼 글자 크기: 편집 화면 Live 버튼과 같은 값 (JUCE 버튼 글꼴 = min (16, 높이 x 0.6))
    inline float buttonFontSize (float height) { return juce::jmin (16.0f, height * 0.6f); }

    // 칸 모양 버튼 바탕 (둥근 사각 + 1 px 테두리)
    inline void drawButtonBox (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour outline)
    {
        g.setColour (stepFill);
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);
    }

    // 표시 칸 (패치 이름, 간이 튜너): 어두운 바탕 + 1 px 테두리
    inline void drawDisplayBox (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (boxFill);
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (border);
        g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);
    }

    // 켜짐 표시 버튼 (Live, 튜너 스위치): 바탕 + 2 px 테두리
    inline void drawIndicatorBox (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour fill, juce::Colour outline)
    {
        indicator::drawBox (g, area, fill, outline);   // 편집 화면 켜짐 표시와 같은 모양 (Widgets.h)
    }
}
