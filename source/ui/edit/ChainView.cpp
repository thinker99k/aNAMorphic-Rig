#include "ChainView.h"

#include "engine/param/common/BlockParams.h"
#include "ui/Widgets.h"

namespace rig
{
    namespace
    {
        // 블록 양식 (Widgets.h)
        using block::colourFor, block::labelFor, block::nameLine, block::nameFont,
              block::drawBlockBackground, block::drawBlockHeader, block::drawBlockText, block::drawBlockFrame;

        constexpr int tileWidth = block::width;
        constexpr int tileHeight = block::height;
        constexpr int endWidth = tileWidth / 2;  // IN / OUT 노드 폭 = 블록 폭의 절반 (높이는 블록과 같다)
        constexpr int gap = 44;                  // 노드 사이 연결선 공간 (선 위에 놓으면 그 자리에 넣는다)
        constexpr int rowGap = 16;               // 병렬의 위 / 아래 갈래 줄 사이
        constexpr int dragStartDistance = 4;     // 블록을 이만큼 움직여야 끌기 (클릭과 구분)
        constexpr int keepMargin = gap / 2;      // 원래 블록을 사방으로 연결선 길이의 절반만큼 넓힌 범위: 놓을 자리 밖에서 커서가 이 밖이면 삭제
        constexpr int dropMargin = 12;           // 놓을 자리 반응 범위를 블록 줄 위 / 아래 줄 아래로 넓히는 폭 (줄 사이는 간격의 절반씩)
        constexpr int autoScrollBorder = 40;     // 끄는 동안 패치맵 좌우 끝에서 이 폭 안에 들어가면 자동 스크롤
        constexpr int autoScrollSpeed = 12;      //                최대 px / 이벤트
        const juce::Colour dropHighlight (juce::Colours::white.withAlpha (0.35f));   // 놓을 자리 표시 (희미하게)
        const juce::Colour dropHint (juce::Colours::white.withAlpha (0.12f));        // 끄는 동안 놓을 수 있는 자리 전부 (더 희미하게)
        const juce::Colour dropHover (juce::Colours::white.withAlpha (0.8f));        // 그중 커서가 올라간 자리 (진하게)
        constexpr float hoverThickness = 3.0f;
        constexpr float mergeSize = 28.0f;       // Merge 버튼의 보이는 크기 (예전 [+] 버튼 20 보다 크게)
        constexpr float mergeHoverScale = 1.25f; // 마우스를 올리면 이만큼 커진다 (28 → 35 px)
        constexpr double mergeAnimSeconds = 0.12;  //               커지고 작아지는 데 걸리는 시간
        constexpr int mergeHitSize = 38;         // Merge 버튼 클릭 영역 (정사각형, 커진 크기보다 조금 크게. 이웃 블록과는 gap 44 의 절반 안)

        // Merge 버튼 그림: [+] 버튼과 같은 둥근 사각 바탕 + 페이더 2개 아이콘 (위 / 아래 갈래 레벨을 섞는 곳). 선택 = 흰 테두리.
        // 바탕은 LookAndFeel_V4::drawButtonBackground 와 같은 모양
        void drawMergeButton (juce::Graphics& g, const juce::Component& c, juce::Point<float> centre, float size,
                              bool highlighted, bool selected)
        {
            const auto button = juce::Rectangle<float> (size, size).withCentre (centre);
            auto colour = c.findColour (juce::TextButton::buttonColourId).withMultipliedSaturation (0.9f);
            if (highlighted)
                colour = colour.contrasting (0.05f);

            g.setColour (colour);
            g.fillRoundedRectangle (button.reduced (0.5f), 6.0f);
            g.setColour (selected ? juce::Colours::white : c.findColour (juce::ComboBox::outlineColourId));
            g.drawRoundedRectangle (button.reduced (selected ? 1.25f : 0.5f), 6.0f, selected ? 2.5f : 1.0f);

            // 페이더 2개: 세로 홈 + 높이가 다른 손잡이 (콘솔 채널 페이더). 아이콘 크기는 버튼의 60 %
            const auto icon = button.reduced (size * 0.2f);
            const float capW = icon.getWidth() * 0.36f, capH = icon.getHeight() * 0.2f;

            g.setColour (c.findColour (juce::TextButton::textColourOffId));

            for (const auto [fx, fy] : { std::pair { 0.3f, 0.68f }, std::pair { 0.7f, 0.32f } })   // (가로 위치, 손잡이 높이) 비율
            {
                const float x = icon.getX() + icon.getWidth() * fx;
                g.drawLine (x, icon.getY(), x, icon.getBottom(), juce::jmax (1.0f, size * 0.06f));
                g.fillRoundedRectangle (juce::Rectangle<float> (capW, capH).withCentre ({ x, icon.getY() + icon.getHeight() * fy }), capH * 0.3f);
            }
        }
        constexpr float lineThickness = 2.0f;
        constexpr float connectorSize = 7.0f;    // 병렬 나누기 화살표 가운데 연결점 지름
        // 선 원칙: 켜진 선 = 흰 실선, 꺼진 선 = 어두운 회색 점선
        const juce::Colour activeLine (juce::Colours::white);
        const juce::Colour inactiveLine (juce::Colours::grey.darker (0.6f));
        constexpr float marqueeSpeed = 35.0f;    // 선택된 블록의 긴 파일명이 흘러가는 속도 (px/초)
        constexpr juce::uint32 marqueePauseMs = 1000;   // 처음과 끝에서 멈추는 시간
        constexpr int margin = 8;


        // 끌고 있는 것 (Widgets.h block::libraryDrag / mapDrag 의 내용)
        struct DragItem
        {
            bool fromLibrary = false;
            BlockType type = BlockType::None;   // 선택기 항목
            juce::File file;
            juce::uint64 slotId = 0;            // 패치맵 블록
        };

        std::optional<DragItem> readDrag (const juce::var& description)
        {
            const auto source = description.getProperty ("source", {}).toString();
            DragItem item;

            if (source == "library")
            {
                item.fromLibrary = true;
                item.type = blockTypeFromString (description.getProperty ("type", {}).toString());
                item.file = juce::File (description.getProperty ("file", {}).toString());
                return item;
            }

            if (source == "map")
            {
                item.slotId = (juce::uint64) description.getProperty ("slot", {}).toString().getLargeIntValue();
                return item;
            }

            return std::nullopt;
        }


        // 선택된 블록의 긴 파일명이 흘러간 거리 (elapsed = 처음 위치로 돌아간 뒤 지난 시간).
        // 처음에 멈춤 → 왼쪽으로 흘러 끝까지 → 끝에서 멈춤 → 처음으로 돌아가 반복
        float marqueeOffset (float overflow, juce::uint32 elapsed)
        {
            const auto moveMs = (juce::uint32) (overflow / marqueeSpeed * 1000.0f);
            const auto t = elapsed % (marqueePauseMs + moveMs + marqueePauseMs);

            if (t < marqueePauseMs)
                return 0.0f;

            return juce::jmin (overflow, (float) (t - marqueePauseMs) * marqueeSpeed / 1000.0f);
        }
    }

    //==============================================================================
    // 블록 하나를 나타내는 타일 (Widgets.h block:: 양식): 머리띠 = 종류(NAM / IR), 본문 = 파일명 + 가장 중요한 값 (NAM Gain, IR Output).
    // 꺼진 블록(바이패스)과 꺼진 갈래(무음) 안의 블록은 같은 흐린 색, 꺼진 블록의 값 줄은 "off"
    class ChainView::Tile final : public juce::Component
    {
    public:
        Tile (RigModel& m, juce::uint64 id, bool inActiveBranch, const juce::uint32& startMs, const StompDrag& stomp)
            : model (m), slotId (id), branchActive (inActiveBranch), marqueeStart (startMs), stompDrag (stomp)
        {
        }

        // 놓을 자리 위인지 / 그 자리가 다른 블록(Swap)인지 (Strip 이 정한다, 화면 좌표)
        std::function<bool (juce::Point<int>)> isOverDropZone, isOverSwap;

