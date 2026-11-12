#include "ChainView.h"

namespace rig
{
    namespace
    {
        constexpr int tileWidth = 150;
        constexpr int tileHeight = 72;
        constexpr int endWidth = 64;     // IN / OUT 노드 폭
        constexpr int gap = 44;          // 노드 사이 연결선 공간 ([+] 포함)
        constexpr int plusSize = 18;
        constexpr int margin = 8;
        constexpr float corner = 8.0f;

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

        // 블록 안쪽을 빨간 빗금으로 채운다 (로딩 실패)
        void drawErrorHatch (juce::Graphics& g, juce::Rectangle<float> bounds)
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (bounds, corner);
            g.reduceClipRegion (clip);

            g.setColour (juce::Colours::red.withAlpha (0.35f));
            const float h = bounds.getHeight();

            for (float x = bounds.getX() - h; x < bounds.getRight(); x += 10.0f)
                g.drawLine (x, bounds.getBottom(), x + h, bounds.getY(), 2.0f);
        }
    }

    //==============================================================================
    // 블록 하나를 나타내는 타일
    class ChainView::Tile final : public juce::Component
    {
    public:
        Tile (RigModel& m, int i) : model (m), index (i)
        {
            const auto& slot = model.getSlots()[(size_t) index];

            onButton.setClickingTogglesState (true);
            onButton.setToggleState (slot.on, juce::dontSendNotification);
            onButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::green.darker());
            onButton.onClick = [this] { model.setOn (index, onButton.getToggleState()); };

            leftButton.onClick   = [this] { model.moveBlock (index, -1); };
            rightButton.onClick  = [this] { model.moveBlock (index, +1); };
            removeButton.onClick = [this] { model.removeBlock (index); };

            onButton.setEnabled (slot.type != BlockType::None);   // 미지정 블록은 지정할 때까지 끌 수 없다
            leftButton.setEnabled (index > 0);
            rightButton.setEnabled (index < (int) model.getSlots().size() - 1);

            for (auto* b : { &onButton, &leftButton, &rightButton, &removeButton })
                addAndMakeVisible (b);
        }

        void mouseDown (const juce::MouseEvent&) override { model.select (index); }

        void paint (juce::Graphics& g) override
        {
            const auto& slot = model.getSlots()[(size_t) index];
            const bool selected = model.getSelectedIndex() == index;
            const bool failed = slot.error.isNotEmpty();
            auto bounds = getLocalBounds().toFloat().reduced (1.5f);

            auto base = colourFor (slot.type);
            if (! slot.on && slot.type != BlockType::None)
                base = base.withSaturation (0.1f).darker (0.5f);

            g.setColour (slot.type == BlockType::None ? juce::Colours::black : base.darker (0.6f));
            g.fillRoundedRectangle (bounds, corner);

            if (failed)
                drawErrorHatch (g, bounds);

            // 테두리: 선택 = 흰색, 오류 = 빨강 (둘 다면 흰 테두리 안쪽에 빨강)
            if (selected)
                g.setColour (juce::Colours::white);
            else if (failed)
                g.setColour (juce::Colours::red);
            else
                g.setColour (slot.type == BlockType::None ? juce::Colours::grey.darker() : base);

            g.drawRoundedRectangle (bounds, corner, selected || failed ? 2.5f : 1.5f);

            if (selected && failed)
            {
                g.setColour (juce::Colours::red);
                g.drawRoundedRectangle (bounds.reduced (3.0f), corner - 2.0f, 2.0f);
            }

            auto text = getLocalBounds().reduced (10, 6).withTrimmedBottom (24);

            g.setColour (slot.type == BlockType::None ? juce::Colours::grey : base.brighter (0.4f));
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText ("#" + juce::String (index + 1) + "  " + labelFor (slot.type),
                        text.removeFromTop (16), juce::Justification::centredLeft);

            g.setColour (slot.type == BlockType::None ? juce::Colours::grey : juce::Colours::white);
            g.setFont (juce::FontOptions (13.0f));
            const auto name = slot.type == BlockType::None ? juce::String ("Empty") : slot.file.getFileNameWithoutExtension();
            g.drawText (name, text, juce::Justification::centredLeft, true);
        }

        void resized() override
        {
            auto row = getLocalBounds().reduced (6).removeFromBottom (22);
            const int w = row.getWidth() / 4;
            leftButton.setBounds (row.removeFromLeft (w).reduced (1));
            onButton.setBounds (row.removeFromLeft (w).reduced (1));
            rightButton.setBounds (row.removeFromLeft (w).reduced (1));
            removeButton.setBounds (row.reduced (1));
        }

    private:
        RigModel& model;
        const int index;
        juce::TextButton onButton { "On" }, leftButton { "<" }, rightButton { ">" }, removeButton { "x" };
    };

    //==============================================================================
    // 뷰포트 안에서 가로로 늘어나는 내용물: IN, 블록들, OUT 노드와 그 사이 연결선/[+]
    class ChainView::Strip final : public juce::Component
    {
    public:
        explicit Strip (RigModel& m) : model (m)
        {
            const int numBlocks = (int) model.getSlots().size();

            for (int i = 0; i < numBlocks; ++i)
                addAndMakeVisible (tiles.add (new Tile (model, i)));

            // 연결선마다 하나: i 번째 [+] 는 블록 i 자리에 넣는다
            for (int i = 0; i <= numBlocks; ++i)
            {
                auto* plus = plusButtons.add (new juce::TextButton ("+"));
                plus->onClick = [this, i] { model.insertBlock (i); };
                addAndMakeVisible (plus);
            }
        }

        int idealWidth() const
        {
            return margin + endWidth + tiles.size() * (gap + tileWidth) + gap + endWidth + margin;
        }

        juce::Rectangle<int> tileBounds (int index) const
        {
            return tiles[index] != nullptr ? tiles[index]->getBounds() : juce::Rectangle<int>();
        }

        void paint (juce::Graphics& g) override
        {
            const auto y = nodeY();
            const juce::Rectangle<int> inNode (margin, y, endWidth, tileHeight);
            const juce::Rectangle<int> outNode (idealWidth() - margin - endWidth, y, endWidth, tileHeight);

            drawEndNode (g, inNode, "IN");
            drawEndNode (g, outNode, "OUT");

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
                plusButtons[i]->setBounds (x + (gap - plusSize) / 2, y + (tileHeight - plusSize) / 2, plusSize, plusSize);
                x += gap;

                if (auto* t = tiles[i])
                {
                    t->setBounds (x, y, tileWidth, tileHeight);
                    x += tileWidth;
                }
            }
        }

    private:
        int nodeY() const { return (getHeight() - tileHeight) / 2; }

        static void drawEndNode (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
        {
            const auto bounds = area.toFloat().reduced (1.5f);
            g.setColour (juce::Colours::darkgrey.darker (0.5f));
            g.fillRoundedRectangle (bounds, corner);
            g.setColour (juce::Colours::grey);
            g.drawRoundedRectangle (bounds, corner, 1.5f);

            g.setColour (juce::Colours::lightgrey);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (text, area, juce::Justification::centred);
        }

        RigModel& model;
        juce::OwnedArray<Tile> tiles;
        juce::OwnedArray<juce::TextButton> plusButtons;
    };

    //==============================================================================
    ChainView::ChainView (RigModel& m) : model (m)
    {
        viewport.setScrollBarsShown (false, true);
        addAndMakeVisible (viewport);
        model.addChangeListener (this);
        rebuild();
    }

    ChainView::~ChainView()
    {
        model.removeChangeListener (this);
    }

    void ChainView::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        rebuild();
    }

    void ChainView::rebuild()
    {
        // 블록 수가 작으므로 변경 때마다 통째로 다시 만든다 (단순함 우선)
        const auto scroll = viewport.getViewPosition();
        strip = std::make_unique<Strip> (model);
        viewport.setViewedComponent (strip.get(), false);
        resized();
        viewport.setViewPosition (scroll);

        // 새로 선택된 블록(방금 넣은 블록 등)이 화면 밖이면 보이도록 스크롤
        const auto* selected = model.getSelectedSlot();
        const auto selectedId = selected != nullptr ? selected->id : 0;

        if (selectedId != shownSelectedId)
        {
            shownSelectedId = selectedId;
            const auto tile = strip->tileBounds (model.getSelectedIndex());
            const auto view = viewport.getViewArea();

            if (! tile.isEmpty() && (tile.getX() < view.getX() || tile.getRight() > view.getRight()))
                viewport.setViewPosition (juce::jmax (0, tile.getCentreX() - view.getWidth() / 2), 0);
        }
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
