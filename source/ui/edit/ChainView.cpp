#include "ChainView.h"

#include "ui/Widgets.h"

namespace rig
{
    namespace
    {
        constexpr int tileWidth = 130;           // 블록 폭 (예전 150)
        constexpr int tileHeight = 44;           // 블록 높이 (예전 72): 위 여백 5 + 종류 줄 16 + 파일명 줄 18 + 아래 여백 5
        constexpr int endWidth = tileWidth / 2;  // IN / OUT 노드 폭 = 블록 폭의 절반 (높이는 블록과 같다)
        constexpr int gap = 44;                  // 노드 사이 연결선 공간 ([+] 포함)
        constexpr int rowGap = 16;               // 병렬의 위 / 아래 갈래 줄 사이
        constexpr int plusSize = 20;             // 연결선 가운데 [+] 버튼의 보이는 크기 (예전 18)
        constexpr int plusHitInset = 2;          // [+] 클릭 영역: 가로는 노드 사이 간격 전체에서 양쪽 2 px 뺀 만큼
        constexpr float plusHitHeightRatio = 0.61f;   //                세로는 블록 높이의 약 0.6 (44 px → 27 px)
        constexpr float dotRadius = 4.0f;        // Split / Merge 점 반지름
        constexpr int mergeHitSize = 20;         // Merge 점 클릭 영역 (정사각형)
        constexpr float lineThickness = 2.0f;
        // 선 원칙: 켜진 선 = 흰 실선, 꺼진 선 = 어두운 회색 점선
        const juce::Colour activeLine (juce::Colours::white);
        const juce::Colour inactiveLine (juce::Colours::grey.darker (0.6f));
        constexpr float marqueeSpeed = 35.0f;    // 선택된 블록의 긴 파일명이 흘러가는 속도 (px/초)
        constexpr juce::uint32 marqueePauseMs = 1000;   // 처음과 끝에서 멈추는 시간
        constexpr int margin = 8;
        constexpr float corner = 8.0f;

        // IN / OUT 노드 색: 고를 수 있는 노드라는 뜻으로 블록처럼 유채색 (무채색 = 미지정 / 꺼짐). NAM, IR, 전원(초록)과 겹치지 않게 보라
        const juce::Colour endpointColour (0xff9b6bd9);

        juce::Colour colourFor (BlockType t)
        {
            switch (t)
            {
                case BlockType::Nam:  return juce::Colour (0xffd9822b);
                case BlockType::Ir:   return juce::Colour (0xff2b9bd9);
                case BlockType::None: break;
            }

            return juce::Colours::black;
        }

        // 블록과 IN / OUT 공통 글자 배치: 윗줄(종류 / IN·OUT) + 아랫줄(파일명 / 채널) 묶음을 세로 가운데에.
        // 높이 44 기준 위아래 여백 (44 - 16 - 18) / 2 = 5 px 씩
        constexpr int headerLineHeight = 16, valueLineHeight = 18;
        constexpr int textInsetX = 10;   // 좌우 여백

        juce::Rectangle<int> textBlock (juce::Rectangle<int> area)
        {
            return area.reduced (textInsetX, 0).withSizeKeepingCentre (area.getWidth() - 2 * textInsetX, headerLineHeight + valueLineHeight);
        }

        juce::Rectangle<int> headerLine (juce::Rectangle<int> area) { return textBlock (area).removeFromTop (headerLineHeight); }
        juce::Rectangle<int> valueLine (juce::Rectangle<int> area)  { return textBlock (area).withTrimmedTop (headerLineHeight); }

        juce::String labelFor (BlockType t)
        {
            switch (t)
            {
                case BlockType::Nam:  return "NAM";
                case BlockType::Ir:   return "IR";
                case BlockType::None: break;
            }

            return {};
        }

        // 연결선 가운데의 [+] 버튼. 컴포넌트(= 클릭 영역)는 넓고, 버튼은 가운데 plusSize 정사각형에만 그린다.
        // 바탕은 LookAndFeel_V4::drawButtonBackground 와 같은 모양, + 는 선으로 그려 버튼을 꽉 채운다 (글꼴 크기와 무관)
        struct PlusButton final : public LeftClickOnly<juce::Button>
        {
            PlusButton() : LeftClickOnly ("+") {}

