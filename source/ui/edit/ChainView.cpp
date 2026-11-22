#include "ChainView.h"

#include "ui/Widgets.h"

namespace rig
{
    namespace
    {
        // 블록 양식 (Widgets.h)
        using block::corner, block::colourFor, block::labelFor, block::textInsetX, block::valueLine, block::valueFont,
              block::drawBlockBackground, block::drawBlockHeader, block::drawBlockFrame;

        constexpr int tileWidth = block::width;
        constexpr int tileHeight = block::height;
        constexpr int endWidth = tileWidth / 2;  // IN / OUT 노드 폭 = 블록 폭의 절반 (높이는 블록과 같다)
        constexpr int gap = 44;                  // 노드 사이 연결선 공간 (선 위에 놓으면 그 자리에 넣는다)
        constexpr int rowGap = 16;               // 병렬의 위 / 아래 갈래 줄 사이
        constexpr int dragStartDistance = 4;     // 블록을 이만큼 움직여야 끌기 (클릭과 구분)
        constexpr int dropMargin = 12;           // 놓을 자리 반응 범위를 블록 줄 위 / 아래 줄 아래로 넓히는 폭 (줄 사이는 간격의 절반씩)
        constexpr int autoScrollBorder = 40;     // 끄는 동안 패치맵 좌우 끝에서 이 폭 안에 들어가면 자동 스크롤
        constexpr int autoScrollSpeed = 12;      //                최대 px / 이벤트
        const juce::Colour dropHighlight (juce::Colours::white.withAlpha (0.35f));   // 놓을 자리 표시 (희미하게)
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
        // 선 원칙: 켜진 선 = 흰 실선, 꺼진 선 = 어두운 회색 점선
        const juce::Colour activeLine (juce::Colours::white);
        const juce::Colour inactiveLine (juce::Colours::grey.darker (0.6f));
        constexpr float marqueeSpeed = 35.0f;    // 선택된 블록의 긴 파일명이 흘러가는 속도 (px/초)
        constexpr juce::uint32 marqueePauseMs = 1000;   // 처음과 끝에서 멈추는 시간
        constexpr int margin = 8;

        // IN / OUT 노드 색: 고를 수 있는 노드라는 뜻으로 블록처럼 유채색 (무채색 = 미지정 / 꺼짐). NAM, IR, 전원(초록)과 겹치지 않게 보라
        const juce::Colour endpointColour (0xff9b6bd9);

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

        // 블록 오른쪽 위 귀퉁이에 박힌 삭제 칸 (공용 CornerButton). 블록 테두리 선(1.5 px 안쪽)에 맞춰 붙고,
        // 블록 테두리는 Tile::paintOverChildren 에서 이 위에 그린다
        struct CornerDeleteButton final : public CornerButton
        {
            CornerDeleteButton() : CornerButton ("X", corner, 1.5f, juce::Colours::red) {}

            void drawContent (juce::Graphics& g, juce::Rectangle<float> area, bool active) override
            {
                g.setColour (juce::Colours::white.withAlpha (active ? 1.0f : 0.8f));
                g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                g.drawText ("X", area.toNearestInt(), juce::Justification::centred);
            }
        };

    }

    //==============================================================================
    // 블록 하나를 나타내는 타일: 윗줄 = 종류(NAM / IR) + 오른쪽 귀퉁이 삭제 칸, 아랫줄 = 파일명.
    // 꺼진 블록(바이패스)과 꺼진 갈래(무음) 안의 블록은 같은 흐린 색
    class ChainView::Tile final : public juce::Component
    {
    public:
        Tile (RigModel& m, juce::uint64 id, bool inActiveBranch, const juce::uint32& startMs)
            : model (m), slotId (id), branchActive (inActiveBranch), marqueeStart (startMs)
        {
            removeButton.onClick = [this] { model.removeBlock (slotId); };
            addAndMakeVisible (removeButton);
        }

