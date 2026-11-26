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
    // 마우스 규칙 (앱 전체): 조작은 왼쪽 클릭으로만 한다. 컨텍스트 메뉴는 없고 (모바일 대비, 할 일은 버튼으로),
    // 오른쪽/가운데/옆 버튼은 아무 동작도 하지 않는다. 클릭에 반응하는 컴포넌트는 LeftClickOnly<...> 로 선언하고,
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

    // 켜짐 표시 버튼 (모드·상태가 켜진 버튼, Live 화면 Live / 튜너 스위치와 같은 모양): 어두운 색 바탕 + 2 px 색 테두리, 글자도 그 색 굵게
    namespace indicator
    {
        inline const juce::Colour amber     { 0xffffa500 };   // 튜너, Copy / Swap, MUTE (앱 강조색 주황)
        inline const juce::Colour amberFill { 0xff3b2c12 };
        inline const juce::Colour red       { 0xffd9534f };   // Scene 모드 (Scene / Stomp 스위치)
        inline const juce::Colour redFill   { 0xff3b1c1c };
        inline const juce::Colour blue      { 0xff4a90d9 };   // Stomp 모드 (Scene / Stomp 스위치)
        inline const juce::Colour blueFill  { 0xff1c2a3b };
        constexpr float corner = 5.0f;

        inline void drawBox (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour fill, juce::Colour outline)
        {
            g.setColour (fill);
            g.fillRoundedRectangle (area, corner);
            g.setColour (outline);
            g.drawRoundedRectangle (area.reduced (1.0f), corner, 2.0f);
        }

        // TextButton 을 켜짐 표시로: 토글 상태가 켜지면 AppLookAndFeel 이 이 색으로 그린다
        inline void setColours (juce::TextButton& b, juce::Colour fill, juce::Colour accent)
        {
            b.setColour (juce::TextButton::buttonOnColourId, fill);
            b.setColour (juce::TextButton::textColourOnId, accent);
        }
    }

    // 마우스를 올리거나 누르는 동안에도 밝아지지 않는 버튼 (Live 와 연관되거나 Live 에서도 쓰는 버튼: Live, MUTE). AppLookAndFeel 이 본다
    inline void disableHoverHighlight (juce::Button& b) { b.getProperties().set ("noHover", true); }
    inline bool hoverHighlightDisabled (juce::Button& b) { return (bool) b.getProperties()["noHover"]; }

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

    // 파라미터 바의 묶음 구분선 |: cell 번 칸과 다음 칸 사이 틈 가운데, 위아래 cellSeparatorInset 만큼 띄운다
    // (Live 패치 번호 | 이름 구분선과 같은 색·여백)
    constexpr float cellSeparatorInset = 14.0f;

    inline void drawCellSeparator (juce::Graphics& g, juce::Rectangle<int> area, int cell)
    {
        const int x = (bottomBarCell (area, cell).getRight() + bottomBarCell (area, cell + 1).getX()) / 2;
        g.setColour (juce::Colour (0xff4c5860));
        g.drawVerticalLine (x, (float) area.getY() + cellSeparatorInset, (float) area.getBottom() - cellSeparatorInset);
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

    // 표(TableListBox) 행의 마우스 규칙. JUCE 의 행은 어느 버튼으로 눌러도 선택하므로, 칸마다 투명한 Cell 을 얹어 클릭을 대신 받는다.
    //  - 왼쪽 클릭: 행 선택 / 왼쪽 더블클릭: onDoubleClick
    //  - 오른쪽·가운데 클릭: 무반응 (컨텍스트 메뉴 없음)
    //  - 왼쪽으로 누른 채 끌기: dragDescription 이 있으면 끌어 놓기 시작
    // 사용: 모델의 refreshComponentForCell 에서 refreshCell 을 그대로 돌려준다. 칸 그림은 모델의 paintCell 을 Cell 이 대신 부른다
    //      (JUCE 는 커스텀 컴포넌트가 있는 칸을 직접 그리지 않는다). 행 배경(paintRowBackground)은 JUCE 가 그대로 그린다
    class TableRowMouse
    {
    public:
        std::function<void (int row)> onDoubleClick;

        // 왼쪽으로 누른 채 끌면 끌어 놓기를 시작한다 (부모 중 DragAndDropContainer 가 있어야 한다).
        // dragDescription 이 void 를 돌려주면 끌지 않는다. dragImage 는 커서 가운데에 붙는 그림
        std::function<juce::var (int row)> dragDescription;
        std::function<juce::Image (int row)> dragImage;

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
                table.selectRowsBasedOnModifierKeys (row, e.mods, false);
        }
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

    // 이름 편집 시작 연필 버튼: 이름 칸 오른쪽 끝 (CheckCornerButton 과 같은 자리·모양, 편집 중에는 체크 버튼이 대신 보인다)
    struct PencilCornerButton final : public CornerButton
    {
        PencilCornerButton (const juce::String& name, float cornerSize, float edgeInset)
            : CornerButton (name, cornerSize, edgeInset, juce::Colours::grey) {}

        void drawContent (juce::Graphics& g, juce::Rectangle<float> area, bool active) override
        {
            // 오른쪽 위 → 왼쪽 아래로 누운 연필 (20 x 20 틀): 몸통 + 지우개 머리
            juce::Path pencil;
            pencil.startNewSubPath (3.0f, 14.0f);
            pencil.lineTo (13.0f, 4.0f);
            pencil.lineTo (17.0f, 8.0f);
            pencil.lineTo (7.0f, 18.0f);
            pencil.lineTo (2.0f, 19.0f);
            pencil.closeSubPath();
            pencil.startNewSubPath (14.0f, 3.0f);
            pencil.lineTo (16.0f, 1.0f);
            pencil.lineTo (20.0f, 5.0f);
            pencil.lineTo (18.0f, 7.0f);
            pencil.closeSubPath();

            const auto box = area.reduced (area.getWidth() * 0.25f);
            pencil.applyTransform (pencil.getTransformToScaleToFit (box, true));
            g.setColour (juce::Colours::white.withAlpha (active ? 1.0f : 0.85f));
            g.fillPath (pencil);
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

    // 글자를 자르지도 찌그러뜨리지도 않고 글꼴 크기만 줄여 area 가운데에 그린다 (버튼 글자, 씬·Stomp 칸 이름).
    // 한 줄과 "단어마다 한 줄" 중 글꼴을 더 크게 쓸 수 있는 쪽. 글자에 \n 이 있으면 그 줄바꿈 그대로.
    // font = 가장 큰 크기와 굵기 (줄 수만큼 area 높이에 들어가도록 먼저 줄인다). 색은 미리 g 에
    inline void drawShrunkText (juce::Graphics& g, const juce::String& rawText, juce::Rectangle<float> area, const juce::Font& font)
    {
        constexpr float minHeight = 6.0f;
        const auto text = rawText.trim();

        const auto bestFont = [&] (const juce::StringArray& lines)
        {
            auto f = font.withHeight (juce::jmin (font.getHeight(), area.getHeight() / (float) lines.size()));

            const auto fits = [&]
            {
                for (const auto& line : lines)
                    if (juce::GlyphArrangement::getStringWidth (f, line) > area.getWidth())
                        return false;
                return true;
            };

            while (f.getHeight() > minHeight && ! fits())
                f = f.withHeight (f.getHeight() - 0.5f);

            return f;
        };

        auto lines = juce::StringArray::fromLines (text);
        auto best = bestFont (lines);

        const auto words = juce::StringArray::fromTokens (text, " ", {});
        if (lines.size() == 1 && words.size() > 1)
        {
            if (const auto wordFont = bestFont (words); wordFont.getHeight() > best.getHeight())
            {
                lines = words;
                best = wordFont;
            }
        }

        g.setFont (best);
        const auto lineHeight = best.getHeight();
        auto y = area.getCentreY() - lineHeight * (float) lines.size() / 2.0f;

        for (const auto& line : lines)
        {
            g.drawText (line, juce::Rectangle<float> (area.getX(), y, area.getWidth(), lineHeight), juce::Justification::centred, false);
            y += lineHeight;
        }
    }

    // 앱 기본 LookAndFeel (V4 그대로). TextButton 글자는 자르지도 찌그러뜨리지도 않고 글꼴 크기만 줄여 맞춘다 (drawShrunkText)
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

        // 토글이 켜진 TextButton (Copy / Swap, MUTE) = 켜짐 표시: buttonOnColourId 바탕 + textColourOnId 2 px 테두리
        void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                                   bool highlighted, bool down) override
        {
            if (hoverHighlightDisabled (button))
                highlighted = down = false;

            if (! button.getToggleState())
                return juce::LookAndFeel_V4::drawButtonBackground (g, button, background, highlighted, down);

            auto fill = background;
            if (down)
                fill = fill.brighter (0.2f);
            else if (highlighted)
                fill = fill.brighter (0.1f);

            indicator::drawBox (g, button.getLocalBounds().toFloat(), fill, button.findColour (juce::TextButton::textColourOnId));
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
        {
            const auto maxHeight = juce::LookAndFeel_V4::getTextButtonFont (button, button.getHeight()).getHeight();

            // 모드 스위치 원칙: 토글이 켜져 있으면 (그 모드에 들어가 있으면, 예: Copy / Swap) 글자를 굵게
            const int style = button.getToggleState() ? juce::Font::bold : juce::Font::plain;

            g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                                    : juce::TextButton::textColourOffId)
                             .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
            drawShrunkText (g, button.getButtonText(), button.getLocalBounds().reduced (3, 2).toFloat(),
                            juce::Font (juce::FontOptions (maxHeight, style)));
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
    // Undo / Redo 로 되돌아간 것 강조 (RigModel::getHighlight): 노란 테두리가 켜졌다가 highlightMs 동안 사라진다
    constexpr int highlightMs = 1000;

    inline float highlightAlpha (juce::uint32 start)
    {
        if (start == 0)
            return 0.0f;

        const auto elapsed = juce::Time::getMillisecondCounter() - start;
        return elapsed >= (juce::uint32) highlightMs ? 0.0f : 1.0f - (float) elapsed / (float) highlightMs;
    }

    inline void drawHighlight (juce::Graphics& g, juce::Rectangle<float> area, float alpha, float corner = 3.0f)
    {
        if (alpha <= 0.0f)
            return;

        g.setColour (juce::Colours::yellow.withAlpha (alpha));
        g.drawRoundedRectangle (area.expanded (2.0f), corner, 3.0f);
    }

    // 강조가 사라지는 동안 화면 갱신마다 component 를 다시 그린다 (끝난 뒤 한 번 더 그려 지운다)
    class HighlightRepaint
    {
    public:
        HighlightRepaint (juce::Component& component, std::function<juce::uint32()> getStart)
            : vblank (&component, [this, &component, startOf = std::move (getStart)]
              {
                  const bool on = highlightAlpha (startOf()) > 0.0f;
                  if (on || active)
                      component.repaint();
                  active = on;
              })
        {
        }

    private:
        bool active = false;
        juce::VBlankAttachment vblank;
    };

    // dB 값 글자 (파라미터 노브 팝업, 블록 타일의 중요 값): "+3.0 dB", "0.0 dB", "-3.0 dB".
    // 0.1 단위로 맞춘 뒤 판단해야 -0.04 같은 값이 "-0.0" 으로 보이지 않는다
    inline juce::String dbText (double value)
    {
        const int tenths = juce::roundToInt (value * 10.0);
        const auto text = juce::String (tenths / 10.0, 1) + " dB";
        return tenths > 0 ? "+" + text : tenths == 0 ? juce::String ("0.0 dB") : text;
    }

    namespace block
    {
        // 블록 = 박스 안 박스 2줄: [머리띠: 종류 (왼쪽) | 중요 값 (오른쪽), 종류 색으로 채움, 흰 굵은 글자] / 본문 [이름 한 줄]
        //   높이 44 = 머리띠 20 + 본문 24. 글자는 모두 같은 글꼴·크기 (fontSize), 머리띠만 굵게
        //   - 머리띠: 실제 글꼴의 대문자 높이를 머리띠의 보이는 부분(위 테두리 선 아래 ~ 본문 시작) 가운데에
        //     (대문자에는 아래 획이 없어 줄 가운데에 두면 위로 쏠려 보인다)
        //   - 본문: 글자 칸 하나 (칸 높이 = 글꼴 높이)를 본문 세로 가운데에, 왼쪽 정렬
        constexpr int width = 130;           // 블록 폭
        constexpr int height = 44;
        constexpr float corner = 3.0f;       // 모서리 (예전 8, 날카롭게)

        constexpr int bandHeight = 20;
        constexpr float frameInset = 1.5f, frameThickness = 1.5f;   // 테두리: 블록 가장자리에서 1.5 안쪽에 1.5 px 선 (선택은 selectedThickness)
        constexpr float selectedThickness = 2.5f;
        constexpr float selectedAlpha = 0.6f;   // 선택 테두리는 살짝 투명하게 (밑의 Stomp 파랑 테두리가 비친다)
        constexpr int textInsetX = 8;        // 좌우 여백
        constexpr float fontSize = 14.0f;    // 블록 안 모든 글자 (머리띠는 굵게)

        // 미지정 블록: 어두운 회색 머리띠 "EMPTY" (IN / OUT 의 테마 회청색과 구분) + 검정보다 살짝 밝은 본문에 "Passthrough" (소리 그대로 통과)
        const juce::Colour emptyFill (0xff1e1e1e);
        const juce::Colour emptyBand (0xff555555);

        // IN / OUT: 앱 테마 색 (JUCE LookAndFeel_V4 다크) = 머리띠·테두리 = 테두리색, 본문 = 콤보박스 바탕
        const juce::Colour endpointBand (0xff8e989b);
        const juce::Colour endpointFill (0xff263238);
        inline const juce::String emptyText { "Passthrough" };

        inline juce::Colour colourFor (BlockType t)
        {
            switch (t)
            {
                case BlockType::Nam:  return juce::Colour (0xffd9822b);
                case BlockType::Ir:   return juce::Colour (0xff4a8a52);   // Celestion Greenback 뒤판 같은 탁한 초록 (Stomp 강조 파랑과 구분)
                case BlockType::None: break;
            }

            return emptyBand;   // 미지정 블록
        }

        inline juce::String labelFor (BlockType t)
        {
            switch (t)
            {
                case BlockType::Nam:  return "NAM";
                case BlockType::Ir:   return "IR";
                case BlockType::None: break;
            }

            return "EMPTY";
        }

        inline juce::Rectangle<int> band (juce::Rectangle<int> area) { return area.withHeight (bandHeight); }

        inline juce::Font headerFont() { return juce::Font (juce::FontOptions (fontSize, juce::Font::bold)); }
        inline juce::Font nameFont()   { return juce::Font (juce::FontOptions (fontSize)); }

        // 글꼴의 대문자 높이 (실제 그린 "H" 의 높이). 글꼴은 OS 마다 다르므로 처음 쓸 때 한 번 잰다
        inline float capHeight (const juce::Font& font)
        {
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText (font, "H", 0.0f, 0.0f);
            return glyphs.getBoundingBox (0, -1, true).getHeight();
        }

        // baseline 에 글자가 앉도록 그릴 칸 (drawText 는 글꼴 높이를 칸 세로 가운데에 놓으므로 칸 높이 = 글꼴 높이면 baseline = 위 + ascent)
        inline juce::Rectangle<int> lineAt (juce::Rectangle<int> area, const juce::Font& font, float baseline)
        {
            return juce::Rectangle<float> ((float) area.getX() + (float) textInsetX, baseline - font.getAscent(),
                                           (float) (area.getWidth() - 2 * textInsetX), font.getHeight()).getSmallestIntegerContainer();
        }

        // 머리띠의 보이는 부분 = 위 테두리 선의 안쪽 끝 (frameInset + frameThickness / 2) ~ bandHeight
        constexpr float bandVisibleTop = frameInset + frameThickness * 0.5f;

        inline juce::Rectangle<int> headerLine (juce::Rectangle<int> area)
        {
            static const float cap = capHeight (headerFont());
            return lineAt (area, headerFont(), (float) area.getY() + (bandVisibleTop + (float) bandHeight) * 0.5f + cap * 0.5f);
        }

        inline juce::Rectangle<int> body (juce::Rectangle<int> area) { return area.withTrimmedTop (bandHeight); }

        // 본문 글자 칸 하나 (이름), 본문 세로 가운데에
        inline juce::Rectangle<int> nameLine (juce::Rectangle<int> area)
        {
            const auto inner = body (area).toFloat().reduced ((float) textInsetX, 0.0f);
            const float h = nameFont().getHeight();
            return inner.withY (inner.getY() + (inner.getHeight() - h) * 0.5f).withHeight (h).getSmallestIntegerContainer();
        }

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
            g.fillRoundedRectangle (area.toFloat().reduced (frameInset), corner);
        }

        // 꺼진 블록(바이패스)·꺼진 갈래 안 블록의 색 (패치맵, Stomp 칸)
        inline juce::Colour dimmed (juce::Colour c) { return c.withSaturation (0.1f).darker (0.5f); }
        inline juce::Colour dimmedText() { return juce::Colours::white.darker (0.5f); }

        // 머리띠: 종류 색으로 채운 윗줄 (위 두 모서리만 둥글게) + 흰 종류 글자 (굵게), value 가 있으면 오른쪽 끝에 (굵게)
        // textColour: 꺼진 블록은 흐린 흰색. justification: 블록은 왼쪽, IN / OUT 은 가운데
        inline void drawBlockHeader (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour,
                                     juce::Colour textColour = juce::Colours::white,
                                     juce::Justification justification = juce::Justification::centredLeft,
                                     const juce::String& value = {})
        {
            const auto strip = band (area).toFloat().reduced (frameInset, 0).withTrimmedTop (frameInset);
            juce::Path path;
            path.addRoundedRectangle (strip.getX(), strip.getY(), strip.getWidth(), strip.getHeight(), corner, corner, true, true, false, false);
            g.setColour (colour);
            g.fillPath (path);

            g.setColour (textColour);
            g.setFont (headerFont());
            g.drawText (value, headerLine (area), juce::Justification::centredRight, true);
            g.drawText (text, headerLine (area), justification, true);
        }

        // 본문 한 줄: 이름 (보통 흰색). 넘치면 말줄임
        inline void drawBlockText (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& name, juce::Colour nameColour)
        {
            g.setColour (nameColour);
            g.setFont (nameFont());
            g.drawText (name, nameLine (area), juce::Justification::centredLeft, true);
        }

        // 테두리: border 1.5 px. Stomp 에 묶인 블록 (Edit Stomp 모드) = Stomp 파랑 2.5 px.
        // 선택 = 그 위에 살짝 투명한 흰색 2.5 px (Stomp 파랑이 먼저 보이고 선택이 덮는다).
        // 오류 = 굵은 빨간 대각선 + 같은 굵기의 빨간 테두리 (선택·Stomp 까지 되어 있으면 그 바깥에)
        inline void drawBlockFrame (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour border, bool selected, bool failed,
                                    bool stompBound = false)
        {
            const auto bounds = area.toFloat().reduced (frameInset);

            if (failed)
            {
                const auto inner = selected || stompBound ? bounds.reduced (selectedThickness) : bounds;
                drawErrorSlash (g, inner);
                g.setColour (juce::Colours::red);
                g.drawRoundedRectangle (inner.reduced (errorThickness * 0.5f - 0.75f), corner, errorThickness);
            }

            if (stompBound)
            {
                g.setColour (indicator::blue);
                g.drawRoundedRectangle (bounds, corner, selectedThickness);
            }
            else if (! failed)
            {
                g.setColour (border);
                g.drawRoundedRectangle (bounds, corner, frameThickness);
            }

            if (selected)
            {
                g.setColour (juce::Colours::white.withAlpha (selectedAlpha));
                g.drawRoundedRectangle (bounds, corner, selectedThickness);
            }
        }

        // Stomp 칸 (Edit ssbar, Live 패드, ssbar 에서 끄는 그림): 블록 모양을 칸 크기로. 머리띠 = 종류 (가운데), 본문 = 파일명
        // (가운데, 자르지 않고 글꼴만 줄인다). 중요 값·칸 번호 없음. 꺼진 블록은 패치맵처럼 흐리게.
        // scale = 머리띠 높이·종류 글자·테두리·모서리 배율 (ssbar 1, Live 패드는 크게), nameSize = 파일명 최대 크기
        inline void drawStomp (juce::Graphics& g, juce::Rectangle<int> area, BlockType type, const juce::String& name, bool off,
                               float scale, const juce::Font& nameFontMax)
        {
            const bool none = type == BlockType::None;
            const auto colour = off ? dimmed (colourFor (type)) : colourFor (type);
            const auto textColour = off ? dimmedText() : juce::Colours::white;
            const auto bounds = area.toFloat().reduced (frameInset * scale);
            const float r = corner * scale, band = (float) bandHeight * scale;

            g.setColour (none ? emptyFill : colour.darker (0.75f));
            g.fillRoundedRectangle (bounds, r);

            juce::Path strip;
            strip.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), band - frameInset * scale, r, r, true, true, false, false);
            g.setColour (colour);
            g.fillPath (strip);

            // 머리띠 글자: 대문자 높이를 머리띠의 보이는 부분 가운데에 (headerLine 과 같은 방법)
            const auto header = headerFont().withHeight (fontSize * scale);
            const float visibleTop = (float) area.getY() + bandVisibleTop * scale;
            const float baseline = (visibleTop + (float) area.getY() + band) * 0.5f + capHeight (header) * 0.5f;
            g.setColour (textColour);
            g.setFont (header);
            g.drawText (labelFor (type), juce::Rectangle<float> (bounds.getX(), baseline - header.getAscent(), bounds.getWidth(), header.getHeight()),
                        juce::Justification::centred, false);

            const auto body = area.toFloat().withTrimmedTop (band).reduced ((float) textInsetX * scale, 4.0f * scale);
            drawShrunkText (g, none ? emptyText : name, body, nameFontMax);

            g.setColour (none ? juce::Colours::grey.darker() : colour);
            g.drawRoundedRectangle (bounds, r, frameThickness * scale);
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
        constexpr float dragImageAlpha = 0.70f;

        // 끄는 그림의 표시: 블록 오른쪽 위 꼭짓점에서 안쪽으로 markInset 들어간 곳을 중심으로 원 하나
        //  드래그앤드롭 원칙 (n개에서): 추가 n→n+1 / 삭제 n→n−1 / 스왑 있는 둘이 맞바뀜 / 이동 있는 하나가 빈 자리로 /
        //                              대체 있는 하나가 새것으로 / 복제 있는 하나가 다른 있는 하나로 덮어씌워짐
        //  - remove : 빨간 원 + 흰 X (삭제: 패치맵 블록·Stomp 칸을 둘레 밖에서 뗄 때)
        //  - add    : 초록 원 + 흰 + (추가·복제: 선택기 항목, ssbar 칸 위 절반, 패치맵 블록 → 빈 Stomp 칸)
        //  - replace: 어두운 노란 원 + 흰 ↔ (대체: 선택기 항목을 기존 블록 위에, 패치맵 블록 → 묶인 Stomp 칸)
        //  - swap   : 어두운 노란 원 + 흰 ⇄ (스왑: 패치맵 블록을 다른 블록 위에, ssbar 칸 아래 절반)
        //  - none   : 이동 (표시 없음, 놓을 자리 강조만)
        // 원이 블록 밖으로 나가므로 끄는 그림은 사방에 (반지름 - 안쪽) 만큼 여백을 둔다: 표시가 바뀌어도 블록 위치가 그대로, 그림 가운데 = 블록 가운데
        enum class DragMark { none, remove, add, replace, swap };
        constexpr float markRadius = 10.0f, markInset = 3.0f;

        // 표시 기호만 (X / + / ↔ / ⇄), 지금 색으로. arm = 가운데에서 팔 끝까지. 끄는 그림의 원 안과 ssbar 놓을 자리 힌트가 같이 쓴다
        inline void drawMarkSymbol (juce::Graphics& g, juce::Point<float> centre, DragMark mark, float arm, float thickness = 2.0f)
        {
            if (mark == DragMark::remove)
            {
                const float d = arm * 0.9f;
                g.drawLine (centre.x - d, centre.y - d, centre.x + d, centre.y + d, thickness);
                g.drawLine (centre.x + d, centre.y - d, centre.x - d, centre.y + d, thickness);
            }
            else if (mark == DragMark::add)
            {
                g.drawLine (centre.x - arm, centre.y, centre.x + arm, centre.y, thickness);
                g.drawLine (centre.x, centre.y - arm, centre.x, centre.y + arm, thickness);
            }
            else if (mark == DragMark::replace || mark == DragMark::swap)
            {
                // 가로 화살표: replace = ↔ (한 줄, 양쪽 화살촉), swap = ⇄ (위 →, 아래 ←). 화살촉·줄 간격은 arm 에 비례 (arm 4.5 에서 3)
                const float h = arm * 0.667f;

                const auto arrow = [&] (float y, bool right, bool left)
                {
                    g.drawLine (centre.x - arm, y, centre.x + arm, y, thickness);

                    if (right)
                    {
                        juce::Path head;
                        head.addTriangle (centre.x + arm + h * 0.5f, y, centre.x + arm - h, y - h, centre.x + arm - h, y + h);
                        g.fillPath (head);
                    }

                    if (left)
                    {
                        juce::Path head;
                        head.addTriangle (centre.x - arm - h * 0.5f, y, centre.x - arm + h, y - h, centre.x - arm + h, y + h);
                        g.fillPath (head);
                    }
                };

                if (mark == DragMark::replace)
                {
                    arrow (centre.y, true, true);
                }
                else
                {
                    arrow (centre.y - h, true, false);
                    arrow (centre.y + h, false, true);
                }
            }
        }

        inline juce::Image createDragImage (const juce::Image& block, DragMark mark)
        {
            const int pad = (int) std::ceil (markRadius - markInset);
            juce::Image image (juce::Image::ARGB, block.getWidth() + 2 * pad, block.getHeight() + 2 * pad, true);
            juce::Graphics g (image);
            g.drawImageAt (block, pad, pad);

            if (mark != DragMark::none)
            {
                const juce::Point<float> centre ((float) (pad + block.getWidth()) - markInset, (float) pad + markInset);
                g.setColour (mark == DragMark::remove ? juce::Colours::red
                           : mark == DragMark::add  ? juce::Colours::limegreen.darker (0.2f)
                                                    : juce::Colour (0xffc9a227));   // 어두운 노랑 (replace / swap)
                g.fillEllipse (juce::Rectangle<float> (markRadius * 2.0f, markRadius * 2.0f).withCentre (centre));

                g.setColour (juce::Colours::white);
                drawMarkSymbol (g, centre, mark, markRadius * 0.45f);
            }

            image.multiplyAllAlphas (dragImageAlpha);
            return image;
        }

        // 선택기에서 끌 때 커서에 붙는 블록 그림 (mark 표시 포함). 미지정 블록은 emptyFill / emptyText, 그 밖은 중요 값 줄에 새 블록의 기본값
        inline juce::Image createImage (BlockType type, const juce::String& name, DragMark mark = DragMark::add)
        {
            const juce::Rectangle<int> area (width, height);
            juce::Image block (juce::Image::ARGB, width, height, true);
            juce::Graphics g (block);
            const bool none = type == BlockType::None;
            const auto colour = colourFor (type);

            drawBlockBackground (g, area, none ? emptyFill : colour.darker (0.75f));
            drawBlockHeader (g, area, labelFor (type), colour, juce::Colours::white, juce::Justification::centredLeft,
                             none ? juce::String() : dbText (0.0));
            drawBlockText (g, area, none ? emptyText : name, juce::Colours::white);
            drawBlockFrame (g, area, none ? juce::Colours::grey.darker() : colour, false, false);
            return createDragImage (block, mark);
        }
    }
}
