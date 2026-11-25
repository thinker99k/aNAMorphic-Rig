#pragma once

#include "engine/Block.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <memory>
#include <optional>
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

    // 장치 채널 번호 표시 (first 는 0부터, 화면은 1부터): 하나 = "3", 둘 = "1+2"
    inline juce::String channelText (int first, int count)
    {
        return count == 2 ? juce::String (first + 1) + "+" + juce::String (first + 2) : juce::String (first + 1);
    }

    // 아래 줄 14칸 격자 (편집 화면 씬 바, Live 아래 줄, 파라미터 바의 리셋 폭이 같이 쓴다): 칸 사이 bottomBarGap (= 바깥 여백),
    // 칸 폭 = (area 폭 - 13 x 간격) / 14 (1024 창에서 약 66.7 px), 칸 높이 = area 높이. first 번 칸부터 count 칸을 덮는 사각형
    constexpr int bottomBarCells = 14, bottomBarGap = 6;

    inline juce::Rectangle<int> bottomBarCell (juce::Rectangle<int> area, int first, int count = 1)
    {
        const float cell = (float) (area.getWidth() - (bottomBarCells - 1) * bottomBarGap) / (float) bottomBarCells;
        const auto edge = [&] (int i) { return area.getX() + juce::roundToInt ((float) i * (cell + (float) bottomBarGap)); };
        const int left = edge (first);
        const int right = edge (first + count) - bottomBarGap;
        return { left, area.getY(), right - left, area.getHeight() };
    }

    // 채널 줄 (파라미터 바의 IN / OUT, Live 아래 줄): 14칸 격자 0 ~ 5 칸에 [이름표] 간격 [목록 = 나머지], 줄 높이 28
    constexpr int channelCells = 6, channelLabelWidth = 110, channelLabelGap = 8, channelRowHeight = 28;

    // 입출력 채널 목록 (파라미터 바의 IN / OUT, Live 아래 줄이 같이 쓴다)
    //  - 항목 id: 단일 채널 = 1 + 채널(0부터), 출력 스테레오 쌍 = channelPairIdBase + 첫 채널(0부터)
    //  - 입력은 단일 채널만. 출력은 엔진이 모노 결과를 고른 채널에 복사하므로 스테레오 쌍 먼저, 그다음 단일 채널
    //  - used 의 채널은 보이되 고를 수 없다. 장치에 없는 채널이면 아무것도 선택하지 않고 번호만 보인다
    constexpr int channelPairIdBase = 1000;

    inline void fillChannelList (juce::ComboBox& box, const juce::StringArray& names, bool isInput,
                                 int first, bool stereo, const juce::BigInteger& used)
    {
        box.clear (juce::dontSendNotification);

        if (! isInput)
        {
            for (int i = 0; i + 1 < names.size(); i += 2)
            {
                box.addItem (channelText (i, 2) + ": " + names[i] + " / " + names[i + 1], channelPairIdBase + i);
                box.setItemEnabled (channelPairIdBase + i, ! used[i] && ! used[i + 1]);
            }
        }

        for (int i = 0; i < names.size(); ++i)
        {
            box.addItem (channelText (i, 1) + ": " + names[i], i + 1);
            box.setItemEnabled (i + 1, ! used[i]);
        }

        const bool pair = ! isInput && stereo;
        const int selectedId = pair ? channelPairIdBase + first : first + 1;
        box.setTextWhenNothingSelected (channelText (first, pair ? 2 : 1));
        box.setSelectedId (box.indexOfItemId (selectedId) >= 0 ? selectedId : 0, juce::dontSendNotification);
        box.setEnabled (! names.isEmpty());
    }

    // 고른 항목의 (첫 채널, 스테레오 쌍인지). 고른 것이 없으면 nullopt
    inline std::optional<std::pair<int, bool>> chosenChannel (const juce::ComboBox& box)
    {
        const int id = box.getSelectedId();

        if (id <= 0)
            return std::nullopt;

        return id >= channelPairIdBase ? std::pair { id - channelPairIdBase, true } : std::pair { id - 1, false };
    }

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
    //  - 왼쪽으로 누른 채 끌기: dragDescription 이 있으면 끌어 놓기 시작
    // 사용: 모델의 refreshComponentForCell 에서 refreshCell 을 그대로 돌려준다. 칸 그림은 모델의 paintCell 을 Cell 이 대신 부른다
    //      (JUCE 는 커스텀 컴포넌트가 있는 칸을 직접 그리지 않는다). 행 배경(paintRowBackground)은 JUCE 가 그대로 그린다
    class TableRowMouse
    {
    public:
        std::function<void (int row)> onDoubleClick;
        std::function<void (juce::PopupMenu&, int row)> fillMenu;

        // 왼쪽으로 누른 채 끌면 끌어 놓기를 시작한다 (부모 중 DragAndDropContainer 가 있어야 한다).
        // dragDescription 이 void 를 돌려주면 끌지 않는다. dragImage 는 커서 가운데에 붙는 그림
        std::function<juce::var (int row)> dragDescription;
        std::function<juce::Image (int row)> dragImage;

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

            void mouseDrag (const juce::MouseEvent& e) override
            {
                if (! isLeftClick (e) || owner.dragDescription == nullptr || e.getDistanceFromDragStart() < dragStartDistance)
                    return;

                auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
                if (container == nullptr || container->isDragAndDropActive())
                    return;

                const auto description = owner.dragDescription (row);
                if (description.isVoid())
                    return;

                const auto image = owner.dragImage != nullptr ? owner.dragImage (row) : juce::Image();
                const juce::Point<int> centre (-image.getWidth() / 2, -image.getHeight() / 2);
                container->startDragging (description, this, juce::ScaledImage (image), false, &centre);
            }

            static constexpr int dragStartDistance = 4;   // 이만큼 움직여야 끌기 (클릭·더블클릭과 구분)

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

    // 전원 아이콘 토글 버튼. 켜짐 = onColour, 꺼짐 = 흐린 회색. 누르면 상태가 바뀐다
    //  - PowerButton    : 블록 켜고 끄기 전용 (초록, size)
    //  - SubPowerButton : 블록 안 기능·합류 갈래 켜고 끄기 (빨강, 조금 작게)
    struct PowerButton : public LeftClickOnly<juce::Button>
    {
        static constexpr int size = 50;   // 보이는 크기 (정사각형)

        explicit PowerButton (juce::Colour on = juce::Colours::limegreen) : LeftClickOnly ("Power"), onColour (on)
        {
            setClickingTogglesState (true);
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (1.5f);
            const bool on = getToggleState();

            auto colour = on ? onColour : juce::Colours::grey.darker (0.3f);
            if (! isEnabled())
                colour = juce::Colours::grey.darker (0.8f);
            else if (down)
                colour = colour.darker (0.3f);
            else if (highlighted)
                colour = colour.brighter (0.3f);

            g.setColour (juce::Colours::black.withAlpha (on && isEnabled() ? 0.35f : 0.2f));
            g.fillRoundedRectangle (bounds, 6.0f);
            g.setColour (colour.withAlpha (0.6f));
            g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

            // 전원 기호: 위가 열린 원 + 세로선
            const auto icon = bounds.reduced (bounds.getHeight() * 0.25f);
            const auto centre = icon.getCentre();
            const float radius = icon.getHeight() * 0.5f;
            const float gapAngle = juce::MathConstants<float>::pi / 5.0f;

            juce::Path path;
            path.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                gapAngle, juce::MathConstants<float>::twoPi - gapAngle, true);
            path.startNewSubPath (centre.x, icon.getY() - radius * 0.15f);
            path.lineTo (centre.x, centre.y);

            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

    private:
        const juce::Colour onColour;
    };

    struct SubPowerButton final : public PowerButton
    {
        static constexpr int size = 40;   // 블록 전원(50)보다 조금 작게

        SubPowerButton() : PowerButton (juce::Colours::red) {}
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

    // 노브 공용 외형 (상태바 볼륨 노브, 파라미터 노브): 노브 몸통 + 호(맨 왼쪽 ~ 현재 값) + 지시선
    struct KnobLookAndFeel final : juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override
        {
            // 굵기·여백은 지름에 비례 (지름 44 px 정도의 작은 노브에서도 잘리지 않게)
            const auto radius = (float) juce::jmin (width, height) * 0.5f;
            const auto arcThickness = juce::jlimit (2.5f, 4.0f, radius * 0.15f);
            const auto arcGap = juce::jlimit (1.5f, 3.0f, radius * 0.08f);   // 호와 몸통 사이 여백

            const auto cx = (float) x + (float) width * 0.5f;
            const auto cy = (float) y + (float) height * 0.5f;
            const auto arcRadius = radius - arcThickness;
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
                g.setColour (juce::Colour (0xffd9822b));   // ChainView 의 NAM 타일과 같은 주황
                g.strokePath (value, stroke);
            }

            // 4. 지시선 (각도는 12시 기준 시계 방향). 길이는 몸통 반지름의 약 0.65 배
            auto pointAt = [&] (float r) { return juce::Point<float> (cx + r * std::sin (toAngle), cy - r * std::cos (toAngle)); };

            juce::Path pointer;
            pointer.startNewSubPath (pointAt (bodyRadius * 0.3f));
            pointer.lineTo (pointAt (bodyRadius * 0.95f));
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.strokePath (pointer, juce::PathStrokeType (juce::jlimit (1.5f, 2.5f, radius * 0.1f),
                                                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    };

    // 앱 기본 LookAndFeel (V4 그대로). TextButton 글자는 자르지도 찌그러뜨리지도 않고 글꼴 크기만 줄여 맞춘다.
    // 한 줄과 "단어마다 한 줄" 중 글꼴을 더 크게 쓸 수 있는 쪽으로 그린다. 글자에 \n 이 있으면 그 줄바꿈 그대로
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

            // 글자에 줄바꿈(\n)이 있으면 그대로 그 줄들로 (예: "All\nReset")
            auto lines = juce::StringArray::fromLines (text);
            auto font = bestFont (lines);

            const auto words = juce::StringArray::fromTokens (text, " ", {});
            if (lines.size() == 1 && words.size() > 1)
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

    // AlertWindow 와 같은 모양의 경고창 / 알림창. 버튼만 LeftClickOnly 로 직접 만든다 (AlertWindow::addButton 은 JUCE TextButton 을 만든다).
    // 버튼 자리는 AlertWindow::updateLayout 과 같다: 버튼 높이만큼 빈 칸(spacer)을 넣어 창 크기를 맞추고, 아래 95 % 선에 가운데 정렬
    //  content 가 있으면 글과 버튼 사이에 넣는다. closeWithKeys 면 Esc / Enter 도 첫 버튼 값으로 닫는다 (알림창)
    class LeftClickAlertWindow final : public LeftClickOnly<juce::AlertWindow>
    {
    public:
        LeftClickAlertWindow (const juce::String& title, const juce::String& message, juce::Component* associatedComponent,
                              const std::vector<std::pair<juce::String, int>>& buttonList,
                              juce::MessageBoxIconType icon = juce::MessageBoxIconType::WarningIcon,
                              std::unique_ptr<juce::Component> contentToOwn = nullptr, bool closeWithKeys = false)
            : LeftClickOnly (title, message, icon, associatedComponent),
              content (std::move (contentToOwn)),
              keyResult (closeWithKeys && ! buttonList.empty() ? std::optional<int> (buttonList.front().second) : std::nullopt)
        {
            if (content != nullptr)
                addCustomComponent (content.get());

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

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (keyResult.has_value() && (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey))
            {
                exitModalState (*keyResult);
                return true;
            }

            return juce::AlertWindow::keyPressed (key);
        }

    private:
        static constexpr int spacing = 16;
        std::unique_ptr<juce::Component> content;
        std::optional<int> keyResult;   // Esc / Enter 로 닫을 때의 값 (없으면 버튼으로만 닫힘)
        juce::OwnedArray<juce::TextButton> buttons;
        juce::Component spacer;
    };

    // 경고창을 프로그램 창 가운데에 모달로 띄운다. 버튼으로만 닫힌다 (ESC, X 없음). 버튼을 누르면 onResult (그 버튼의 값)
    inline void showAlert (juce::Component& origin, const juce::String& title, const juce::String& message,
                           const std::vector<std::pair<juce::String, int>>& buttons,
                           std::function<void (int)> onResult = nullptr)
    {
        auto* alert = new LeftClickAlertWindow (title, message, origin.getTopLevelComponent(), buttons);
        alert->setEscapeKeyCancels (false);
        alert->enterModalState (true, juce::ModalCallbackFunction::create ([onResult] (int result)
        {
            if (onResult != nullptr)
                onResult (result);
        }), true);
    }

    // 알림창 (사용자가 한 동작의 결과): 같은 자리에 OK 하나, OK / Esc / Enter 어느 것으로도 닫힌다.
    // 아이콘은 결과에 따라: 모두 잘 됨 = 정보(i), 실패가 있음 = 경고(!). list 가 있으면 글 아래에 읽기 전용 목록
    // (모두 볼 수 있게 noticeListRows 줄을 넘으면 스크롤)
    inline void showNotice (juce::Component& origin, const juce::String& title, const juce::String& message,
                            const juce::StringArray& list = {},
                            juce::MessageBoxIconType icon = juce::MessageBoxIconType::InfoIcon)
    {
        constexpr int noticeListRows = 8;      // 목록이 한 번에 보여 주는 최대 줄 수
        constexpr int noticeListWidth = 280;   // AlertWindow 최소 폭 350 의 80 % (커스텀 칸은 폭의 10 % 에서 시작)
        constexpr float noticeListBrighter = 0.3f;   // 목록 바탕 = 알림창 바탕보다 밝은 회색 (같은 색이면 목록 칸이 구분되지 않는다)

        std::unique_ptr<juce::Component> content;

        if (! list.isEmpty())
        {
            auto box = std::make_unique<LeftClickOnly<juce::TextEditor>>();
            box->setMultiLine (true, true);
            box->setReadOnly (true);
            box->setCaretVisible (false);
            box->setPopupMenuEnabled (false);   // 입력칸 오른쪽 클릭 메뉴는 쓰지 않는다 (마우스 규칙)
            box->setScrollbarsShown (true);
            box->setWantsKeyboardFocus (false);   // Esc / Enter 는 알림창이 받는다
            box->setMouseClickGrabsKeyboardFocus (false);
            box->setColour (juce::TextEditor::backgroundColourId,
                            juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::AlertWindow::backgroundColourId).brighter (noticeListBrighter));
            box->setText (list.joinIntoString ("\n"), false);

            const int rows = juce::jmin (list.size(), noticeListRows);
            const auto border = box->getBorder();
            const int height = juce::roundToInt (box->getFont().getHeight() * (float) rows)
                             + border.getTopAndBottom() + box->getTopIndent() * 2;
            box->setSize (noticeListWidth, height);
            content = std::move (box);
        }

        auto* alert = new LeftClickAlertWindow (title, message, origin.getTopLevelComponent(), { { "OK", 0 } },
                                                icon, std::move (content), true);
        alert->enterModalState (true, nullptr, true);
    }

    // 파일 쓰기 결과 알림 (패치 저장, 설정창 Save 의 audio-device.xml). 앱 전체에 상태 하나:
    // 성공하다가 실패로 바뀔 때만 경고 한 번. 실패가 이어지는 동안은 다시 띄우지 않고, 쓰기가 성공하면 다음 실패에 또 띄운다.
    // 시작·종료 때의 쓰기와 settings.json(볼륨)은 알리지 않는다
    inline void reportWriteResult (juce::Component& origin, bool ok, const juce::File& file)
    {
        static bool failing = false;

        if (ok)
        {
            failing = false;
            return;
        }

        if (failing)
            return;

        failing = true;
        showAlert (origin, "Could not save file",
                   file.getFullPathName() + "\n\nCheck disk space and folder permissions. Changes may be lost.",
                   { { "OK", 0 } });
    }

    // 초기화 경고: File not found 창과 같은 모양. [Reset] / [Cancel] 버튼으로만 닫힌다 (ESC, X 없음).
    // Reset 을 누르면 onReset 을 부른다. 프로그램 창 가운데에 띄운다.
    // 리셋 단계 (CLAUDE.md 6절):
    //   큰 리셋   = 씬 여러 개가 한꺼번에 바뀜 (패치 이름 메뉴 Reset, Scene Reset) -> 항상 이 경고. 이름 끝에 말줄임표.
    //               메뉴면 구분선 아래, 버튼이면 바 위아래를 꽉 채움
    //   중간 리셋 = 한 씬 안에서 값 여러 개 (씬 메뉴 Reset, Param Reset, Level Reset) -> 경고 없음 (의도한 동작일 가능성이 큼).
    //               버튼이면 큰 리셋 높이의 2/3
    //   작은 리셋 = 값 하나 (노브·슬라이더 더블클릭) -> 경고 없음
    inline void showResetWarning (juce::Component& origin, const juce::String& title, const juce::String& message,
                                  std::function<void()> onReset)
    {
        showAlert (origin, title, message, { { "Reset", 1 }, { "Cancel", 0 } }, [onReset] (int result)
        {
            if (result == 1 && onReset != nullptr)
                onReset();
        });
    }

    //==============================================================================
    // 블록 양식: 패치맵 블록, IN / OUT, 선택기에서 끌 때 보이는 블록이 같이 쓴다.
    // 색 하나로: 바탕 = 색.darker (0.6), 윗줄(종류 / IN·OUT) = 색.brighter (0.4) 굵게, 아랫줄(파일명 / 채널) = 흰색, 테두리 = 색
    namespace block
    {
        constexpr int width = 130;           // 블록 폭 (예전 150)
        constexpr int height = 44;           // 블록 높이 (예전 72): 위 여백 5 + 종류 줄 16 + 파일명 줄 18 + 아래 여백 5
        constexpr float corner = 8.0f;

        // 글자 배치: 윗줄 + 아랫줄 묶음을 세로 가운데에. 높이 44 기준 위아래 여백 (44 - 16 - 18) / 2 = 5 px 씩
        constexpr int headerLineHeight = 16, valueLineHeight = 18;
        constexpr int textInsetX = 10;           // 좌우 여백
        constexpr float headerFontSize = 12.0f;  // 윗줄
        constexpr float valueFontSize = 13.0f;   // 아랫줄

        inline juce::Colour colourFor (BlockType t)
        {
            switch (t)
            {
                case BlockType::Nam:  return juce::Colour (0xffd9822b);
                case BlockType::Ir:   return juce::Colour (0xff2b9bd9);
                case BlockType::None: break;
            }

            return juce::Colours::black;
        }

        inline juce::String labelFor (BlockType t)
        {
            switch (t)
            {
                case BlockType::Nam:  return "NAM";
                case BlockType::Ir:   return "IR";
                case BlockType::None: break;
            }

            return {};
        }

        inline juce::Rectangle<int> textBlock (juce::Rectangle<int> area)
        {
            return area.reduced (textInsetX, 0).withSizeKeepingCentre (area.getWidth() - 2 * textInsetX, headerLineHeight + valueLineHeight);
        }

        inline juce::Rectangle<int> headerLine (juce::Rectangle<int> area) { return textBlock (area).removeFromTop (headerLineHeight); }
        inline juce::Rectangle<int> valueLine (juce::Rectangle<int> area)  { return textBlock (area).withTrimmedTop (headerLineHeight); }

        inline juce::Font valueFont() { return juce::Font (juce::FontOptions (valueFontSize)); }

        // 로딩 실패 표시: 블록 위에 굵은 빨간 대각선 하나 (테두리와 같은 굵기)
        constexpr float errorThickness = 4.0f;

        inline void drawErrorSlash (juce::Graphics& g, juce::Rectangle<float> bounds)
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (bounds, corner);
            g.reduceClipRegion (clip);

            g.setColour (juce::Colours::red);
            g.drawLine (bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getY(), errorThickness);
        }

        inline void drawBlockBackground (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour fill)
        {
            g.setColour (fill);
            g.fillRoundedRectangle (area.toFloat().reduced (1.5f), corner);
        }

        // rightInset: 윗줄 오른쪽에 비울 폭 (일반 블록의 삭제 칸)
        inline void drawBlockHeader (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour, int rightInset = 0)
        {
            g.setColour (colour);
            g.setFont (juce::FontOptions (headerFontSize, juce::Font::bold));
            g.drawText (text, headerLine (area).withTrimmedRight (rightInset), juce::Justification::centredLeft, true);
        }

        // 테두리: 선택 = 흰색 2.5 px, 아니면 border 1.5 px.
        // 오류 = 굵은 빨간 대각선 + 같은 굵기의 빨간 테두리 (선택까지 되어 있으면 그 바깥에 흰 테두리)
        inline void drawBlockFrame (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour border, bool selected, bool failed)
        {
            const auto bounds = area.toFloat().reduced (1.5f);

            if (failed)
            {
                const auto inner = selected ? bounds.reduced (2.5f) : bounds;
                drawErrorSlash (g, inner);
                g.setColour (juce::Colours::red);
                g.drawRoundedRectangle (inner.reduced (errorThickness * 0.5f - 0.75f), corner, errorThickness);
            }

            if (selected || ! failed)
            {
                g.setColour (selected ? juce::Colours::white : border);
                g.drawRoundedRectangle (bounds, corner, selected ? 2.5f : 1.5f);
            }
        }

        // 끌어 놓기 내용 (DragAndDropContainer 의 sourceDescription).
        //  선택기 항목 : { "source": "library", "type": "nam" / "ir" / "none", "file": 절대 경로 }
        //  패치맵 블록 : { "source": "map", "slot": 슬롯 id }
        inline juce::var libraryDrag (BlockType type, const juce::File& file)
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty ("source", "library");
            obj->setProperty ("type", toString (type));
            obj->setProperty ("file", file.getFullPathName());
            return juce::var (obj.release());
        }

        inline juce::var mapDrag (juce::uint64 slotId)
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty ("source", "map");
            obj->setProperty ("slot", juce::String (slotId));   // var 에 uint64 가 없어서 문자열로
            return juce::var (obj.release());
        }

        // 끄는 동안 커서에 붙는 블록 그림의 불투명도 (밑의 놓을 자리 표시가 비쳐 보이게)
        constexpr float dragImageAlpha = 0.6f;

        // 선택기에서 끌 때 커서에 붙는 블록 그림 (삭제 칸 없음). 미지정 블록은 "Empty"
        inline juce::Image createImage (BlockType type, const juce::String& name)
        {
            const juce::Rectangle<int> area (width, height);
            juce::Image image (juce::Image::ARGB, width, height, true);
            juce::Graphics g (image);
            const bool none = type == BlockType::None;
            const auto colour = colourFor (type);

            drawBlockBackground (g, area, none ? juce::Colours::black : colour.darker (0.6f));
            drawBlockHeader (g, area, labelFor (type), colour.brighter (0.4f));
            g.setColour (none ? juce::Colours::grey : juce::Colours::white);
            g.setFont (valueFont());
            g.drawText (none ? juce::String ("Empty") : name, valueLine (area), juce::Justification::centredLeft, true);
            drawBlockFrame (g, area, none ? juce::Colours::grey.darker() : colour, false, false);
            image.multiplyAllAlphas (dragImageAlpha);
            return image;
        }
    }
}