        // 선택은 끌지 않고 뗐을 때 (누를 때 고르면 패치맵이 다시 만들어져 끌기가 끊긴다).
        // 끌다 뗐을 때: 놓을 자리 위면 Strip 이 미리보기대로 처리하고 (우선), 자리 밖이면 삭제 범위(isDeleteAt)일 때만 삭제.
        // Edit Stomp 모드에서 ssbar 쪽(stompDrag)이면 지우지 않는다 (ssbar 칸 위면 ssbar 가 받는다).
        // (블록의 mouseUp 이 끌기 그림보다 먼저 불린다. Esc 로 취소했으면 끌기가 이미 끝나 있다)
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! isLeftClick (e))
                return;

            if (! e.mouseWasDraggedSinceMouseDown())
            {
                model.selectSlot (slotId);
                return;
            }

            auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
            const auto screen = e.getScreenPosition();

            if (container == nullptr || ! container->isDragAndDropActive() || ! isDeleteAt (screen) || stompMarkAt (screen).has_value())
                return;

            model.removeBlock (slotId);
        }

        // 더블클릭 = 켜고 끄기 (미지정 블록은 지정할 때까지 끌 수 없다).
        // JUCE 는 세 번째 클릭부터도 mouseDoubleClick 을 부르므로 정확히 두 번째 클릭에서만
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const auto* slot = model.findSlot (slotId);
            if (isLeftClick (e) && e.getNumberOfClicks() == 2 && slot != nullptr && slot->type != BlockType::None)
                model.setOn (slotId, ! model.isOn (*slot));
        }

        // 끌어서 다른 자리에 놓으면 이동
        // 끄는 동안 커서의 블록 그림 표시: 삭제 범위(isDeleteAt) = X, 다른 블록 위(Swap) = ⇄, 그 밖 = 없음 (떼면 일어날 일의 미리보기).
        // Edit Stomp 모드에서 선택기·ssbar 쪽이면 Stomp 칸 모양 그림 + ssbar 표시 (+ / ↔ / 없음)
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! isLeftClick (e) || e.getDistanceFromDragStart() < dragStartDistance)
                return;

            auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
            if (container == nullptr)
                return;

            if (container->isDragAndDropActive())
            {
                const auto screen = e.getScreenPosition();
                const auto stompMark = stompMarkAt (screen);
                const auto mark = stompMark.has_value() ? *stompMark
                                : isDeleteAt (screen) ? block::DragMark::remove
                                : isOverSwap != nullptr && isOverSwap (screen) ? block::DragMark::swap
                                                                               : block::DragMark::none;

                if (mark != shownMark || stompMark.has_value() != shownStomp)
                {
                    shownMark = mark;
                    shownStomp = stompMark.has_value();
                    container->setCurrentDragImage (juce::ScaledImage (shownStomp ? stompDrag.image (slotId, mark)
                                                                                  : block::createDragImage (snapshot, mark)));
                }

                return;
            }

            snapshot = createComponentSnapshot (getLocalBounds());
            shownMark = block::DragMark::none;
            shownStomp = false;
            const auto dragImage = block::createDragImage (snapshot, shownMark);

            // 커서 = 블록 가운데 = 끄는 그림 가운데 (표시 원 여백은 사방에 같다)
            const juce::Point<int> centre (-dragImage.getWidth() / 2, -dragImage.getHeight() / 2);
            container->startDragging (block::mapDrag (slotId), this, juce::ScaledImage (dragImage), false, &centre);
        }

        juce::uint64 getSlotId() const noexcept { return slotId; }

        std::optional<block::DragMark> stompMarkAt (juce::Point<int> screen) const
        {
            return stompDrag.markAt != nullptr ? stompDrag.markAt (screen) : std::nullopt;
        }

        // 화면 좌표 screen 에서 떼면 삭제되는지: 놓을 자리가 없고 (미리보기 조작이 우선), 커서가 원래 블록 + 사방 keepMargin 밖.
        // 블록 사이 연결선은 양쪽 블록이 절반씩 나눠 가진다 (블록을 옆 화살표까지 살짝 밀어도 지워지지 않게)
        bool isDeleteAt (juce::Point<int> screen) const
        {
            return ! getScreenBounds().expanded (keepMargin).contains (screen) && ! (isOverDropZone != nullptr && isOverDropZone (screen));
        }

        void paint (juce::Graphics& g) override
        {
            const auto* slot = model.findSlot (slotId);
            if (slot == nullptr)
                return;   // 지워진 블록: 변경 알림(비동기)으로 타일을 다시 만들기 전에 그려지는 경우

            const bool none = slot->type == BlockType::None;

            // 블록 양식. 미지정 블록은 회색 머리띠 "EMPTY" + block::emptyFill 본문에 "Passthrough"
            drawBlockBackground (g, getLocalBounds(), none ? block::emptyFill : baseColour (*slot).darker (0.75f));
            const auto textColour = isDimmed (*slot) ? block::dimmedText() : juce::Colours::white;

            // 머리띠 오른쪽 = 중요 값 (이름은 아래에서 따로: 선택된 긴 이름은 흘러간다)
            drawBlockHeader (g, getLocalBounds(), labelFor (slot->type), baseColour (*slot), textColour,
                             juce::Justification::centredLeft, headline (*slot));

            if (none)
            {
                drawBlockText (g, getLocalBounds(), block::emptyText, textColour);
                return;
            }

            const auto nameColour = textColour;
            const auto name = slot->file.getFileNameWithoutExtension();

            const auto text = nameArea();
            g.setColour (nameColour);
            g.setFont (nameFont());
            const auto overflow = nameOverflow();

            // 선택되지 않았거나 다 들어가면 앞부분만 (넘치면 말줄임). 선택된 긴 이름은 이름 칸 안에서 흘러간다
            if (model.getSelectedSlotId() != slotId || overflow <= 0.0f)
            {
                g.drawText (name, text, juce::Justification::centredLeft, true);
                return;
            }

            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (text);
            const auto offset = marqueeOffset (overflow, juce::Time::getMillisecondCounter() - marqueeStart);
            g.drawText (name, text.toFloat().withX ((float) text.getX() - offset).withWidth ((float) text.getWidth() + overflow + 1.0f),
                        juce::Justification::centredLeft, false);
        }

        // 파일명이 이름 칸보다 긴 만큼 (px). 0 이하면 다 들어간다
        float nameOverflow() const
        {
            const auto* slot = model.findSlot (slotId);
            if (slot == nullptr || slot->type == BlockType::None)
                return 0.0f;

            const auto width = juce::GlyphArrangement::getStringWidth (nameFont(),
                                                                      slot->file.getFileNameWithoutExtension());
            return width - (float) nameArea().getWidth();
        }

        // 파일명 줄 = 본문 첫 줄
        juce::Rectangle<int> nameArea() const
        {
            return nameLine (getLocalBounds());
        }

        void paintOverChildren (juce::Graphics& g) override
        {
            const auto* slot = model.findSlot (slotId);
            if (slot == nullptr)
                return;

            // Edit Stomp 모드: Stomp 칸에 묶인 블록은 Stomp 파랑 테두리 (선택 테두리가 그 위에 살짝 투명하게)
            const bool stompBound = model.isStompMode()
                                 && std::find (slot->stomps.begin(), slot->stomps.end(), true) != slot->stomps.end();
            drawBlockFrame (g, getLocalBounds(), slot->type == BlockType::None ? juce::Colours::grey.darker() : baseColour (*slot),
                            model.getSelectedSlotId() == slotId, slot->error.isNotEmpty(), stompBound);
        }

    private:
        // 값 줄: 지금 씬의 가장 중요한 값 (BlockParams::headlineIndex), 꺼진 블록은 "off", 미지정 블록은 없음
        juce::String headline (const RigModel::Slot& slot) const
        {
            if (slot.type == BlockType::None)
                return {};

            if (! model.isOn (slot))
                return "off";

            const auto index = BlockParams::headlineIndex (slot.type);
            return index >= 0 ? dbText (model.getParam (slot, index)) : juce::String();
        }

        // 꺼진 블록(바이패스) 또는 꺼진 갈래(무음) 안
        bool isDimmed (const RigModel::Slot& slot) const
        {
            return ! branchActive || (! model.isOn (slot) && slot.type != BlockType::None);
        }

        juce::Colour baseColour (const RigModel::Slot& slot) const
        {
            const auto base = colourFor (slot.type);
            return isDimmed (slot) ? block::dimmed (base) : base;
        }

        RigModel& model;
        const juce::uint64 slotId;
        const bool branchActive;            // 블록이 있는 갈래가 현재 씬에서 켜져 있는지 (본 줄은 항상 true)
        const juce::uint32& marqueeStart;   // ChainView 소유: 타일을 다시 만들어도 흘러가던 위치가 이어진다
        const StompDrag& stompDrag;          // ChainView 소유
        juce::Image snapshot;                // 끄기 시작할 때 블록 그림 (표시를 바꿀 때 다시 쓴다)
        block::DragMark shownMark = block::DragMark::none;
        bool shownStomp = false;             // 지금 끄는 그림이 Stomp 칸 모양인지
    };

    //==============================================================================
    // 병렬의 합류(Merge) 버튼. 클릭 영역(컴포넌트)은 mergeHitSize 정사각형.
    //  - 클릭(끌지 않고 뗌) = 합류 선택 (파라미터 바에 갈래 On/Off·레벨)
    //  - 끌기 = 합류 지점 옮기기 / 이어붙이기 (Strip 이 놓을 자리를 보여주고, 자리 위에서 뗄 때만 동작)
    //  - 마우스를 올리면 mergeHoverScale 까지 커진다 (화면 갱신마다 조금씩)
    class ChainView::MergeButton final : public juce::Component
    {
    public:
        MergeButton (RigModel& m, juce::uint64 id) : model (m), parallelId (id) {}

        std::function<void (juce::Point<int>)> onDragMove, onDragEnd;   // Strip 좌표

        void paint (juce::Graphics& g) override
        {
            drawMergeButton (g, *this, getLocalBounds().toFloat().getCentre(), mergeSize * scale, hovered,
                             model.getSelectedParallelId() == parallelId);
        }

        void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { hovered = false; repaint(); }
        void mouseDown (const juce::MouseEvent&) override  { dragging = false; }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! isLeftClick (e) || (! dragging && e.getDistanceFromDragStart() < dragStartDistance))
                return;

            dragging = true;
            if (onDragMove != nullptr)
                onDragMove (e.getEventRelativeTo (getParentComponent()).getPosition());
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! isLeftClick (e))
                return;

            if (! dragging)
                model.selectParallel (parallelId);
            else if (onDragEnd != nullptr)
                onDragEnd (e.getEventRelativeTo (getParentComponent()).getPosition());

            dragging = false;
        }

    private:
        // 커지기 / 작아지기: 목표(hovered)를 향해 mergeAnimSeconds 동안 선형으로. 목표에 닿으면 아무 일도 안 한다
        void animate (double now)
        {
            const double dt = lastFrame > 0.0 ? now - lastFrame : 0.0;
            lastFrame = now;

            const float target = hovered ? mergeHoverScale : 1.0f;
            if (juce::approximatelyEqual (scale, target))
                return;

            const auto delta = (float) (dt / mergeAnimSeconds) * (mergeHoverScale - 1.0f);
            scale = target > scale ? juce::jmin (target, scale + delta) : juce::jmax (target, scale - delta);
            repaint();
        }

        RigModel& model;
        const juce::uint64 parallelId;
        bool hovered = false, dragging = false;
        float scale = 1.0f;
        double lastFrame = 0.0;
        juce::VBlankAttachment vblank { this, [this] (double now) { animate (now); } };
    };

    //==============================================================================
    // 뷰포트 안에서 가로로 늘어나는 내용물: IN, 블록들, OUT 노드와 그 사이 연결선.
    // 병렬은 위 갈래를 본 줄 높이에, 아래 갈래를 그 아래 줄에 그린다. 갈라지는 곳(Split)은 선만, 합쳐지는 곳 = 합류(Merge) 버튼.
    // 짧은 갈래는 선이 Merge 까지 늘어난다.
    //
    // 끌어 놓기 (선택기 항목 또는 패치맵 블록). 놓을 자리를 희미하게 미리 보여준다
    //  - 연결선 위   : 그 자리에 넣기 (패치맵 블록이면 이동)
    //  - 블록 위     : 선택기 항목 = 그 블록의 파일 교체, 패치맵 블록 = 두 블록 자리 맞바꾸기 (Swap)
    //  - 본 줄 블록 바로 아래 : 그 블록과 병렬 (위 갈래 = 그 블록, 아래 갈래 = 놓은 것). 갈래 안 블록 아래는 놓을 자리 아님
    //  - 그 밖 / Esc : 취소. 제자리에 놓으면 아무 일도 없다. 패치맵 블록을 자리 밖, 원래 블록 + 사방 keepMargin 밖에서 떼면 삭제
    class ChainView::Strip final : public juce::Component,
                                   public juce::DragAndDropTarget
    {
    public:
        Strip (RigModel& m, bool inFailed, bool outFailed, const juce::uint32& startMs, const StompDrag& stomp)
            : model (m), inputFailed (inFailed), outputFailed (outFailed), marqueeStart (startMs), stompDrag (stomp)
        {
            // 넣을 자리의 순서 = resized() 에서 연결선을 배치하는 순서
            const auto& chain = model.getChain();

            for (int i = 0; i < (int) chain.size(); ++i)
            {
                insertPlaces.push_back ({ i, -1, 0 });

                if (const auto* slot = std::get_if<RigModel::Slot> (&chain[(size_t) i]))
                {
                    items.push_back ({ addTile (slot->id, true), -1 });
                    continue;
                }

                const auto& parallel = std::get<RigModel::Parallel> (chain[(size_t) i]);
                ParallelView view;
                view.id = parallel.id;
                const auto index = (int) parallels.size();
                view.item = (int) items.size();
                view.button = mergeButtons.add (new MergeButton (model, parallel.id));
                view.button->onDragMove = [this, index] (juce::Point<int> p) { mergeDragging = index; mergeDragPos = p; setMergeTarget (index, p); repaint(); };
                view.button->onDragEnd  = [this, index, id = parallel.id] (juce::Point<int> p)
                {
                    setMergeTarget (index, p);
                    const auto target = mergeTarget;
                    mergeTarget.reset();
                    mergeDragging = -1;
                    repaint();

                    if (! target.has_value())
                        return;   // 놓을 자리 밖: 아무 일도 없다

                    if (target->join)
                        model.joinParallel (id);
                    else
                        model.moveMerge (id, target->delta);
                };
                addAndMakeVisible (view.button);

                for (int b = 0; b < 2; ++b)
                {
                    const auto& branch = parallel.branches[(size_t) b];
                    view.active[(size_t) b] = model.isBranchOn (parallel, b);

                    for (int k = 0; k < (int) branch.size(); ++k)
                    {
                        insertPlaces.push_back ({ i, b, k });
                        view.tiles[(size_t) b].push_back (addTile (branch[(size_t) k].id, view.active[(size_t) b]));
                    }

                    insertPlaces.push_back ({ i, b, (int) branch.size() });
                }

                items.push_back ({ nullptr, (int) parallels.size() });
                parallels.push_back (std::move (view));
                hasParallel = true;
            }

            insertPlaces.push_back ({ (int) chain.size(), -1, 0 });
        }

        int idealWidth() const
        {
            int width = margin + endWidth;

            for (const auto& item : items)
                width += item.tile != nullptr ? gap + tileWidth : gap + parallelWidth (parallels[(size_t) item.parallel]);

            return width + gap + endWidth + margin;
        }

        juce::Rectangle<int> selectedTileBounds() const
        {
            auto* t = selectedTile();
            return t != nullptr ? t->getBounds() : juce::Rectangle<int>();
        }

        // 선택된 블록의 파일명이 넘쳐서 흘러가야 하는지
        bool selectedNameOverflows() const
        {
            auto* t = selectedTile();
            return t != nullptr && t->nameOverflow() > 0.0f;
        }

        void repaintSelectedTile()
        {
            if (auto* t = selectedTile())
                t->repaint (t->nameArea());
        }

        // IN / OUT 노드 클릭 = 선택 (파라미터 바에 채널 선택이 나온다). 빈 바탕 클릭 = 선택 해제 (mouseUp).
        // 병렬 갈래의 블록→블록 화살표를 눌러 같은 병렬의 다른 줄 블록→블록 화살표로 끌어 놓으면 그 자리에서 병렬을 둘로 나눈다
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! isLeftClick (e))
                return;

            splitFrom = splitEdgeAt (e.getPosition());
            splitTo.reset();

            if (inNode().contains (e.getPosition()))
                model.selectEndpoint (RigModel::Endpoint::input);
            else if (outNode().contains (e.getPosition()))
                model.selectEndpoint (RigModel::Endpoint::output);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! splitFrom.has_value() || ! isLeftClick (e))
                return;

            splitCursor = e.getPosition();
            const auto edge = splitEdgeAt (e.getPosition());
            splitTo = edge.has_value() && edge->parallel == splitFrom->parallel && edge->branch != splitFrom->branch
                    ? edge : std::nullopt;
            repaint();
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            const auto from = splitFrom, to = splitTo;
            splitFrom.reset();
            splitTo.reset();
            repaint();

            if (! isLeftClick (e))
                return;

            // 빈 바탕 클릭 (연결선을 끌지 않고 뗀 것 포함) = 선택 해제. IN / OUT 은 누를 때 이미 골랐다
            if (! e.mouseWasDraggedSinceMouseDown())
            {
                if (! inNode().contains (e.getPosition()) && ! outNode().contains (e.getPosition()))
                    model.selectSlot (0);

                return;
            }

            if (! from.has_value() || ! to.has_value())
                return;   // 다른 줄 화살표 위가 아니면 아무 일도 없다

            const auto& upper = from->branch == 0 ? *from : *to;
            const auto& lower = from->branch == 0 ? *to : *from;
            model.splitParallel (upper.parallelId, upper.count, lower.count);
        }

        void paint (juce::Graphics& g) override
        {
            const auto selected = model.getSelectedEndpoint();
            const auto& routing = model.getRouting();
            drawEndNode (g, inNode(), "IN", channelText (routing.input, 1),
                         selected == RigModel::Endpoint::input, inputFailed);
            drawEndNode (g, outNode(), "OUT", channelText (routing.output, routing.stereoOutput ? 2 : 1),
                         selected == RigModel::Endpoint::output, outputFailed);

            // 연결선: 켜진 선 = 흰 실선, 꺼진 갈래의 선 = 어두운 회색 점선. 블록 / OUT 으로 들어가는 선에만 화살표
            for (const auto& line : lines)
            {
                if (! line.active)
                {
                    const float dashes[] = { 4.0f, 4.0f };
                    g.setColour (inactiveLine);
                    g.drawDashedLine (line.line, dashes, 2, lineThickness);
                }
                else if (line.arrow)
                {
                    g.setColour (activeLine);
                    g.drawArrow (line.line, lineThickness, 8.0f, 8.0f);
                }
                else
                {
                    g.setColour (activeLine);
                    g.drawLine (line.line, lineThickness);
                }
            }

            // 병렬 나누기에 쓰는 갈래 블록→블록 화살표 가운데 = 연결점 (채운 작은 원: 눌러 다른 줄로 이을 수 있다는 표시)
            for (const auto& edge : splitEdges)
            {
                g.setColour (parallels[(size_t) edge.parallel].active[(size_t) edge.branch] ? activeLine : inactiveLine);
                g.fillEllipse (juce::Rectangle<float> (connectorSize, connectorSize).withCentre (edge.area.toFloat().getCentre()));
            }
        }

        void resized() override
        {
            lines.clear();

            dropZones.clear();

            const float row0 = (float) rowY (0) + (float) tileHeight * 0.5f, row1 = (float) rowY (1) + (float) tileHeight * 0.5f;
            size_t nextPlace = 0;

            // 연결선 한 칸: 놓을 자리 = x ~ to 의 줄 높이 전체, 선은 x + 2 ~ to (블록 앞이면 화살표 끝을 2 px 띄운다).
            // 아래 갈래의 ㄴ / ㄱ 모서리는 세로선과 가로선을 선 두께의 절반씩 겹쳐 끊기지 않게 한다 (from, to 를 직접 준다)
            const float half = lineThickness * 0.5f;

            const auto segment = [&] (int x, int row, float to, bool arrow, bool active, float from = -1.0f)
            {
                dropZones.push_back ({ { x, rowY (row), juce::roundToInt (to) - x, tileHeight }, DropZone::insert, insertPlaces[nextPlace++], 0, {} });

                const float mid = row == 0 ? row0 : row1;
                lines.push_back ({ { from >= 0.0f ? from : (float) x + 2.0f, mid, to - (arrow ? 2.0f : 0.0f), mid }, active, arrow });
            };

            int x = margin + endWidth;

            for (const auto& item : items)
            {
                if (item.tile != nullptr)
                {
                    segment (x, 0, (float) (x + gap), true, true);
                    item.tile->setBounds (x + gap, rowY (0), tileWidth, tileHeight);

                    // 본 줄 블록 바로 아래 = 병렬 (아래 갈래 줄 자리)
                    const auto tile = item.tile->getBounds();
                    dropZones.push_back ({ tile.translated (0, tileHeight + rowGap), DropZone::parallel, {}, item.tile->getSlotId(), tile });
                    x += gap + tileWidth;
                    continue;
                }

                // 병렬: 본 줄 선이 Split 점에서 끝나고, 갈래마다 블록을 지나 Merge 점까지
                const auto& view = parallels[(size_t) item.parallel];
                const int split = x + gap;
                const int merge = split + parallelWidth (view);
                segment (x, 0, (float) split, false, true);

                for (size_t b = 0; b < 2; ++b)
                {
                    const int row = (int) b;
                    const bool active = view.active[b];
                    int bx = split;

                    // 아래 갈래: Split 에서 내려와(ㄴ) 블록들을 지나 Merge 로 올라간다(ㄱ)
                    if (row == 1)
                        lines.push_back ({ { (float) split, row0, (float) split, row1 + half }, active, false });

                    for (size_t k = 0; k < view.tiles[b].size(); ++k)
                    {
                        // 갈래 첫 칸은 Split 에서 바로 이어진다 (점이 없으므로 띄우지 않는다)
                        segment (bx, row, (float) (bx + gap), true, active, k == 0 ? (float) split - (row == 1 ? half : 0.0f) : -1.0f);
                        view.tiles[b][k]->setBounds (bx + gap, rowY (row), tileWidth, tileHeight);
                        bx += gap + tileWidth;
                    }

                    segment (bx, row, row == 1 ? (float) merge + half : (float) merge, false, active);

                    if (row == 1)
                        lines.push_back ({ { (float) merge, row1 + half, (float) merge, row0 }, active, false });
                }

                view.button->setBounds (juce::Rectangle<int> (mergeHitSize, mergeHitSize).withCentre ({ merge, juce::roundToInt (row0) }));
                parallels[(size_t) item.parallel].mergeX = merge;
                x = merge;
            }

            segment (x, 0, (float) (x + gap), true, true);

            layoutMergeZones();
            layoutSplitEdges();

            for (auto* tile : tiles)
                dropZones.push_back ({ tile->getBounds(), DropZone::replace, {}, tile->getSlotId(), tile->getBounds() });

            repaint();
        }

        //==============================================================================
        bool isInterestedInDragSource (const SourceDetails& details) override
        {
            return readDrag (details.description).has_value();
        }

        void itemDragEnter (const SourceDetails& details) override
        {
            activeDrag = readDrag (details.description);   // 끄는 동안 놓을 수 있는 자리를 전부 희미하게
            libraryMark = block::DragMark::add;
            itemDragMove (details);
            repaint();   // 처음 자리가 놓을 자리가 아니어도(자기 블록 위) 힌트는 바로 그린다
        }

        void itemDragMove (const SourceDetails& details) override
        {
            if (auto* scroller = findParentComponentOfClass<juce::Viewport>())
            {
                const auto p = scroller->getLocalPoint (this, details.localPosition);
                scroller->autoScroll (p.x, p.y, autoScrollBorder, autoScrollSpeed);   // 세로 스크롤은 없으므로 가로만 움직인다
            }

            setHover (zoneAt (details));

            // 선택기 항목: 기존 블록 위(교체)면 되돌아오는 화살표, 그 밖은 + (끌기 시작부터)
            if (activeDrag.has_value() && activeDrag->fromLibrary)
                showLibraryMark (hover.has_value() && hover->kind == DropZone::replace ? block::DragMark::replace : block::DragMark::add);
        }

        void itemDragExit (const SourceDetails&) override
        {
            if (activeDrag.has_value() && activeDrag->fromLibrary)
                showLibraryMark (block::DragMark::add);

            activeDrag.reset();
            setHover (std::nullopt);
            repaint();
        }

        void itemDropped (const SourceDetails& details) override
        {
            const auto zone = zoneAt (details);
            const auto drag = readDrag (details.description);
            activeDrag.reset();
            setHover (std::nullopt);
            repaint();

            if (! zone.has_value() || ! drag.has_value())
                return;

            if (drag->fromLibrary)
            {
                switch (zone->kind)
                {
                    case DropZone::insert:   model.insertBlock (zone->place, drag->type, drag->file); break;
                    case DropZone::replace:  model.assignBlock (zone->slotId, drag->type, drag->file); break;
                    case DropZone::parallel: model.addParallel (zone->slotId, drag->type, drag->file); break;
                }

                return;
            }

            if (zone->kind == DropZone::insert)
                model.moveBlock (drag->slotId, zone->place);
            else if (zone->kind == DropZone::parallel)
                model.moveBlockBelow (drag->slotId, zone->slotId);
            else
                model.swapBlocks (drag->slotId, zone->slotId);
        }

        // 놓을 자리 / 옮길 합류 지점 미리보기 (블록 위에), Undo / Redo 로 되돌아간 블록·합류 버튼·IN / OUT 강조
        void paintOverChildren (juce::Graphics& g) override
        {
            if (const auto alpha = highlightAlpha (model.getHighlightStart()); alpha > 0.0f)
            {
                const auto& touched = model.getHighlight();
                const auto has = [] (const auto& list, auto value) { return std::find (list.begin(), list.end(), value) != list.end(); };

                for (auto* tile : tiles)
                    if (has (touched.slots, tile->getSlotId()))
                        drawHighlight (g, tile->getBounds().toFloat(), alpha);

                for (const auto& view : parallels)
                    if (has (touched.parallels, view.id))
                        drawHighlight (g, juce::Rectangle<float> (mergeSize, mergeSize).withCentre (view.button->getBounds().toFloat().getCentre()), alpha, 6.0f);

                if (touched.endpoint != RigModel::Endpoint::none)
                    drawHighlight (g, (touched.endpoint == RigModel::Endpoint::input ? inNode() : outNode()).toFloat(), alpha);
            }

            paintDropHints (g);

            if (mergeTarget.has_value())
            {
                const float row0 = (float) rowY (0) + (float) tileHeight * 0.5f, row1 = (float) rowY (1) + (float) tileHeight * 0.5f;
                const float half = lineThickness * 0.5f;
                g.beginTransparencyLayer (dropHighlight.getFloatAlpha());
                g.setColour (activeLine);

                if (mergeTarget->join)
                {
                    // 이어붙이기: 놓을 자리(아래 줄 빈 공간) + 이어질 선 (앞 병렬 갈래 끝 → 뒤 병렬 갈래 첫 블록)
                    const auto& view = parallels[(size_t) mergeTarget->parallel];
                    const auto& next = parallels[(size_t) mergeTarget->parallel + 1];

                    for (size_t b = 0; b < 2; ++b)
                    {
                        const float y = b == 0 ? row0 : row1;
                        g.drawArrow ({ (float) view.tiles[b].back()->getRight(), y, (float) next.tiles[b].front()->getX() - 2.0f, y },
                                     lineThickness, 8.0f, 8.0f);
                    }
                }
                else
                {
                    // 합류 버튼이 갈 자리: 희미한 버튼 + 아래 갈래가 올라올 세로선.
                    // 오른쪽으로 옮기면 아래 갈래 선이 지금 합류 자리에서 새 자리까지 늘어난다 (ㄱ 모서리는 선 두께 절반 겹침)
                    const auto x = (float) mergeTarget->x;
                    const auto current = (float) parallels[(size_t) mergeTarget->parallel].mergeX;

                    if (x > current)
                        g.drawLine (current, row1, x + half, row1, lineThickness);

                    g.drawLine (x, row0, x, row1 + half, lineThickness);
                    drawMergeButton (g, *this, { x, row0 }, mergeSize, false, false);
                }

                g.endTransparencyLayer();

                g.setColour (dropHover);   // 커서가 올라간 놓을 자리
                g.drawRoundedRectangle (mergeTarget->area.toFloat().reduced (2.0f), block::corner, hoverThickness);
            }

            if (splitFrom.has_value())
            {
                // 병렬 나누기: 누른 화살표 강조 + 커서까지 선. 다른 줄 화살표 위면 두 화살표를 잇는 세로선 + 새 합류 버튼 자리
                const float row0 = (float) rowY (0) + (float) tileHeight * 0.5f, row1 = (float) rowY (1) + (float) tileHeight * 0.5f;
                g.beginTransparencyLayer (dropHighlight.getFloatAlpha());
                g.setColour (activeLine);
                g.drawRoundedRectangle (splitFrom->area.toFloat().reduced (2.0f), block::corner, lineThickness);

                if (splitTo.has_value())
                {
                    const auto& upper = splitFrom->branch == 0 ? *splitFrom : *splitTo;
                    const auto& lower = splitFrom->branch == 0 ? *splitTo : *splitFrom;
                    const float ux = (float) upper.area.getCentreX(), lx = (float) lower.area.getCentreX();
                    g.drawLine (ux, row0, ux, row1, lineThickness);
                    g.drawLine (juce::jmin (ux, lx), row1, juce::jmax (ux, lx), row1, lineThickness);
                    drawMergeButton (g, *this, { ux, row0 }, mergeSize, false, false);
                }
                else
                {
                    g.drawLine (splitFrom->area.toFloat().getCentre().x, splitFrom->area.toFloat().getCentre().y,
                                (float) splitCursor.x, (float) splitCursor.y, lineThickness);
                }

                g.endTransparencyLayer();

                if (splitTo.has_value())
                {
                    g.setColour (dropHover);   // 커서가 올라간 놓을 자리
                    g.drawRoundedRectangle (splitTo->area.toFloat().reduced (2.0f), block::corner, hoverThickness);
                }
            }

            // 끄는 합류 버튼은 블록처럼 커서를 따라간다
            if (mergeDragging >= 0)
            {
                g.beginTransparencyLayer (block::dragImageAlpha);
                drawMergeButton (g, *this, mergeDragPos.toFloat(), mergeSize, true, false);
                g.endTransparencyLayer();
            }

            if (! hover.has_value())
                return;

            g.setColour (dropHover);   // 커서가 올라간 놓을 자리 = 진한 테두리

            if (hover->kind == DropZone::insert)
            {
                g.drawRoundedRectangle (hover->area.toFloat().reduced (2.0f), block::corner, hoverThickness);
                return;
            }

            if (hover->kind == DropZone::replace)
            {
                g.drawRoundedRectangle (hover->area.toFloat().expanded (2.0f), block::corner, hoverThickness);

                // Swap (패치맵 블록): 원래 자리에도 대상 블록이 들어올 모습을 반투명하게
                if (activeDrag.has_value() && ! activeDrag->fromLibrary)
                {
                    const auto* source = tileFor (activeDrag->slotId);
                    auto* target = tileFor (hover->slotId);

                    if (source != nullptr && target != nullptr)
                    {
                        // 원래 블록 위에 겹쳐 글자가 섞이지 않게 그 자리를 패치맵 바탕으로 덮은 뒤 그린다
                        g.setColour (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.15f));   // ChainView::paint 와 같은 바탕
                        g.fillRect (source->getBounds());
                        g.setOpacity (block::dragImageAlpha);
                        g.drawImageAt (target->createComponentSnapshot (target->getLocalBounds()), source->getX(), source->getY());
                        g.setColour (dropHighlight);
                        g.drawRoundedRectangle (source->getBounds().toFloat().expanded (2.0f), block::corner, 3.0f);
                    }
                }

                return;
            }

            // 병렬: 블록 앞뒤 연결선 가운데에서 갈라져 아래로 내려갔다 다시 합쳐지는 모습 + 아래 갈래 블록 자리
            const auto target = hover->target.toFloat();
            const auto lower = hover->area.toFloat();
            const float split = target.getX() - (float) gap * 0.5f, merge = target.getRight() + (float) gap * 0.5f;

            juce::Path path;
            path.startNewSubPath (split, target.getCentreY());
            path.lineTo (split, lower.getCentreY());
            path.lineTo (lower.getX(), lower.getCentreY());
            path.startNewSubPath (lower.getRight(), lower.getCentreY());
            path.lineTo (merge, lower.getCentreY());
            path.lineTo (merge, target.getCentreY());
            g.setColour (dropHighlight);
            g.strokePath (path, juce::PathStrokeType (lineThickness));

            g.setColour (dropHover);
            g.drawRoundedRectangle (lower.reduced (1.5f), block::corner, hoverThickness);

            g.beginTransparencyLayer (dropHighlight.getFloatAlpha());
            drawMergeButton (g, *this, { merge, target.getCentreY() }, mergeSize, false, false);
            g.endTransparencyLayer();
        }

    private:
        struct Item
        {
            Tile* tile = nullptr;   // 본 줄 블록이면
            int parallel = -1;      // 병렬이면 parallels 번호
        };

        struct ParallelView
        {
            std::array<std::vector<Tile*>, 2> tiles;
            std::array<bool, 2> active { true, true };
            MergeButton* button = nullptr;
            juce::uint64 id = 0;    // 병렬 id
            int item = -1;          // items 안의 번호
            int mergeX = 0;         // 지금 합류 버튼 x (resized 에서)
        };

        // 합류 버튼을 놓을 자리 (resized 에서 계산). 이 자리 위에서만 미리보기가 뜨고 동작한다
        //  - 위 갈래 안 연결선 (첫 블록 앞 제외) : 그 뒤 위 갈래 블록을 합류 뒤 본 줄로 (delta < 0)
        //  - 병렬 뒤 본 줄 연결선 (다음 병렬 전까지) : 사이 본 줄 블록을 위 갈래 끝으로 (delta > 0)
        //  - 아래 줄 빈 공간 (바로 뒤가 병렬일 때, 합류 세로선 ~ 다음 갈림 세로선 사이) : 이어붙이기
        // 아래 갈래 안, 다음 병렬 안 / 너머는 자리가 아니다 (병렬끼리 겹치거나 품게 된다)
        struct MergeTarget
        {
            juce::Rectangle<int> area;
            int parallel = -1, delta = 0, x = 0;   // x = 옮길 합류 버튼 자리 (delta 일 때)
            bool join = false;
        };

        void layoutMergeZones()
        {
            mergeZones.clear();

            for (int p = 0; p < (int) parallels.size(); ++p)
            {
                const auto& view = parallels[(size_t) p];
                const auto& upper = view.tiles[0];

                for (size_t k = 1; k < upper.size(); ++k)
                {
                    const int left = upper[k - 1]->getRight();
                    mergeZones.push_back ({ { left, rowY (0), gap, tileHeight }, p, (int) k - (int) upper.size(), left + gap / 2, false });
                }

                size_t j = (size_t) view.item + 1;

                for (; j < items.size() && items[j].tile != nullptr; ++j)
                {
                    const int left = items[j].tile->getRight();
                    mergeZones.push_back ({ { left, rowY (0), gap, tileHeight }, p, (int) (j - (size_t) view.item), left + gap / 2, false });
                }

                // 바로 뒤가 병렬: 아래 줄에서 이 합류의 세로선과 다음 병렬 갈림의 세로선 사이 빈 공간 (gap 폭)
                if (j == (size_t) view.item + 1 && j < items.size())
                    mergeZones.push_back ({ { view.mergeX, rowY (1), gap, tileHeight }, p, 0, 0, true });
            }
        }

        // 병렬 나누기에 쓰는 화살표: 갈래 안 블록→블록 연결선 (갈림 → 첫 블록, 마지막 블록 → 합류는 아니다). count = 그 화살표 앞 블록 수
        struct SplitEdge
        {
            juce::Rectangle<int> area;
            juce::uint64 parallelId = 0;
            int parallel = -1, branch = 0, count = 0;
        };

        void layoutSplitEdges()
        {
            splitEdges.clear();

            for (int p = 0; p < (int) parallels.size(); ++p)
            {
                const auto& view = parallels[(size_t) p];

                for (int b = 0; b < 2; ++b)
                {
                    const auto& row = view.tiles[(size_t) b];

                    for (size_t k = 1; k < row.size(); ++k)
                        splitEdges.push_back ({ { row[k - 1]->getRight(), rowY (b), gap, tileHeight }, view.id, p, b, (int) k });
                }
            }
        }

        std::optional<SplitEdge> splitEdgeAt (juce::Point<int> position) const
        {
            for (const auto& edge : splitEdges)
                if (hitArea (edge.area).contains (position))
                    return edge;

            return std::nullopt;
        }

        void setMergeTarget (int index, juce::Point<int> position)
        {
            std::optional<MergeTarget> target;

            for (const auto& zone : mergeZones)
                if (zone.parallel == index && hitArea (zone.area).contains (position))
                    target = zone;

            const auto same = target.has_value() == mergeTarget.has_value()
                           && (! target.has_value() || (target->area == mergeTarget->area && target->parallel == mergeTarget->parallel));

            if (same)
                return;

            mergeTarget = target;
            repaint();
        }

        struct Line
        {
            juce::Line<float> line;
            bool active = true, arrow = false;
        };

        struct DropZone
        {
            juce::Rectangle<int> area;
            enum Kind { insert, replace, parallel } kind = insert;
            RigModel::Place place;              // insert: 넣을 자리
            juce::uint64 slotId = 0;            // replace: 교체할 블록, parallel: 위 갈래가 될 본 줄 블록
            juce::Rectangle<int> target;        // replace / parallel: 그 블록의 자리
        };

        // 끄는 것과 위치로 놓을 자리를 고른다. 그 끌기에 맞지 않거나 아무 일도 없는 자리는 없음
        std::optional<DropZone> zoneAt (const SourceDetails& details) const
        {
            const auto drag = readDrag (details.description);
            if (! drag.has_value())
                return std::nullopt;

            for (const auto& zone : dropZones)
                if (hitArea (zone.area).contains (details.localPosition))
                    return isValidZone (zone, *drag) ? std::optional<DropZone> (zone) : std::nullopt;

            return std::nullopt;
        }

        // 그 끌기로 놓을 수 있는 자리인지 (아무 일도 없는 자리는 아니다)
        bool isValidZone (const DropZone& zone, const DragItem& drag) const
        {
            if (drag.fromLibrary)
                return true;

            const auto from = model.findPlace (drag.slotId);
            if (! from.has_value())
                return false;

            // 패치맵 블록: 다른 블록 위 = Swap, 본 줄 블록 아래 = 병렬. 자기 자신은 자리 아님
            if (zone.kind == DropZone::replace || zone.kind == DropZone::parallel)
                return zone.slotId != drag.slotId;

            // 같은 목록에서 바로 앞이나 바로 뒤 자리 = 제자리
            const bool sameList = from->branch < 0 ? zone.place.branch < 0
                                                   : zone.place.branch == from->branch && zone.place.item == from->item;
            const int position = from->branch < 0 ? from->item : from->index;
            const int target = zone.place.branch < 0 ? zone.place.item : zone.place.index;

            return ! (sameList && (target == position || target == position + 1));
        }

        // 패치맵 블록 id 를 화면 좌표 screen 에 놓을 때의 놓을 자리 (패치맵에 보이는 영역 안에서만)
        std::optional<DropZone> dropZoneAt (juce::uint64 id, juce::Point<int> screen) const
        {
            const auto* scroller = findParentComponentOfClass<juce::Viewport>();

            if (scroller == nullptr || ! scroller->getScreenBounds().contains (screen))
                return std::nullopt;

            return zoneAt ({ block::mapDrag (id), nullptr, getLocalPoint (nullptr, screen) });
        }

        // 놓을 자리가 반응하는 범위: 그 자리(미리보기 위치)에서 세로로 넓힌다. 바깥쪽(본 줄 위 / 아래 줄 아래)은 dropMargin,
        // 본 줄과 아래 줄 사이는 간격(rowGap)의 절반씩 나눠 가진다. 가로는 그대로 (선 구간과 블록이 이미 빈틈없이 이어져 있다)
        juce::Rectangle<int> hitArea (juce::Rectangle<int> area) const
        {
            const bool upperRow = area.getY() == rowY (0);
            return area.withTop (area.getY() - (upperRow ? dropMargin : rowGap / 2))
                       .withBottom (area.getBottom() + (upperRow ? rowGap / 2 : dropMargin));
        }

        void setHover (std::optional<DropZone> zone)
        {
            const auto same = [] (const std::optional<DropZone>& a, const std::optional<DropZone>& b)
            {
                return a.has_value() == b.has_value() && (! a.has_value() || (a->area == b->area && a->kind == b->kind));
            };

            if (same (hover, zone))
                return;

            hover = zone;
            repaint();
        }

        // 선택기 항목을 끄는 그림의 표시를 바꾼다 (같으면 그대로)
        void showLibraryMark (block::DragMark mark)
        {
            if (mark == libraryMark || ! activeDrag.has_value())
                return;

            libraryMark = mark;

            // 끌기가 끝나는 중(끄는 그림이 지워지며 itemDragExit 가 불린다)이면 바꿀 그림이 없다
            if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this); container != nullptr && container->isDragAndDropActive())
                container->setCurrentDragImage (juce::ScaledImage (block::createImage (activeDrag->type, activeDrag->file.getFileNameWithoutExtension(), mark)));
        }

        Tile* tileFor (juce::uint64 id) const
        {
            for (auto* t : tiles)
                if (t->getSlotId() == id)
                    return t;

            return nullptr;
        }

        // 끄는 동안 놓을 수 있는 자리 전부 (아주 희미한 테두리). 올라간 자리는 위 미리보기가 진하게 덮는다
        //  - 선택기 항목 / 패치맵 블록 (activeDrag): dropZones 중 그 끌기로 놓을 수 있는 곳
        //  - 합류 버튼 (mergeDragging): 그 병렬의 mergeZones
        //  - 병렬 나누기 (splitFrom): 같은 병렬 다른 줄의 화살표
        void paintDropHints (juce::Graphics& g) const
        {
            g.setColour (dropHint);
            const auto outline = [&] (juce::Rectangle<int> area) { g.drawRoundedRectangle (area.toFloat().reduced (2.0f), block::corner, lineThickness); };

            if (activeDrag.has_value())
                for (const auto& zone : dropZones)
                    if (isValidZone (zone, *activeDrag))
                        outline (zone.kind == DropZone::replace ? zone.area.expanded (4) : zone.area);

            if (mergeDragging >= 0)
                for (const auto& zone : mergeZones)
                    if (zone.parallel == mergeDragging)
                        outline (zone.area);

            if (splitFrom.has_value())
                for (const auto& edge : splitEdges)
                    if (edge.parallel == splitFrom->parallel && edge.branch != splitFrom->branch)
                        outline (edge.area);
        }

        Tile* addTile (juce::uint64 id, bool active)
        {
            auto* tile = tiles.add (new Tile (model, id, active, marqueeStart, stompDrag));
            tile->isOverDropZone = [this, id] (juce::Point<int> screen) { return dropZoneAt (id, screen).has_value(); };
            tile->isOverSwap = [this, id] (juce::Point<int> screen) { const auto zone = dropZoneAt (id, screen); return zone.has_value() && zone->kind == DropZone::replace; };
            addAndMakeVisible (tile);
            return tile;
        }

        // Split ~ Merge 폭: 긴 갈래 기준 (블록 n 개 = 앞 연결선 n 칸 + 블록 n 개 + 마지막 연결선)
        static int parallelWidth (const ParallelView& view)
        {
            const auto width = [] (size_t n) { return (int) n * (gap + tileWidth) + gap; };
            return juce::jmax (width (view.tiles[0].size()), width (view.tiles[1].size()));
        }

        // 줄의 위쪽 y. 병렬이 있으면 두 줄 묶음을, 없으면 한 줄을 세로 가운데에
        int rowY (int row) const
        {
            const int rows = hasParallel ? 2 : 1;
            const int top = (getHeight() - (rows * tileHeight + (rows - 1) * rowGap)) / 2;
            return top + row * (tileHeight + rowGap);
        }

        Tile* selectedTile() const
        {
            for (auto* t : tiles)
                if (t->getSlotId() == model.getSelectedSlotId())
                    return t;

            return nullptr;
        }

        juce::Rectangle<int> inNode() const  { return { margin, rowY (0), endWidth, tileHeight }; }
        juce::Rectangle<int> outNode() const { return { idealWidth() - margin - endWidth, rowY (0), endWidth, tileHeight }; }

        // 블록 양식 그대로: 머리띠 "IN" / "OUT" (가운데), 본문 = 채널 번호 하나를 가로·세로 가운데에. 장치에 없는 채널 = 블록의 로딩 실패와 같은 오류 표시
        static void drawEndNode (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
                                 const juce::String& channels, bool selected, bool failed)
        {
            drawBlockBackground (g, area, block::endpointFill);
            drawBlockHeader (g, area, title, block::endpointBand, juce::Colours::white, juce::Justification::centred);
            g.setColour (juce::Colours::white);
            g.setFont (block::nameFont());
            g.drawText (channels, block::body (area), juce::Justification::centred, true);   // 넘치면 말줄임
            drawBlockFrame (g, area, block::endpointBand, selected, failed);
        }

        RigModel& model;
        const bool inputFailed, outputFailed;
        const juce::uint32& marqueeStart;   // ChainView 소유 (타일에 넘긴다)
        const StompDrag& stompDrag;         // ChainView 소유 (타일에 넘긴다)
        bool hasParallel = false;
        std::vector<Item> items;                 // 본 줄 순서
        std::vector<ParallelView> parallels;
        juce::OwnedArray<Tile> tiles;
        juce::OwnedArray<MergeButton> mergeButtons;
        std::vector<RigModel::Place> insertPlaces;   // 연결선 순서 (resized 의 배치 순서와 같다)
        std::vector<Line> lines;                     // resized 에서 계산
        std::vector<DropZone> dropZones;             // resized 에서 계산
        std::optional<DropZone> hover;               // 끄는 동안 놓을 자리
        std::vector<MergeTarget> mergeZones;         // resized 에서 계산
        std::vector<SplitEdge> splitEdges;           // resized 에서 계산
        std::optional<SplitEdge> splitFrom, splitTo; // 병렬 나누기: 누른 화살표 / 지금 놓을 화살표 (다른 줄, 같은 병렬)
        juce::Point<int> splitCursor;
        std::optional<MergeTarget> mergeTarget;      // 합류 버튼을 끄는 동안 놓을 자리 (자리 밖이면 없음)
        int mergeDragging = -1;                      // 끌고 있는 합류 버튼의 병렬 번호 (놓을 자리 전부 희미하게)
        juce::Point<int> mergeDragPos;               // 끄는 합류 버튼이 따라가는 커서 위치
        std::optional<DragItem> activeDrag;          // 패치맵 위로 끌고 있는 선택기 항목 / 블록 (놓을 자리 전부 희미하게)
        block::DragMark libraryMark = block::DragMark::add;   // 선택기 항목을 끄는 그림의 지금 표시
    };

    //==============================================================================
    ChainView::ChainView (RigModel& m, juce::AudioDeviceManager& dm) : model (m), deviceManager (dm)
    {
        viewport.setScrollBarsShown (false, true);
        addAndMakeVisible (viewport);

        errorLabel.setColour (juce::Label::textColourId, juce::Colours::red.brighter (0.2f));
        warningLabel.setColour (juce::Label::textColourId, juce::Colours::yellow);

        for (auto* label : { &errorLabel, &warningLabel })
        {
            label->setJustificationType (juce::Justification::topLeft);
            label->setBorderSize ({});
            label->setMinimumHorizontalScale (1.0f);   // 찌그러뜨리지 않고 말줄임
            label->setInterceptsMouseClicks (false, false);   // 아래 패치 맵 조작을 막지 않는다
            addChildComponent (label);   // viewport 다음에 넣어 위에 그린다
        }
        model.addChangeListener (this);
        deviceManager.addChangeListener (this);
        rebuild();
    }

    ChainView::~ChainView()
    {
        deviceManager.removeChangeListener (this);
        model.removeChangeListener (this);
    }

    void ChainView::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        if (! updatesPaused)
            rebuild();
    }

    // [SPECTRUM] begin (테스트용)
    void ChainView::showSpectrumButton (std::function<void()> onClick)
    {
        spectrumButton.onClick = std::move (onClick);
        addAndMakeVisible (spectrumButton);   // viewport 다음에 넣어 위에 그린다
        resized();
    }
    // [SPECTRUM] end

    void ChainView::setUpdatesPaused (bool paused)
    {
        if (paused == updatesPaused)
            return;

        updatesPaused = paused;

        if (paused)
            stopTimer();   // 숨겨진 동안 흘러가는 이름을 다시 그릴 필요가 없다
        else
            rebuild();     // 숨겨진 동안 쌓인 변경을 한 번에 반영 (타이머도 필요하면 다시 켠다)
    }

    void ChainView::rebuild()
    {
        // 블록 수가 작으므로 변경 때마다 통째로 다시 만든다 (단순함 우선)
        const auto scroll = viewport.getViewPosition();

        // 패치의 채널이 지금 장치에 없으면 빗금
        const auto* device = deviceManager.getCurrentAudioDevice();
        const bool inputFailed = model.isChannelMissing (RigModel::Endpoint::input, device);
        const bool outputFailed = model.isChannelMissing (RigModel::Endpoint::output, device);

        // 흘러가는 파일명: 선택 블록, 그 파일, 패치, 씬이 바뀌면 처음 위치로 (그 외 알림으로는 이어서 흐른다)
        const auto* current = model.getSelectedSlot();
        const MarqueeKey key { current != nullptr ? current->id : 0, current != nullptr ? current->file : juce::File(),
                               model.getPatchNumber(), model.getCurrentScene() };

        if (! (key == marqueeKey))
        {
            marqueeKey = key;
            marqueeStartMs = juce::Time::getMillisecondCounter();
        }

        strip = std::make_unique<Strip> (model, inputFailed, outputFailed, marqueeStartMs, stompDrag);
        viewport.setViewedComponent (strip.get(), false);
        resized();
        viewport.setViewPosition (scroll);

        // 타이머는 흘러갈 블록이 있을 때만
        if (strip->selectedNameOverflows())
        {
            if (! isTimerRunning())
                startTimerHz (30);
        }
        else
        {
            stopTimer();
        }

        // 오류: 선택된 블록의 로딩 실패, 선택된 IN / OUT 의 채널이 지금 장치에 없음
        juce::String error;
        const auto endpoint = model.getSelectedEndpoint();

        if (current != nullptr)
            error = current->error;
        else if (endpoint == RigModel::Endpoint::input ? inputFailed : endpoint == RigModel::Endpoint::output && outputFailed)
            error = "Channel not available on the current device";

        errorLabel.setText (error, juce::dontSendNotification);
        errorLabel.setVisible (error.isNotEmpty());

        // 모델 레이트와 장치 레이트가 다르면 소리가 틀어진다 (리샘플 미구현)
        juce::String warning;
        const auto deviceRate = model.getDeviceSampleRate();

        if (current != nullptr && current->dsp != nullptr)
            if (const auto modelRate = current->dsp->expectedSampleRate(); modelRate > 0.0 && std::abs (modelRate - deviceRate) > 0.5)
                warning = "Sample rate mismatch:\nmodel " + juce::String (juce::roundToInt (modelRate)) + " Hz\ndevice "
                        + juce::String (juce::roundToInt (deviceRate)) + " Hz";

        warningLabel.setText (warning, juce::dontSendNotification);
        warningLabel.setVisible (warning.isNotEmpty());
        resized();

        // 새로 선택된 블록(방금 넣은 블록 등)이 화면 밖이면 보이도록 스크롤
        const auto* selected = model.getSelectedSlot();
        const auto selectedId = selected != nullptr ? selected->id : 0;

        if (selectedId != shownSelectedId)
        {
            shownSelectedId = selectedId;
            const auto tile = strip->selectedTileBounds();
            const auto view = viewport.getViewArea();

            if (! tile.isEmpty() && (tile.getX() < view.getX() || tile.getRight() > view.getRight()))
                viewport.setViewPosition (juce::jmax (0, tile.getCentreX() - view.getWidth() / 2), 0);
        }
    }

    void ChainView::timerCallback()
    {
        if (strip != nullptr)
            strip->repaintSelectedTile();
    }

    void ChainView::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.15f));
    }

    void ChainView::resized()
    {
        viewport.setBounds (getLocalBounds());

        // [SPECTRUM] 오른쪽 위 고정 (스크롤 안 됨). 오류 / 경고 문구(왼쪽 위)와 같은 여백
        constexpr int spectrumWidth = 90, spectrumHeight = 28;   // [SPECTRUM] TopBar Settings... 버튼과 같은 크기 (글자도 JUCE 기본 계산 16)
        spectrumButton.setBounds (getWidth() - margin - spectrumWidth, margin, spectrumWidth, spectrumHeight);   // [SPECTRUM]

        // 오류 / 경고: 왼쪽 위 (IN 노드와 같은 여백), 위에서부터 오류 → 경고. 글자의 줄바꿈(\n)만큼 줄 수
        constexpr int messageWidth = 420, messageLineHeight = 18;
        auto messages = juce::Rectangle<int> (margin, margin, messageWidth, getHeight());

        for (auto* label : { &errorLabel, &warningLabel })
        {
            const auto& text = label->getText();
            const int lines = label->isVisible() ? text.length() - text.removeCharacters ("\n").length() + 1 : 0;
            label->setBounds (messages.removeFromTop (messageLineHeight * lines));
        }

        if (strip != nullptr)
            strip->setSize (juce::jmax (strip->idealWidth(), viewport.getWidth()),
                            viewport.getHeight() - viewport.getScrollBarThickness());
    }
}