            void paintButton (juce::Graphics& g, bool highlighted, bool down) override
            {
                const auto button = getLocalBounds().toFloat().withSizeKeepingCentre ((float) plusSize, (float) plusSize);

                auto colour = findColour (juce::TextButton::buttonColourId).withMultipliedSaturation (hasKeyboardFocus (true) ? 1.3f : 0.9f);
                if (down || highlighted)
                    colour = colour.contrasting (down ? 0.2f : 0.05f);

                g.setColour (colour);
                g.fillRoundedRectangle (button.reduced (0.5f), 6.0f);
                g.setColour (findColour (juce::ComboBox::outlineColourId));
                g.drawRoundedRectangle (button.reduced (0.5f), 6.0f, 1.0f);

                const auto area = button.reduced ((float) plusSize * 0.25f);   // 팔 길이 = 버튼의 50 %
                g.setColour (findColour (juce::TextButton::textColourOffId));
                g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.6f);
                g.drawLine (area.getCentreX(), area.getY(), area.getCentreX(), area.getBottom(), 1.6f);
            }
        };

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

        // 로딩 실패 표시: 블록 위에 굵은 빨간 대각선 하나 (테두리와 같은 굵기)
        constexpr float errorThickness = 4.0f;

        void drawErrorSlash (juce::Graphics& g, juce::Rectangle<float> bounds)
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (bounds, corner);
            g.reduceClipRegion (clip);

