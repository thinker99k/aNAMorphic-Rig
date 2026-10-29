#include "ChainView.h"

namespace rig
{
    namespace
    {
        constexpr int tileWidth = 150;
        constexpr int gap = 28;          // 블록 사이 화살표 공간
        constexpr int endWidth = 44;     // IN / OUT 표시 폭

        juce::Colour colourFor (BlockType t)
        {
            return t == BlockType::Nam ? juce::Colour (0xffd9822b) : juce::Colour (0xff2b9bd9);
        }

        void drawArrow (juce::Graphics& g, float x1, float x2, float y)
        {
            g.setColour (juce::Colours::grey);
            g.drawArrow ({ x1, y, x2, y }, 2.0f, 8.0f, 8.0f);
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

            bypassButton.setClickingTogglesState (true);
            bypassButton.setToggleState (! slot.bypassed, juce::dontSendNotification);
            bypassButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::green.darker());
            bypassButton.onClick = [this] { model.setBypassed (index, ! bypassButton.getToggleState()); };

            leftButton.onClick   = [this] { model.moveBlock (index, -1); };
            rightButton.onClick  = [this] { model.moveBlock (index, +1); };
            removeButton.onClick = [this] { model.removeBlock (index); };

            leftButton.setEnabled (index > 0);
            rightButton.setEnabled (index < (int) model.getSlots().size() - 1);

            for (auto* b : { &bypassButton, &leftButton, &rightButton, &removeButton })
                addAndMakeVisible (b);
        }

        void mouseDown (const juce::MouseEvent&) override { model.select (index); }

        void paint (juce::Graphics& g) override
        {
            const auto& slot = model.getSlots()[(size_t) index];
            const bool selected = model.getSelectedIndex() == index;
            auto bounds = getLocalBounds().toFloat().reduced (1.0f);

            auto base = colourFor (slot.type);
            if (slot.bypassed)
                base = base.withSaturation (0.1f).darker (0.5f);

            g.setColour (base.darker (0.6f));
            g.fillRoundedRectangle (bounds, 8.0f);
            g.setColour (selected ? juce::Colours::white : base);
            g.drawRoundedRectangle (bounds, 8.0f, selected ? 3.0f : 1.5f);

            auto text = getLocalBounds().reduced (10).withTrimmedBottom (34);

            g.setColour (base.brighter (0.4f));
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText ("#" + juce::String (index + 1) + "  " + (slot.type == BlockType::Nam ? "NAM" : "IR"),
                        text.removeFromTop (18), juce::Justification::centredLeft);

            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (14.0f));
            const auto name = slot.file == juce::File() ? juce::String ("(no file)") : slot.file.getFileNameWithoutExtension();
            g.drawFittedText (name, text.removeFromTop (40), juce::Justification::topLeft, 2);

            g.setFont (juce::FontOptions (12.0f));
            g.setColour (slot.error.isNotEmpty() ? juce::Colours::red.brighter() : juce::Colours::lightgrey);
            const auto status = slot.loading                ? juce::String ("loading...")
                              : slot.error.isNotEmpty()      ? juce::String ("error")
                              : slot.dsp != nullptr          ? (slot.bypassed ? juce::String ("bypassed") : juce::String ("active"))
                                                             : juce::String ("pass-through");
            g.drawText (status, text.removeFromTop (16), juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto row = getLocalBounds().reduced (8).removeFromBottom (26);
            const int w = row.getWidth() / 4;
            leftButton.setBounds (row.removeFromLeft (w).reduced (1));
            bypassButton.setBounds (row.removeFromLeft (w).reduced (1));
            rightButton.setBounds (row.removeFromLeft (w).reduced (1));
            removeButton.setBounds (row.reduced (1));
        }

    private:
        RigModel& model;
        const int index;
        juce::TextButton bypassButton { "On" }, leftButton { "<" }, rightButton { ">" }, removeButton { "x" };
    };

    //==============================================================================
    // 뷰포트 안에서 가로로 늘어나는 내용물
    class ChainView::Strip final : public juce::Component
    {
    public:
        Strip (RigModel& m, std::function<void()> onAdd) : model (m)
        {
            addButton.onClick = std::move (onAdd);
            addAndMakeVisible (addButton);

            for (int i = 0; i < (int) model.getSlots().size(); ++i)
                addAndMakeVisible (tiles.add (new Tile (model, i)));
        }

        int idealWidth() const
        {
            return 8 + endWidth + (tiles.size() + 1) * (gap + tileWidth) + gap + endWidth + 8;
        }

        void paint (juce::Graphics& g) override
        {
            const float midY = (float) getHeight() * 0.5f;

            g.setColour (juce::Colours::lightgrey);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText ("IN", 8, (int) midY - 10, endWidth, 20, juce::Justification::centred);

            // IN -> 타일들 -> [+ Add] -> OUT : 각 요소 앞에 화살표
            float x = 8.0f + endWidth;

            for (int i = 0; i <= tiles.size(); ++i)
            {
                drawArrow (g, x + 2.0f, x + gap - 2.0f, midY);
                x += gap + tileWidth;
            }

            drawArrow (g, x + 2.0f, x + gap - 2.0f, midY);

            g.setColour (juce::Colours::lightgrey);
            g.drawText ("OUT", (int) (x + gap), (int) midY - 10, endWidth, 20, juce::Justification::centred);
        }

        void resized() override
        {
            const int tileHeight = juce::jmin (150, getHeight() - 24);
            const int y = (getHeight() - tileHeight) / 2;
            int x = 8 + endWidth + gap;

            for (auto* t : tiles)
            {
                t->setBounds (x, y, tileWidth, tileHeight);
                x += tileWidth + gap;
            }

            addButton.setBounds (x, y + tileHeight / 2 - 20, tileWidth, 40);
        }

    private:
        RigModel& model;
        juce::OwnedArray<Tile> tiles;
        juce::TextButton addButton { "+ Add block" };
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
        strip = std::make_unique<Strip> (model, [this] { showAddMenu(); });
        viewport.setViewedComponent (strip.get(), false);
        resized();
        viewport.setViewPosition (scroll);
    }

    void ChainView::showAddMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Add block");
        menu.addItem (1, "NAM capture (.nam)");
        menu.addItem (2, "Impulse response (.wav)");

        juce::Component::SafePointer<ChainView> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options(), [safeThis] (int result)
        {
            if (safeThis == nullptr || result == 0)
                return;

            safeThis->model.addBlock (result == 1 ? BlockType::Nam : BlockType::Ir);

            // 새 블록이 보이도록 오른쪽 끝으로 스크롤
            auto& vp = safeThis->viewport;
            vp.setViewPosition (juce::jmax (0, vp.getViewedComponent()->getWidth() - vp.getViewWidth()), 0);
        });
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
