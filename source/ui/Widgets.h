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

    // 저장 체크 버튼: 이름 입력칸 오른쪽 위 귀퉁이에 딱 붙는 정사각형 (CornerButton). 누르면 초록
    struct CheckCornerButton final : public CornerButton
    {
        CheckCornerButton (const juce::String& name, float cornerSize, float edgeInset)
            : CornerButton (name, cornerSize, edgeInset, juce::Colours::limegreen) {}

        void drawContent (juce::Graphics& g, juce::Rectangle<float> area, bool active) override
        {
            const auto box = area.reduced (area.getWidth() * 0.22f);

            juce::Path check;
            check.startNewSubPath (box.getX(), box.getCentreY());
            check.lineTo (box.getX() + box.getWidth() * 0.38f, box.getBottom());
            check.lineTo (box.getRight(), box.getY());

            g.setColour (juce::Colours::white.withAlpha (active ? 1.0f : 0.85f));
            g.strokePath (check, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    };

    // 이름 편집용 Label. 편집 중에는 모달이 되어 바깥 클릭이 컴포넌트에 닿기 전에 편집을 끝내는데,
    // allowWhileEditing 이 허락한 컴포넌트(저장 체크 버튼)의 클릭만은 그대로 통과시킨다 (그래야 체크로 저장할 수 있다)
    struct PassThroughLabel final : public juce::Label
    {
        std::function<bool (const juce::Component*)> allowWhileEditing;

        bool canModalEventBeSentToComponent (const juce::Component* target) override
        {
            return allowWhileEditing != nullptr && allowWhileEditing (target);
        }
    };

    // 앱 기본 LookAndFeel (V4 그대로). TextButton 글자는 자르지도 찌그러뜨리지도 않고 글꼴 크기만 줄여 맞춘다.
    // 한 줄과 "단어마다 한 줄" 중 글꼴을 더 크게 쓸 수 있는 쪽으로 그린다 (예: 좁은 "All Reset" → All / Reset)
    struct AppLookAndFeel : public juce::LookAndFeel_V4
    {
        void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
        {
            const auto area = button.getLocalBounds().reduced (3, 2).toFloat();
            const auto text = button.getButtonText().trim();
            const auto maxHeight = juce::LookAndFeel_V4::getTextButtonFont (button, button.getHeight()).getHeight();

            const auto bestFont = [&] (const juce::StringArray& lines)
            {
                auto font = juce::Font (juce::FontOptions (juce::jmin (maxHeight, area.getHeight() / (float) lines.size())));

                const auto fits = [&]
                {
                    for (const auto& line : lines)
                        if (juce::GlyphArrangement::getStringWidth (font, line) > area.getWidth())
                            return false;
                    return true;
                };

                while (font.getHeight() > 6.0f && ! fits())
                    font = font.withHeight (font.getHeight() - 0.5f);

                return font;
            };

            juce::StringArray lines { text };
            auto font = bestFont (lines);

            const auto words = juce::StringArray::fromTokens (text, " ", {});
            if (words.size() > 1)
            {
                if (const auto wordFont = bestFont (words); wordFont.getHeight() > font.getHeight())
                {
                    lines = words;
                    font = wordFont;
                }
            }

            g.setFont (font);
            g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                                    : juce::TextButton::textColourOffId)
                             .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));

            const auto lineHeight = font.getHeight();
            auto y = area.getCentreY() - lineHeight * (float) lines.size() / 2.0f;

            for (const auto& line : lines)
            {
                g.drawText (line, juce::Rectangle<float> (area.getX(), y, area.getWidth(), lineHeight),
                            juce::Justification::centred, false);
                y += lineHeight;
            }
        }
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
