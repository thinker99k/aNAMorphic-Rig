#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 왼쪽 클릭에만 반응하는 TextButton (오른쪽/가운데 클릭은 무시)
    struct LeftClickTextButton : public juce::TextButton
    {
        using juce::TextButton::TextButton;

        void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isLeftButtonDown()) juce::TextButton::mouseDown (e); }
        void mouseUp (const juce::MouseEvent& e) override   { if (! e.mods.isPopupMenu() && ! e.mods.isMiddleButtonDown()) juce::TextButton::mouseUp (e); }
        void mouseDrag (const juce::MouseEvent& e) override { if (e.mods.isLeftButtonDown()) juce::TextButton::mouseDrag (e); }
    };

    // 둥근 버튼(블록, 씬 버튼 등)의 오른쪽 위 귀퉁이에 딱 붙는 작은 칸.
    // 오른쪽 위 모서리만 바깥 버튼과 같은 반지름으로 둥글게 깎고, 왼쪽/아래에만 구분선을 긋는다.
    // edgeInset = 바깥 버튼 테두리 선이 가장자리에서 들어온 만큼 (그 선에 맞춰 붙는다). 안쪽 그림은 drawContent
    struct CornerButton : public juce::Button
    {
        CornerButton (const juce::String& name, float cornerSize, float edgeInset, juce::Colour hoverColour)
            : juce::Button (name), cornerRadius (cornerSize), borderInset (edgeInset), activeColour (hoverColour) {}

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto area = getLocalBounds().toFloat().withTrimmedTop (borderInset).withTrimmedRight (borderInset);

            juce::Path shape;
            shape.addRoundedRectangle (area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                                       cornerRadius, cornerRadius, false, true, false, false);

            g.setColour (down        ? activeColour.darker (0.3f)
                       : highlighted ? activeColour.darker (0.1f)
                                     : juce::Colours::black.withAlpha (0.35f));
            g.fillPath (shape);

            g.setColour (juce::Colours::grey.withAlpha (0.7f));
            g.drawVerticalLine (0, area.getY(), area.getBottom());
            g.drawHorizontalLine (getHeight() - 1, 0.0f, area.getRight());

            drawContent (g, area, highlighted || down);
        }

        virtual void drawContent (juce::Graphics&, juce::Rectangle<float> area, bool active) = 0;

    private:
        const float cornerRadius, borderInset;
        const juce::Colour activeColour;
    };

    // 초기화 경고: File not found 창과 같은 모양. [Reset] / [Cancel] 버튼으로만 닫힌다 (ESC, X 없음).
    // Reset 을 누르면 onReset 을 부른다. 프로그램 창 가운데에 띄운다.
    // 기준: 두 개 이상 항목을 동시에 초기화하면 경고 (패치 리셋, All Reset).
    //   씬 하나 Reset (오른쪽 클릭 메뉴) 은 경고 없이 바로
    inline void showResetWarning (juce::Component& origin, const juce::String& title, const juce::String& message,
                                  std::function<void()> onReset)
    {
        auto* alert = new juce::AlertWindow (title, message, juce::MessageBoxIconType::WarningIcon,
                                             origin.getTopLevelComponent());
        alert->addButton ("Reset", 1);
        alert->addButton ("Cancel", 0);
        alert->setEscapeKeyCancels (false);
        alert->enterModalState (true, juce::ModalCallbackFunction::create ([onReset] (int result)
        {
            if (result == 1 && onReset != nullptr)
                onReset();
        }), true);
    }
}