            g.setColour (juce::Colours::red);
            g.drawLine (bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getY(), errorThickness);
        }

        // 블록 양식 (일반 블록, IN / OUT 공통). 색 하나로: 바탕 = 색.darker (0.6), 윗줄 = 색.brighter (0.4) 굵게,
        // 아랫줄 = 흰색, 테두리 = 색. 글자 배치는 headerLine / valueLine
        constexpr float headerFontSize = 12.0f;   // 윗줄 (종류 / IN·OUT)
        constexpr float valueFontSize = 13.0f;    // 아랫줄 (파일명 / 채널)

        juce::Font valueFont() { return juce::Font (juce::FontOptions (valueFontSize)); }

        void drawBlockBackground (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour fill)
        {
            g.setColour (fill);
            g.fillRoundedRectangle (area.toFloat().reduced (1.5f), corner);
        }

        // rightInset: 윗줄 오른쪽에 비울 폭 (일반 블록의 삭제 칸)
        void drawBlockHeader (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour, int rightInset = 0)
        {
            g.setColour (colour);
            g.setFont (juce::FontOptions (headerFontSize, juce::Font::bold));
            g.drawText (text, headerLine (area).withTrimmedRight (rightInset), juce::Justification::centredLeft, true);
        }

        // 테두리: 선택 = 흰색 2.5 px, 아니면 border 1.5 px.
        // 오류 = 굵은 빨간 대각선 + 같은 굵기의 빨간 테두리 (선택까지 되어 있으면 그 바깥에 흰 테두리)
        void drawBlockFrame (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour border, bool selected, bool failed)
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

        void mouseDown (const juce::MouseEvent& e) override { if (isLeftClick (e)) model.selectSlot (slotId); }

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
    // 병렬의 합류(Merge) 점. 보이는 점은 작고, 클릭 영역(컴포넌트)은 mergeHitSize 정사각형. 누르면 합류 선택
    struct ChainView::MergeDot final : public LeftClickOnly<juce::Button>
    {
        MergeDot (RigModel& m, juce::uint64 id) : LeftClickOnly ("Merge"), model (m), parallelId (id)
        {
            onClick = [this] { model.selectParallel (parallelId); };
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool) override
        {
            const auto centre = getLocalBounds().toFloat().getCentre();
            const bool selected = model.getSelectedParallelId() == parallelId;
            const float r = selected || highlighted ? dotRadius + 1.5f : dotRadius;

            g.setColour (activeLine);
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));

            // 선택 = 바깥 흰 고리 (블록 선택 테두리와 같은 굵기)
            if (selected)
                g.drawEllipse (juce::Rectangle<float> ((r + 4.0f) * 2.0f, (r + 4.0f) * 2.0f).withCentre (centre), 2.5f);
        }

        RigModel& model;
        const juce::uint64 parallelId;
    };

    //==============================================================================
    // 뷰포트 안에서 가로로 늘어나는 내용물: IN, 블록들, OUT 노드와 그 사이 연결선/[+].
    // 병렬은 위 갈래를 본 줄 높이에, 아래 갈래를 그 아래 줄에 그린다. 갈래가 갈라지는 곳 = Split 점, 합쳐지는 곳 = Merge 점.
    // 짧은 갈래는 선이 Merge 까지 늘어난다
    class ChainView::Strip final : public juce::Component
    {
    public:
        Strip (RigModel& m, bool inFailed, bool outFailed, const juce::uint32& startMs)
            : model (m), inputFailed (inFailed), outputFailed (outFailed), marqueeStart (startMs)
        {
            // 만드는 순서 = resized() 에서 배치하는 순서: [+] 와 블록이 연결선을 따라 차례로
            const auto& chain = model.getChain();

            for (int i = 0; i < (int) chain.size(); ++i)
            {
                addPlus ({ i, -1, 0 });

                if (const auto* slot = std::get_if<RigModel::Slot> (&chain[(size_t) i]))
                {
                    items.push_back ({ addTile (slot->id, true), -1 });
                    continue;
                }

                const auto& parallel = std::get<RigModel::Parallel> (chain[(size_t) i]);
                ParallelView view;
                view.dot = mergeDots.add (new MergeDot (model, parallel.id));
                addAndMakeVisible (view.dot);

                for (int b = 0; b < 2; ++b)
                {
                    const auto& branch = parallel.branches[(size_t) b];
                    view.active[(size_t) b] = model.isBranchOn (parallel, b);

                    for (int k = 0; k < (int) branch.size(); ++k)
                    {
                        addPlus ({ i, b, k });
                        view.tiles[(size_t) b].push_back (addTile (branch[(size_t) k].id, view.active[(size_t) b]));
                    }

                    addPlus ({ i, b, (int) branch.size() });
                }

                items.push_back ({ nullptr, (int) parallels.size() });
                parallels.push_back (std::move (view));
                hasParallel = true;
            }

            addPlus ({ (int) chain.size(), -1, 0 });
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

            g.setColour (activeLine);
            for (const auto& p : splitPoints)
                g.fillEllipse (juce::Rectangle<float> (dotRadius * 2.0f, dotRadius * 2.0f).withCentre (p));
        }

        void resized() override
        {
            lines.clear();
            splitPoints.clear();

            const float row0 = (float) rowY (0) + (float) tileHeight * 0.5f, row1 = (float) rowY (1) + (float) tileHeight * 0.5f;
            size_t nextPlus = 0;

            // 연결선 한 칸: [+] 는 앞쪽 gap 안 가운데, 선은 x + 2 ~ to (블록 앞이면 화살표 끝을 2 px 띄운다).
            // 아래 갈래의 ㄴ / ㄱ 모서리는 세로선과 가로선을 선 두께의 절반씩 겹쳐 끊기지 않게 한다 (from, to 를 직접 준다)
            const float half = lineThickness * 0.5f;

            const auto segment = [&] (int x, int row, float to, bool arrow, bool active, float from = -1.0f)
            {
                const int y = rowY (row);
                const int hitHeight = juce::roundToInt ((float) tileHeight * plusHitHeightRatio);
                plusButtons[(int) nextPlus++]->setBounds (x + plusHitInset, y + (tileHeight - hitHeight) / 2, gap - 2 * plusHitInset, hitHeight);

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
                    x += gap + tileWidth;
                    continue;
                }

                // 병렬: 본 줄 선이 Split 점에서 끝나고, 갈래마다 블록을 지나 Merge 점까지
                const auto& view = parallels[(size_t) item.parallel];
                const int split = x + gap;
                const int merge = split + parallelWidth (view);
                segment (x, 0, (float) split, false, true);
                splitPoints.push_back ({ (float) split, row0 });

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
                        segment (bx, row, (float) (bx + gap), true, active, row == 1 && k == 0 ? (float) split - half : -1.0f);
                        view.tiles[b][k]->setBounds (bx + gap, rowY (row), tileWidth, tileHeight);
                        bx += gap + tileWidth;
                    }

                    segment (bx, row, row == 1 ? (float) merge + half : (float) merge, false, active);

                    if (row == 1)
                        lines.push_back ({ { (float) merge, row1 + half, (float) merge, row0 }, active, false });
                }

                view.dot->setBounds (juce::Rectangle<int> (mergeHitSize, mergeHitSize).withCentre ({ merge, juce::roundToInt (row0) }));
                x = merge;
            }

            segment (x, 0, (float) (x + gap), true, true);
            repaint();
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
            MergeDot* dot = nullptr;
        };

        struct Line
        {
            juce::Line<float> line;
            bool active = true, arrow = false;
        };

        void addPlus (RigModel::Place where)
        {
            auto* plus = plusButtons.add (new PlusButton());
            plus->onClick = [this, where] { model.insertBlock (where, BlockType::None, {}); };
            addAndMakeVisible (plus);
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
        juce::OwnedArray<MergeDot> mergeDots;
        juce::OwnedArray<PlusButton> plusButtons;   // 연결선 순서 (resized 의 배치 순서와 같다)
        std::vector<Line> lines;                    // resized 에서 계산
        std::vector<juce::Point<float>> splitPoints;
    };

    //==============================================================================
    ChainView::ChainView (RigModel& m, juce::AudioDeviceManager& dm) : model (m), deviceManager (dm)
    {
        viewport.setScrollBarsShown (false, true);
        addAndMakeVisible (viewport);
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

        if (strip != nullptr)
            strip->setSize (juce::jmax (strip->idealWidth(), viewport.getWidth()),
                            viewport.getHeight() - viewport.getScrollBarThickness());
    }
}