        // 선택은 끌지 않고 뗐을 때 (누를 때 고르면 패치맵이 다시 만들어져 끌기가 끊긴다)
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (isLeftClick (e) && ! e.mouseWasDraggedSinceMouseDown())
                model.selectSlot (slotId);
        }

        // 끌어서 다른 자리에 놓으면 이동 (삭제 칸에서 누른 것은 버튼이 받으므로 끌기가 시작되지 않는다)
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! isLeftClick (e) || e.getDistanceFromDragStart() < dragStartDistance)
                return;

            auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
            if (container == nullptr || container->isDragAndDropActive())
                return;

            auto image = createComponentSnapshot (getLocalBounds());
            image.multiplyAllAlphas (block::dragImageAlpha);
            const juce::Point<int> centre (-getWidth() / 2, -getHeight() / 2);
            container->startDragging (block::mapDrag (slotId), this, juce::ScaledImage (image), false, &centre);
        }

        juce::uint64 getSlotId() const noexcept { return slotId; }

        void paint (juce::Graphics& g) override
        {
            const auto* slot = model.findSlot (slotId);
            if (slot == nullptr)
                return;   // 지워진 블록: 변경 알림(비동기)으로 타일을 다시 만들기 전에 그려지는 경우

            const bool none = slot->type == BlockType::None;

            // 블록 양식. 미지정 블록은 검은 바탕에 윗줄 없음
            drawBlockBackground (g, getLocalBounds(), none ? juce::Colours::black : baseColour (*slot).darker (0.6f));
            drawBlockHeader (g, getLocalBounds(), labelFor (slot->type), baseColour (*slot).brighter (0.4f), removeWidth - textInsetX);

            const auto text = nameArea();
            g.setColour (none ? (isDimmed (*slot) ? juce::Colours::grey.darker (0.5f) : juce::Colours::grey) : juce::Colours::white);
            g.setFont (valueFont());
            const auto name = none ? juce::String ("Empty") : slot->file.getFileNameWithoutExtension();
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

            const auto width = juce::GlyphArrangement::getStringWidth (valueFont(),
                                                                      slot->file.getFileNameWithoutExtension());
            return width - (float) nameArea().getWidth();
        }

        // 파일명 줄 = 아랫줄
        juce::Rectangle<int> nameArea() const
        {
            return valueLine (getLocalBounds());
        }

        // 테두리는 버튼(삭제 칸) 위에 그려야 귀퉁이 칸이 블록 테두리를 같이 쓰는 것처럼 보인다
        void paintOverChildren (juce::Graphics& g) override
        {
            const auto* slot = model.findSlot (slotId);
            if (slot == nullptr)
                return;

            drawBlockFrame (g, getLocalBounds(), slot->type == BlockType::None ? juce::Colours::grey.darker() : baseColour (*slot),
                            model.getSelectedSlotId() == slotId, slot->error.isNotEmpty());
        }

        void resized() override
        {
            // 삭제 칸은 블록 오른쪽 위 귀퉁이에 딱 붙인다
            removeButton.setBounds (getWidth() - removeWidth, 0, removeWidth, removeHeight);
        }

    private:
        // 꺼진 블록(바이패스) 또는 꺼진 갈래(무음) 안
        bool isDimmed (const RigModel::Slot& slot) const
        {
            return ! branchActive || (! model.isOn (slot) && slot.type != BlockType::None);
        }

        juce::Colour baseColour (const RigModel::Slot& slot) const
        {
            const auto base = colourFor (slot.type);
            return isDimmed (slot) ? base.withSaturation (0.1f).darker (0.5f) : base;
        }

        RigModel& model;
        const juce::uint64 slotId;
        const bool branchActive;            // 블록이 있는 갈래가 현재 씬에서 켜져 있는지 (본 줄은 항상 true)
        const juce::uint32& marqueeStart;   // ChainView 소유: 타일을 다시 만들어도 흘러가던 위치가 이어진다
        static constexpr int removeWidth = 36, removeHeight = 20;
        CornerDeleteButton removeButton;
    };

    //==============================================================================
    // 병렬의 합류(Merge) 버튼. 클릭 영역(컴포넌트)은 mergeHitSize 정사각형.
    //  - 클릭(끌지 않고 뗌) = 합류 선택 (파라미터 바에 갈래 On/Off·레벨)
    //  - 끌기 = 합류 지점 옮기기 (Strip 이 후보 자리를 보여주고 뗄 때 옮긴다)
    //  - 마우스를 올리면 mergeHoverScale 까지 커진다 (화면 갱신마다 조금씩)
    class ChainView::MergeButton final : public juce::Component
    {
    public:
        MergeButton (RigModel& m, juce::uint64 id) : model (m), parallelId (id) {}

        std::function<void (int stripX)> onDragMove, onDragEnd;   // Strip 좌표의 x

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
                onDragMove (e.getEventRelativeTo (getParentComponent()).x);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! isLeftClick (e))
                return;

            if (! dragging)
                model.selectParallel (parallelId);
            else if (onDragEnd != nullptr)
                onDragEnd (e.getEventRelativeTo (getParentComponent()).x);

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
    //  - 블록 위     : 그 블록의 파일 교체 (선택기 항목만)
    //  - 본 줄 블록 바로 아래 : 그 블록과 병렬 (위 갈래 = 그 블록, 아래 갈래 = 놓은 것). 갈래 안 블록 아래는 놓을 자리 아님
    //  - 그 밖 / Esc : 취소. 제자리에 놓으면 아무 일도 없다
    class ChainView::Strip final : public juce::Component,
                                   public juce::DragAndDropTarget
    {
    public:
        Strip (RigModel& m, bool inFailed, bool outFailed, const juce::uint32& startMs)
            : model (m), inputFailed (inFailed), outputFailed (outFailed), marqueeStart (startMs)
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
                const auto index = (int) parallels.size();
                view.item = (int) items.size();
                view.button = mergeButtons.add (new MergeButton (model, parallel.id));
                view.button->onDragMove = [this, index] (int x) { setMergeTarget (index, x); };
                view.button->onDragEnd  = [this, index, id = parallel.id] (int x)
                {
                    setMergeTarget (index, x);
                    const auto delta = mergeTarget.has_value() ? mergeTarget->delta : 0;
                    mergeTarget.reset();
                    repaint();

                    if (delta != 0)
                        model.moveMerge (id, delta);
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

        // IN / OUT 노드 클릭 = 선택 (파라미터 바에 채널 선택이 나온다)
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! isLeftClick (e))
                return;

            if (inNode().contains (e.getPosition()))
                model.selectEndpoint (RigModel::Endpoint::input);
            else if (outNode().contains (e.getPosition()))
                model.selectEndpoint (RigModel::Endpoint::output);
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

            for (auto* tile : tiles)
                dropZones.push_back ({ tile->getBounds(), DropZone::replace, {}, tile->getSlotId(), tile->getBounds() });

            repaint();
        }

        //==============================================================================
        bool isInterestedInDragSource (const SourceDetails& details) override
        {
            return readDrag (details.description).has_value();
        }

        void itemDragEnter (const SourceDetails& details) override { itemDragMove (details); }

        void itemDragMove (const SourceDetails& details) override
        {
            if (auto* scroller = findParentComponentOfClass<juce::Viewport>())
            {
                const auto p = scroller->getLocalPoint (this, details.localPosition);
                scroller->autoScroll (p.x, p.y, autoScrollBorder, autoScrollSpeed);   // 세로 스크롤은 없으므로 가로만 움직인다
            }

            setHover (zoneAt (details));
        }

        void itemDragExit (const SourceDetails&) override { setHover (std::nullopt); }

        void itemDropped (const SourceDetails& details) override
        {
            const auto zone = zoneAt (details);
            const auto drag = readDrag (details.description);
            setHover (std::nullopt);

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
        }

        // 놓을 자리 / 옮길 합류 지점 미리보기 (블록 위에)
        void paintOverChildren (juce::Graphics& g) override
        {
            if (mergeTarget.has_value() && mergeTarget->delta != 0)
            {
                // 합류 버튼이 갈 자리: 희미한 버튼 + 아래 갈래가 올라올 세로선.
                // 오른쪽으로 옮기면 아래 갈래 선이 지금 합류 자리에서 새 자리까지 늘어난다 (ㄱ 모서리는 선 두께 절반 겹침)
                const float row0 = (float) rowY (0) + (float) tileHeight * 0.5f, row1 = (float) rowY (1) + (float) tileHeight * 0.5f;
                const float half = lineThickness * 0.5f;
                const auto x = (float) mergeTarget->x;
                const auto current = (float) parallels[(size_t) mergeTarget->parallel].mergeX;
                g.beginTransparencyLayer (dropHighlight.getFloatAlpha());
                g.setColour (activeLine);

                if (x > current)
                    g.drawLine (current, row1, x + half, row1, lineThickness);

                g.drawLine (x, row0, x, row1 + half, lineThickness);
                drawMergeButton (g, *this, { x, row0 }, mergeSize, false, false);
                g.endTransparencyLayer();
            }

            if (! hover.has_value())
                return;

            g.setColour (dropHighlight);

            if (hover->kind == DropZone::insert)
            {
                g.fillRoundedRectangle (hover->area.toFloat().reduced (2.0f), block::corner);
                return;
            }

            if (hover->kind == DropZone::replace)
            {
                g.drawRoundedRectangle (hover->area.toFloat().expanded (2.0f), block::corner, 3.0f);
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
            g.strokePath (path, juce::PathStrokeType (lineThickness));

            g.drawRoundedRectangle (lower.reduced (1.5f), block::corner, 1.5f);

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
            int item = -1;          // items 안의 번호
            int mergeX = 0;         // 지금 합류 버튼 x (resized 에서)
        };

        // 합류 버튼을 끄는 동안 고른 자리
        struct MergeTarget
        {
            int parallel = -1, delta = 0, x = 0;
        };

        // 합류 지점 후보 (delta, x): 위 갈래를 1개까지 줄이는 자리들, 지금 자리, 병렬 바로 뒤 본 줄 블록을 넣는 자리들(다른 병렬 앞까지).
        // x 는 지금 배치에서 그 블록 오른쪽 연결선 가운데
        std::vector<std::pair<int, int>> mergeCandidates (int index) const
        {
            const auto& view = parallels[(size_t) index];
            const auto& upper = view.tiles[0];
            std::vector<std::pair<int, int>> candidates;

            for (size_t k = 1; k < upper.size(); ++k)
                candidates.push_back ({ (int) k - (int) upper.size(), upper[k - 1]->getRight() + gap / 2 });

            candidates.push_back ({ 0, view.mergeX });

            for (size_t j = (size_t) view.item + 1; j < items.size() && items[j].tile != nullptr; ++j)
                candidates.push_back ({ (int) (j - (size_t) view.item), items[j].tile->getRight() + gap / 2 });

            return candidates;
        }

        void setMergeTarget (int index, int x)
        {
            const auto candidates = mergeCandidates (index);
            const auto nearest = std::min_element (candidates.begin(), candidates.end(),
                                                   [x] (const auto& a, const auto& b) { return std::abs (a.second - x) < std::abs (b.second - x); });

            mergeTarget = MergeTarget { index, nearest->first, nearest->second };
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

            const auto from = drag->fromLibrary ? std::nullopt : model.findPlace (drag->slotId);

            for (const auto& zone : dropZones)
            {
                if (! hitArea (zone.area).contains (details.localPosition))
                    continue;

                if (drag->fromLibrary)
                    return zone;

                if (! from.has_value() || zone.kind == DropZone::replace)   // 패치맵 안에서는 이동만
                    return std::nullopt;

                if (zone.kind == DropZone::parallel)
                    return zone.slotId == drag->slotId ? std::nullopt : std::optional<DropZone> (zone);

                // 같은 목록에서 바로 앞이나 바로 뒤 자리 = 제자리
                const bool sameList = from->branch < 0 ? zone.place.branch < 0
                                                       : zone.place.branch == from->branch && zone.place.item == from->item;
                const int position = from->branch < 0 ? from->item : from->index;
                const int target = zone.place.branch < 0 ? zone.place.item : zone.place.index;

                return sameList && (target == position || target == position + 1) ? std::nullopt : std::optional<DropZone> (zone);
            }

            return std::nullopt;
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

        Tile* addTile (juce::uint64 id, bool active)
        {
            auto* tile = tiles.add (new Tile (model, id, active, marqueeStart));
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

        // 블록 양식 그대로: 윗줄 "IN" / "OUT", 아랫줄 채널. 장치에 없는 채널 = 블록의 로딩 실패와 같은 오류 표시
        static void drawEndNode (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, const juce::String& channels,
                                 bool selected, bool failed)
        {
            drawBlockBackground (g, area, endpointColour.darker (0.6f));
            drawBlockHeader (g, area, title, endpointColour.brighter (0.4f));
            g.setColour (juce::Colours::white);
            g.setFont (valueFont());
            g.drawText (channels, valueLine (area), juce::Justification::centredLeft, true);   // 넘치면 말줄임
            drawBlockFrame (g, area, endpointColour, selected, failed);
        }

        RigModel& model;
        const bool inputFailed, outputFailed;
        const juce::uint32& marqueeStart;   // ChainView 소유 (타일에 넘긴다)
        bool hasParallel = false;
        std::vector<Item> items;                 // 본 줄 순서
        std::vector<ParallelView> parallels;
        juce::OwnedArray<Tile> tiles;
        juce::OwnedArray<MergeButton> mergeButtons;
        std::vector<RigModel::Place> insertPlaces;   // 연결선 순서 (resized 의 배치 순서와 같다)
        std::vector<Line> lines;                     // resized 에서 계산
        std::vector<DropZone> dropZones;             // resized 에서 계산
        std::optional<DropZone> hover;               // 끄는 동안 놓을 자리
        std::optional<MergeTarget> mergeTarget;      // 합류 버튼을 끄는 동안 옮길 자리
    };

    //==============================================================================
    ChainView::ChainView (RigModel& m, juce::AudioDeviceManager& dm) : model (m), deviceManager (dm)
    {
        viewport.setScrollBarsShown (false, true);
        addAndMakeVisible (viewport);

        warningLabel.setColour (juce::Label::textColourId, juce::Colours::yellow);
        warningLabel.setJustificationType (juce::Justification::topLeft);
        warningLabel.setBorderSize ({});
        warningLabel.setMinimumHorizontalScale (1.0f);   // 찌그러뜨리지 않고 말줄임
        warningLabel.setInterceptsMouseClicks (false, false);   // 아래 패치 맵 조작을 막지 않는다
        addChildComponent (warningLabel);   // viewport 다음에 넣어 위에 그린다
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

        strip = std::make_unique<Strip> (model, inputFailed, outputFailed, marqueeStartMs);
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

        // 경고: 왼쪽 위 (IN 노드와 같은 여백), 글자의 줄바꿈(\n)만큼 줄 수. 두 줄(병렬)일 때도 블록 줄 위에 들어간다
        constexpr int warningWidth = 420, warningLineHeight = 18;
        const auto& text = warningLabel.getText();
        const int lines = text.length() - text.removeCharacters ("\n").length() + 1;
        warningLabel.setBounds (margin, margin, warningWidth, warningLineHeight * lines);

        if (strip != nullptr)
            strip->setSize (juce::jmax (strip->idealWidth(), viewport.getWidth()),
                            viewport.getHeight() - viewport.getScrollBarThickness());
    }
}
