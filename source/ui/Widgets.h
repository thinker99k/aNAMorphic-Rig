#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <utility>
#include <vector>

namespace rig
{
    // 마우스 규칙 (앱 전체): 조작은 왼쪽 클릭으로만 한다. 오른쪽 클릭은 컨텍스트 메뉴가 있는 곳(씬 버튼, 패치 이름)에서만 쓰고,
    // 가운데/옆 버튼은 아무 동작도 하지 않는다. 클릭에 반응하는 컴포넌트는 LeftClickOnly<...> 로 선언하고,
    // 직접 mouseDown 을 처리하는 곳은 isLeftClick 으로 거른다
    inline bool isLeftClick (const juce::MouseEvent& e)
    {
        const auto buttons = e.mods.getRawFlags() & juce::ModifierKeys::allMouseButtonModifiers;
        return buttons == juce::ModifierKeys::leftButtonModifier && ! e.mods.isPopupMenu();   // macOS 의 Ctrl+클릭 = 오른쪽 클릭
    }

    // Base 의 마우스 동작을 왼쪽 클릭일 때만 부른다 (예: LeftClickOnly<juce::TextButton>)
    template <typename Base>
    struct LeftClickOnly : public Base
    {
        using Base::Base;

        void mouseDown (const juce::MouseEvent& e) override        { if (isLeftClick (e)) Base::mouseDown (e); }
        void mouseUp (const juce::MouseEvent& e) override          { if (isLeftClick (e)) Base::mouseUp (e); }
        void mouseDrag (const juce::MouseEvent& e) override        { if (isLeftClick (e)) Base::mouseDrag (e); }
        void mouseDoubleClick (const juce::MouseEvent& e) override { if (isLeftClick (e)) Base::mouseDoubleClick (e); }
    };

