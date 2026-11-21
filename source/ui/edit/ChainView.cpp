#include "ChainView.h"

#include "ui/Widgets.h"

namespace rig
{
    namespace
    {
        constexpr int tileWidth = 150;
        constexpr int tileHeight = 72;
        constexpr int endWidth = 76;     // IN / OUT 노드 폭 (채널 번호 포함)
        constexpr int gap = 44;          // 노드 사이 연결선 공간 ([+] 포함)
        constexpr int plusSize = 20;             // 연결선 가운데 [+] 버튼의 보이는 크기 (예전 18)
        constexpr int plusHitInset = 2;          // [+] 클릭 영역: 가로는 노드 사이 간격 전체에서 양쪽 2 px 뺀 만큼
        constexpr float plusHitHeightRatio = 0.61f;   //                세로는 블록 높이의 약 0.6 (72 px → 44 px)
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
    }

    //==============================================================================
    // 블록 하나를 나타내는 타일. 병렬은 아직 그리지 않고 위 갈래, 아래 갈래 블록을 본 줄처럼 이어서 보여준다 (임시).
    // < > 는 블록이 있는 목록(본 줄 또는 그 갈래) 안에서만 옮긴다
    class ChainView::Tile final : public juce::Component
    {
    public:
        Tile (RigModel& m, juce::uint64 id, int n, RigModel::Place p, int listSize, const juce::uint32& startMs)
            : model (m), slotId (id), number (n), place (p), marqueeStart (startMs)
        {
            // moveBlock 의 자리는 옮기기 전 기준: 왼쪽 = 한 칸 앞, 오른쪽 = 다음 블록의 뒤
            leftButton.onClick   = [this] { model.moveBlock (slotId, shifted (-1)); };
            rightButton.onClick  = [this] { model.moveBlock (slotId, shifted (2)); };
            removeButton.onClick = [this] { model.removeBlock (slotId); };

            const int position = place.branch < 0 ? place.item : place.index;
            leftButton.setEnabled (position > 0);
            rightButton.setEnabled (position < listSize - 1);

            for (auto* b : std::initializer_list<juce::Component*> { &leftButton, &rightButton, &removeButton })
                addAndMakeVisible (b);
        }

        void mouseDown (const juce::MouseEvent& e) override { if (isLeftClick (e)) model.selectSlot (slotId); }

        juce::uint64 getSlotId() const noexcept { return slotId; }

