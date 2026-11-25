#include "PatchBar.h"

#include "ui/live/LiveStyle.h"

namespace rig
{
    namespace
    {
        constexpr int units = 8;   // 폭 비율: 이전 1, 이름 6, 다음 1
        constexpr int iconSize = 34;
        constexpr int iconGap = 10;        // 아이콘과 번호 사이
        constexpr int triangleInset = 4;   // 아이콘 칸 안에서 삼각형이 줄어드는 만큼
        constexpr float numberFontSize = 40.0f;   // ScenePads 의 씬 번호와 같은 글꼴
        constexpr float nameFontSize = 40.0f;
        constexpr int nameInset = 15;      // 이름 칸 안쪽 좌우 여백

        // iconSize x iconSize 안에 채워진 삼각형 (◀ / ▶)
        void drawTriangle (juce::Graphics& g, juce::Rectangle<float> box, bool pointsLeft)
        {
            const auto inner = box.reduced ((float) triangleInset);
            juce::Path p;

            if (pointsLeft)
                p.addTriangle (inner.getRight(), inner.getY(), inner.getRight(), inner.getBottom(), inner.getX(), inner.getCentreY());
            else
                p.addTriangle (inner.getX(), inner.getY(), inner.getX(), inner.getBottom(), inner.getRight(), inner.getCentreY());

            g.fillPath (p);
        }

        // 이전 / 다음 버튼: [◀ 03] / [05 ▶] 를 한 덩어리로 가운데에. 이웃이 없으면 화살표만 흐리게
        void drawStepButton (juce::Graphics& g, juce::Rectangle<int> bounds, bool isPrev, int neighbour, bool enabled)
        {
            const auto area = bounds.toFloat();
            live::drawButtonBox (g, area, enabled ? live::stepBorder : live::border);

            const auto font = live::boldFont (numberFontSize);
            const auto number = enabled ? juce::String (neighbour).paddedLeft ('0', 2) : juce::String();
            const float numberWidth = number.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (font, number);
            const float groupWidth = (float) iconSize + (number.isEmpty() ? 0.0f : (float) iconGap + numberWidth);
            const float left = area.getCentreX() - groupWidth * 0.5f;

            const float iconX = isPrev ? left : left + groupWidth - (float) iconSize;
            const juce::Rectangle<float> icon (iconX, area.getCentreY() - (float) iconSize / 2.0f, (float) iconSize, (float) iconSize);

            g.setColour (enabled ? live::textLight : live::textMuted);
            drawTriangle (g, icon, isPrev);

            if (number.isNotEmpty())
            {
                const float numberX = isPrev ? icon.getRight() + (float) iconGap : left;
                g.setFont (font);
                g.drawText (number, juce::Rectangle<float> (numberX, area.getY(), numberWidth + 1.0f, area.getHeight()),
                            juce::Justification::centredLeft, false);
            }
        }
    }

    PatchBar::PatchBar (RigModel& m) : model (m)
    {
        model.addChangeListener (this);
    }

    PatchBar::~PatchBar()
    {
        model.removeChangeListener (this);
    }

    void PatchBar::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        repaint();   // 패치 번호와 이름
    }

    void PatchBar::resized()
    {
        auto area = getLocalBounds().reduced (live::gap);
        const int unit = (area.getWidth() - 2 * live::gap) / units;
        prevBounds = area.removeFromLeft (unit);
        nextBounds = area.removeFromRight (unit);
        area.reduce (live::gap, 0);
        nameBounds = area;
    }

    void PatchBar::paint (juce::Graphics& g)
    {
        g.fillAll (live::barFill);
        g.setColour (live::border);
        g.drawRect (getLocalBounds(), 1);

        const int patch = model.getPatchNumber();
        drawStepButton (g, prevBounds, true, patch - 1, hasPrev());
        drawStepButton (g, nextBounds, false, patch + 1, hasNext());

        g.setColour (live::textLight);
        g.setFont (live::semiBoldFont (nameFontSize));
        g.drawText (model.getPatchName(), nameBounds.reduced (nameInset, 0), juce::Justification::centred, true);
    }

    void PatchBar::mouseDown (const juce::MouseEvent& e)
    {
        if (! isLeftClick (e) || onPatchStep == nullptr)
            return;

        const auto pos = e.getPosition();

        if (prevBounds.contains (pos) && hasPrev())
            onPatchStep (-1);
        else if (nextBounds.contains (pos) && hasNext())
            onPatchStep (1);
    }
}