    // 컨텍스트 메뉴는 오른쪽 클릭한 커서 위치에 띄운다: 메뉴의 왼쪽 위 = 클릭 지점, 화면 밖으로 나가면 위/왼쪽으로 붙인다.
    // 영역이 비어 있으면(withMousePosition) JUCE 가 화면 가운데 쪽으로 펼치므로 1 px 영역을 준다
    inline void showContextMenu (juce::PopupMenu& menu, const juce::MouseEvent& click, std::function<void (int)> onClosed = nullptr)
    {
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ click.getScreenX(), click.getScreenY(), 1, 1 }),
                            std::move (onClosed));
    }

    // 표(TableListBox) 행의 마우스 규칙. JUCE 의 행은 어느 버튼으로 눌러도 선택하므로, 칸마다 투명한 Cell 을 얹어 클릭을 대신 받는다.
    //  - 왼쪽 클릭: 행 선택 / 왼쪽 더블클릭: onDoubleClick
    //  - 오른쪽 클릭: fillMenu 가 항목을 채우면 커서 위치에 메뉴. 선택은 바꾸지 않고, 메뉴가 열린 동안 그 행이 isMenuRow (테두리는 모델이 그린다).
    //      fillMenu 가 없거나 비면 무반응
    //  - 가운데 클릭: 무반응
    // 사용: 모델의 refreshComponentForCell 에서 refreshCell 을 그대로 돌려준다. 칸 그림은 모델의 paintCell 을 Cell 이 대신 부른다
    //      (JUCE 는 커스텀 컴포넌트가 있는 칸을 직접 그리지 않는다). 행 배경(paintRowBackground)은 JUCE 가 그대로 그린다
    class TableRowMouse
    {
    public:
        std::function<void (int row)> onDoubleClick;
        std::function<void (juce::PopupMenu&, int row)> fillMenu;

        bool isMenuRow (int row) const { return row >= 0 && row == menuRow; }

        juce::Component* refreshCell (int row, int columnId, juce::Component* existing)
        {
            auto* cell = dynamic_cast<Cell*> (existing);

            if (cell == nullptr)
            {
                delete existing;
                cell = new Cell (*this);
            }

            cell->row = row;
            cell->columnId = columnId;
            cell->repaint();
            return cell;
        }

    private:
        struct Cell final : public juce::Component
        {
            explicit Cell (TableRowMouse& o) : owner (o) {}

            juce::TableListBox* table() const { return findParentComponentOfClass<juce::TableListBox>(); }

            void paint (juce::Graphics& g) override
            {
                if (auto* t = table())
                    if (auto* model = t->getTableListBoxModel())
                        model->paintCell (g, row, columnId, getWidth(), getHeight(), t->isRowSelected (row));
            }

            void mouseDown (const juce::MouseEvent& e) override
            {
                if (auto* t = table())
                    owner.clicked (*t, row, e);
            }

            void mouseDoubleClick (const juce::MouseEvent& e) override
            {
                if (isLeftClick (e) && owner.onDoubleClick != nullptr)
                    owner.onDoubleClick (row);
            }

            TableRowMouse& owner;
            int row = -1, columnId = 0;
        };

        void clicked (juce::TableListBox& table, int row, const juce::MouseEvent& e)
        {
            if (isLeftClick (e))
            {
                table.selectRowsBasedOnModifierKeys (row, e.mods, false);
                return;
            }

            if (! e.mods.isPopupMenu() || fillMenu == nullptr)
                return;

            juce::PopupMenu menu;
            fillMenu (menu, row);

            if (menu.getNumItems() == 0)
                return;

            menuRow = row;
            table.repaint();

            // 메뉴가 열린 채로 다른 행을 우클릭하면 새 메뉴가 먼저 뜨고 옛 메뉴가 나중에 닫힌다.
            // 옛 메뉴의 닫힘이 새 메뉴의 테두리를 지우지 않도록, 가장 최근 메뉴가 닫힐 때만 지운다
            const auto menuId = ++lastMenuId;

            showContextMenu (menu, e, [this, menuId, safeTable = juce::Component::SafePointer<juce::TableListBox> (&table)] (int)
            {
                if (menuId != lastMenuId)
                    return;

                menuRow = -1;

                if (safeTable != nullptr)
                    safeTable->repaint();
            });
        }

        int menuRow = -1;      // 메뉴가 열린 행 (-1 = 없음)
        int lastMenuId = 0;    // 가장 최근에 연 메뉴 번호
    };

    // 둥근 버튼(블록, 씬 버튼 등)의 오른쪽 위 귀퉁이에 딱 붙는 작은 칸.
    // 오른쪽 위 모서리만 바깥 버튼과 같은 반지름으로 둥글게 깎고, 왼쪽/아래에만 구분선을 긋는다.
    // edgeInset = 바깥 버튼 테두리 선이 가장자리에서 들어온 만큼 (그 선에 맞춰 붙는다). 안쪽 그림은 drawContent
    struct CornerButton : public LeftClickOnly<juce::Button>
    {
        CornerButton (const juce::String& name, float cornerSize, float edgeInset, juce::Colour hoverColour)
            : LeftClickOnly (name), cornerRadius (cornerSize), borderInset (edgeInset), activeColour (hoverColour) {}

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

        // 입력칸의 오른쪽 클릭 메뉴(잘라내기/복사/붙여넣기)는 쓰지 않는다 (마우스 규칙)
        void editorShown (juce::TextEditor* editor) override
        {
            editor->setPopupMenuEnabled (false);
            juce::Label::editorShown (editor);
        }
    };

    // 앱 기본 LookAndFeel (V4 그대로). TextButton 글자는 자르지도 찌그러뜨리지도 않고 글꼴 크기만 줄여 맞춘다.
    // 한 줄과 "단어마다 한 줄" 중 글꼴을 더 크게 쓸 수 있는 쪽으로 그린다 (예: 좁은 "All Reset" → All / Reset)
    struct AppLookAndFeel : public juce::LookAndFeel_V4
    {
        // 창 제목 표시줄의 X 도 왼쪽 클릭만 (마우스 규칙). 모양은 LookAndFeel_V4 의 X 그대로 (V4 버튼 클래스는 JUCE 안에 숨어 있어 상속할 수 없다)
        struct WindowCloseButton final : public LeftClickOnly<juce::Button>
        {
            WindowCloseButton() : LeftClickOnly ("close") {}

            void paintButton (juce::Graphics& g, bool highlighted, bool down) override
            {
                const auto colour = juce::Colour (0xff9a131d);
                auto background = juce::Colours::grey;

                if (auto* lf = dynamic_cast<juce::LookAndFeel_V4*> (&getLookAndFeel()))
                    background = lf->getCurrentColourScheme().getUIColour (juce::LookAndFeel_V4::ColourScheme::widgetBackground);

                g.fillAll (background);
                g.setColour (! isEnabled() || down ? colour.withAlpha (0.6f) : colour);

                if (highlighted)
                {
                    g.fillAll();
                    g.setColour (background);
                }

                juce::Path cross;
                cross.addLineSegment ({ 0.0f, 0.0f, 1.0f, 1.0f }, 0.15f);
                cross.addLineSegment ({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.15f);

                const auto area = juce::Justification (juce::Justification::centred)
                                      .appliedToRectangle (juce::Rectangle<int> (getHeight(), getHeight()), getLocalBounds())
                                      .toFloat().reduced ((float) getHeight() * 0.3f);
                g.fillPath (cross, cross.getTransformToScaleToFit (area, true));
            }
        };

        juce::Button* createDocumentWindowButton (int buttonType) override
        {
            if (buttonType == juce::DocumentWindow::closeButton)
                return new WindowCloseButton();

            return juce::LookAndFeel_V4::createDocumentWindowButton (buttonType);
        }

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

    // AlertWindow 와 같은 모양의 경고창. 버튼만 LeftClickOnly 로 직접 만든다 (AlertWindow::addButton 은 JUCE TextButton 을 만든다).
    // 버튼 자리는 AlertWindow::updateLayout 과 같다: 버튼 높이만큼 빈 칸(spacer)을 넣어 창 크기를 맞추고, 아래 95 % 선에 가운데 정렬
    class LeftClickAlertWindow final : public LeftClickOnly<juce::AlertWindow>
    {
    public:
        LeftClickAlertWindow (const juce::String& title, const juce::String& message, juce::Component* associatedComponent,
                              const std::vector<std::pair<juce::String, int>>& buttonList)
            : LeftClickOnly (title, message, juce::MessageBoxIconType::WarningIcon, associatedComponent)
        {
            for (const auto& [name, result] : buttonList)
            {
                auto* button = buttons.add (new LeftClickOnly<juce::TextButton> (name));
                button->onClick = [this, value = result] { exitModalState (value); };
                addAndMakeVisible (button);
            }

            juce::Array<juce::TextButton*> buttonArray (buttons.begin(), buttons.size());
            const auto widths = getLookAndFeel().getWidthsForTextButtons (*this, buttonArray);
            const auto height = getLookAndFeel().getAlertWindowButtonHeight();
            int totalWidth = 40;

            for (int i = 0; i < buttons.size(); ++i)
            {
                buttons[i]->setSize (widths[i], height);
                totalWidth += 16 + widths[i];
            }

            spacer.setSize (totalWidth * 80 / 100, 10 + height);
            spacer.setInterceptsMouseClicks (false, false);   // 빈 칸이 버튼과 겹치므로 클릭은 통과시킨다
            addCustomComponent (&spacer);
        }

        void resized() override
        {
            juce::AlertWindow::resized();

            int totalWidth = -spacing;
            for (auto* b : buttons)
                totalWidth += b->getWidth() + spacing;

            auto x = (getWidth() - totalWidth) / 2;

            for (auto* b : buttons)
            {
                b->setTopLeftPosition (x, proportionOfHeight (0.95f) - b->getHeight());
                x += b->getWidth() + spacing;
            }
        }

    private:
        static constexpr int spacing = 16;
        juce::OwnedArray<juce::TextButton> buttons;
        juce::Component spacer;
    };

    // 경고창을 프로그램 창 가운데에 모달로 띄운다. 버튼을 누르면 onResult (그 버튼의 값). escapeCloses = Esc 로 닫기(값 0)
    inline void showAlert (juce::Component& origin, const juce::String& title, const juce::String& message,
                           const std::vector<std::pair<juce::String, int>>& buttons, bool escapeCloses,
                           std::function<void (int)> onResult = nullptr)
    {
        auto* alert = new LeftClickAlertWindow (title, message, origin.getTopLevelComponent(), buttons);
        alert->setEscapeKeyCancels (escapeCloses);
        alert->enterModalState (true, juce::ModalCallbackFunction::create ([onResult] (int result)
        {
            if (onResult != nullptr)
                onResult (result);
        }), true);
    }

    // 초기화 경고: File not found 창과 같은 모양. [Reset] / [Cancel] 버튼으로만 닫힌다 (ESC, X 없음).
    // Reset 을 누르면 onReset 을 부른다. 프로그램 창 가운데에 띄운다.
    // 기준: 두 개 이상 항목을 동시에 초기화하면 경고 (패치 리셋, All Reset).
    //   씬 하나 Reset (오른쪽 클릭 메뉴) 은 경고 없이 바로
    inline void showResetWarning (juce::Component& origin, const juce::String& title, const juce::String& message,
                                  std::function<void()> onReset)
    {
        showAlert (origin, title, message, { { "Reset", 1 }, { "Cancel", 0 } }, false, [onReset] (int result)
        {
            if (result == 1 && onReset != nullptr)
                onReset();
        });
    }
}