        void paint (juce::Graphics& g) override
        {
            const auto* found = model.findSlot (slotId);
            if (found == nullptr)
                return;   // 지워진 블록: 변경 알림(비동기)으로 타일을 다시 만들기 전에 그려지는 경우

            const auto& slot = *found;
            auto bounds = getLocalBounds().toFloat().reduced (1.5f);

            auto base = colourFor (slot.type);
            if (! model.isOn (slot) && slot.type != BlockType::None)
                base = base.withSaturation (0.1f).darker (0.5f);

            g.setColour (slot.type == BlockType::None ? juce::Colours::black : base.darker (0.6f));
            g.fillRoundedRectangle (bounds, corner);

            auto text = getLocalBounds().reduced (10, 6).withTrimmedBottom (24);   // 윗줄 16 px 를 떼면 nameArea()

            g.setColour (slot.type == BlockType::None ? juce::Colours::grey : base.brighter (0.4f));
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText ("#" + juce::String (number) + "  " + labelFor (slot.type),
                        text.removeFromTop (16).withTrimmedRight (removeWidth),   // 윗줄 오른쪽 = 삭제 칸
                        juce::Justification::centredLeft);

            g.setColour (slot.type == BlockType::None ? juce::Colours::grey : juce::Colours::white);
            g.setFont (juce::FontOptions (13.0f));
            const auto name = slot.type == BlockType::None ? juce::String ("Empty") : slot.file.getFileNameWithoutExtension();
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

            const auto width = juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (13.0f)),
                                                                      slot->file.getFileNameWithoutExtension());
            return width - (float) nameArea().getWidth();
        }

        juce::Rectangle<int> nameArea() const
        {
            return getLocalBounds().reduced (10, 6).withTrimmedBottom (24).withTrimmedTop (16);
        }

        // 테두리는 버튼(삭제 칸) 위에 그려야 귀퉁이 칸이 블록 테두리를 같이 쓰는 것처럼 보인다
        void paintOverChildren (juce::Graphics& g) override
        {
            const auto* found = model.findSlot (slotId);
            if (found == nullptr)
                return;

            const auto& slot = *found;
            const bool selected = model.getSelectedSlotId() == slotId;
            const bool failed = slot.error.isNotEmpty();
            const auto bounds = getLocalBounds().toFloat().reduced (1.5f);

            auto base = colourFor (slot.type);
            if (! model.isOn (slot) && slot.type != BlockType::None)
                base = base.withSaturation (0.1f).darker (0.5f);

            // 오류: 블록 위(버튼 포함)에 굵은 빨간 대각선 + 같은 굵기의 빨간 테두리.
            // 선택까지 되어 있으면 빨간 테두리 바깥에 흰 테두리
            if (failed)
            {
                const auto inner = selected ? bounds.reduced (2.5f) : bounds;
                drawErrorSlash (g, inner);
                g.setColour (juce::Colours::red);
                g.drawRoundedRectangle (inner.reduced (errorThickness * 0.5f - 0.75f), corner, errorThickness);
            }

            // 테두리: 선택 = 흰색
            if (selected)
                g.setColour (juce::Colours::white);
            else if (! failed)
                g.setColour (slot.type == BlockType::None ? juce::Colours::grey.darker() : base);

            if (selected || ! failed)
                g.drawRoundedRectangle (bounds, corner, selected ? 2.5f : 1.5f);
        }

        void resized() override
        {
            // 삭제 칸은 블록 오른쪽 위 귀퉁이에 딱 붙인다
            removeButton.setBounds (getWidth() - removeWidth, 0, removeWidth, removeHeight);

            auto row = getLocalBounds().reduced (6).removeFromBottom (22);
            leftButton.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (1));
            rightButton.setBounds (row.reduced (1));
        }

    private:
        RigModel::Place shifted (int delta) const
        {
            auto p = place;
            (p.branch < 0 ? p.item : p.index) += delta;
            return p;
        }

        RigModel& model;
        const juce::uint64 slotId;
        const int number;               // 화면에 보이는 순번 (1부터)
        const RigModel::Place place;
        const juce::uint32& marqueeStart;   // ChainView 소유: 타일을 다시 만들어도 흘러가던 위치가 이어진다
        static constexpr int removeWidth = 36, removeHeight = 20;
        LeftClickOnly<juce::TextButton> leftButton { "<" }, rightButton { ">" };
        CornerDeleteButton removeButton;
    };

    //==============================================================================
    // 뷰포트 안에서 가로로 늘어나는 내용물: IN, 블록들, OUT 노드와 그 사이 연결선/[+]
    class ChainView::Strip final : public juce::Component
    {
    public:
        Strip (RigModel& m, bool inFailed, bool outFailed, const juce::uint32& startMs)
            : model (m), inputFailed (inFailed), outputFailed (outFailed)
        {
            // 블록을 화면 순서로: 본 줄 블록, 병렬은 위 갈래 다음 아래 갈래 (임시)
            std::vector<std::pair<juce::uint64, RigModel::Place>> blocks;
            std::vector<int> listSizes;
            const auto& chain = model.getChain();

            for (int i = 0; i < (int) chain.size(); ++i)
            {
                if (const auto* slot = std::get_if<RigModel::Slot> (&chain[(size_t) i]))
                {
                    blocks.push_back ({ slot->id, { i, -1, 0 } });
                    listSizes.push_back ((int) chain.size());
                    continue;
                }

                const auto& branches = std::get<RigModel::Parallel> (chain[(size_t) i]).branches;

                for (int b = 0; b < 2; ++b)
                    for (int k = 0; k < (int) branches[(size_t) b].size(); ++k)
                    {
                        blocks.push_back ({ branches[(size_t) b][(size_t) k].id, { i, b, k } });
                        listSizes.push_back ((int) branches[(size_t) b].size());
                    }
            }

            for (size_t i = 0; i < blocks.size(); ++i)
                addAndMakeVisible (tiles.add (new Tile (model, blocks[i].first, (int) i + 1, blocks[i].second, listSizes[i], startMs)));

            // 연결선마다 하나: 첫 [+] 는 본 줄 맨 앞, 나머지는 바로 왼쪽 블록의 뒤 (그 블록이 있는 목록 안)에 넣는다
            for (size_t i = 0; i <= blocks.size(); ++i)
            {
                RigModel::Place where;

                if (i > 0)
                {
                    where = blocks[i - 1].second;
                    (where.branch < 0 ? where.item : where.index) += 1;
                }

                auto* plus = plusButtons.add (new PlusButton());
                plus->onClick = [this, where] { model.insertBlock (where, BlockType::None, {}); };
                addAndMakeVisible (plus);
            }
        }

        int idealWidth() const
        {
            return margin + endWidth + tiles.size() * (gap + tileWidth) + gap + endWidth + margin;
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
            const auto y = nodeY();
            const auto selected = model.getSelectedEndpoint();

            const auto& routing = model.getRouting();
            drawEndNode (g, inNode(), "IN", channelText (routing.input, 1),
                         selected == RigModel::Endpoint::input, inputFailed);
            drawEndNode (g, outNode(), "OUT", channelText (routing.output, routing.stereoOutput ? 2 : 1),
                         selected == RigModel::Endpoint::output, outputFailed);

            // 연결선: 각 [+] 의 왼쪽 노드 끝에서 오른쪽 노드 시작까지
            const float midY = (float) y + tileHeight * 0.5f;
            g.setColour (juce::Colours::grey);

            for (int i = 0; i <= tiles.size(); ++i)
            {
                const float x1 = (float) (margin + endWidth + i * (gap + tileWidth));
                const float x2 = x1 + gap;
                g.drawArrow ({ x1 + 2.0f, midY, x2 - 2.0f, midY }, 2.0f, 8.0f, 8.0f);
            }
        }

        void resized() override
        {
            const int y = nodeY();
            int x = margin + endWidth;

            for (int i = 0; i <= tiles.size(); ++i)
            {
                // 클릭 영역: 양옆 노드와 plusHitInset 씩 띄운 간격 전체 × 블록 높이의 약 0.6 (그림은 가운데 plusSize)
                const int hitHeight = juce::roundToInt ((float) tileHeight * plusHitHeightRatio);
                plusButtons[i]->setBounds (x + plusHitInset, y + (tileHeight - hitHeight) / 2, gap - 2 * plusHitInset, hitHeight);
                x += gap;

                if (auto* t = tiles[i])
                {
                    t->setBounds (x, y, tileWidth, tileHeight);
                    x += tileWidth;
                }
            }
        }

    private:
        Tile* selectedTile() const
        {
            for (auto* t : tiles)
                if (t->getSlotId() == model.getSelectedSlotId())
                    return t;

            return nullptr;
        }

        int nodeY() const { return (getHeight() - tileHeight) / 2; }
        juce::Rectangle<int> inNode() const  { return { margin, nodeY(), endWidth, tileHeight }; }
        juce::Rectangle<int> outNode() const { return { idealWidth() - margin - endWidth, nodeY(), endWidth, tileHeight }; }

        // 테두리와 오류 표시는 블록 타일(Tile::paintOverChildren)과 같은 규칙: 선택 = 흰 테두리,
        // 장치에 없는 채널 = 굵은 빨간 대각선 + 빨간 테두리 (선택까지 되어 있으면 그 바깥에 흰 테두리)
        static void drawEndNode (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, const juce::String& channels,
                                 bool selected, bool failed)
        {
            // 색 규칙은 블록 타일과 같다: 바탕 = 색.darker (0.6), 제목 = 색.brighter (0.4), 테두리 = 색
            const auto bounds = area.toFloat().reduced (1.5f);
            g.setColour (endpointColour.darker (0.6f));
            g.fillRoundedRectangle (bounds, corner);

            auto text = area.withSizeKeepingCentre (area.getWidth() - 8, 34);
            g.setColour (endpointColour.brighter (0.4f));
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (title, text.removeFromTop (17), juce::Justification::centred);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (12.0f));
            g.drawFittedText (channels, text, juce::Justification::centred, 1, 1.0f);   // 찌그러뜨리지 않고 말줄임

            if (failed)
            {
                const auto inner = selected ? bounds.reduced (2.5f) : bounds;
                drawErrorSlash (g, inner);
                g.setColour (juce::Colours::red);
                g.drawRoundedRectangle (inner.reduced (errorThickness * 0.5f - 0.75f), corner, errorThickness);
            }

            if (selected || ! failed)
            {
                g.setColour (selected ? juce::Colours::white : endpointColour);
                g.drawRoundedRectangle (bounds, corner, selected ? 2.5f : 1.5f);
            }
        }

        RigModel& model;
        const bool inputFailed, outputFailed;
        juce::OwnedArray<Tile> tiles;
        juce::OwnedArray<PlusButton> plusButtons;
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

        // 패치의 채널이 지금 장치에 없으면 빗금 (장치는 모든 채널을 열어 둔다. 장치가 없으면 둘 다 없음)
        int numInputs = 0, numOutputs = 0;

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            numInputs = device->getActiveInputChannels().countNumberOfSetBits();
            numOutputs = device->getActiveOutputChannels().countNumberOfSetBits();
        }

        const auto& routing = model.getRouting();
        const bool inputFailed = routing.input >= numInputs;
        const bool outputFailed = routing.output + (routing.stereoOutput ? 2 : 1) > numOutputs;

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
