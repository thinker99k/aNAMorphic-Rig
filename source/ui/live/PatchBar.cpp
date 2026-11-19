#include "PatchBar.h"

namespace rig
{
    namespace
    {
        constexpr int gap = 6;          // 바깥 여백, 버튼 사이 (SceneBar 와 같은 값)
        constexpr int iconSize = 22;
        constexpr int iconGap = 16;     // 아이콘과 글자 묶음 사이
        constexpr int textGap = 2;      // 위 글자와 아래 글자 사이
        constexpr int textInset = 12;   // 글자 묶음이 버튼 가장자리에 닿지 않게

        // 색 (시안)
        const juce::Colour barFill    { 0xff323c44 };
        const juce::Colour border     { 0xff4c5860 };
        const juce::Colour liveFill   { 0xff1c3b22 };
        const juce::Colour liveGreen  { 0xff35c435 };
        const juce::Colour stepFill   { 0xff2a343b };
        const juce::Colour stepBorder { 0xff8a949b };
        const juce::Colour textLight  { 0xfff2f4f5 };
        const juce::Colour textMuted  { 0xffaeb7bd };

        juce::Font boldFont (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }

        // 앱 기본 글꼴에 SemiBold 가 있으면 쓰고, 없으면 보통 굵기
        juce::Font semiBoldFont (float size)
        {
            static const juce::String style = []
            {
                const auto family = juce::Font (juce::FontOptions()).getTypefacePtr()->getName();
                for (const auto& s : juce::Font::findAllTypefaceStyles (family))
                    if (s.removeCharacters (" -").equalsIgnoreCase ("SemiBold"))
                        return s;
                return juce::String ("Regular");
            }();

            return juce::Font (juce::FontOptions (size).withStyle (style));
        }

        const juce::String emDash (juce::CharPointer_UTF8 ("\xe2\x80\x94"));   // —

        // 22 x 22 안에 채워진 삼각형 (◀ / ▶)
        void drawTriangle (juce::Graphics& g, juce::Rectangle<float> box, bool pointsLeft)
        {
            const auto inner = box.reduced (4.0f);
            juce::Path p;

            if (pointsLeft)
                p.addTriangle (inner.getRight(), inner.getY(), inner.getRight(), inner.getBottom(), inner.getX(), inner.getCentreY());
            else
                p.addTriangle (inner.getX(), inner.getY(), inner.getX(), inner.getBottom(), inner.getRight(), inner.getCentreY());

            g.fillPath (p);
        }

        // PREV / NEXT 버튼: [아이콘] [간격] [위 글자 / 아래 글자] 묶음을 버튼 안 가운데에 (NEXT 는 좌우 반대)
        void drawStepButton (juce::Graphics& g, juce::Rectangle<int> bounds, bool isPrev, bool enabled, const juce::String& bottomText,
                             const juce::Font& topFont, const juce::Font& bottomFont)
        {
            const auto area = bounds.toFloat();
            g.setColour (stepFill);
            g.fillRoundedRectangle (area, 6.0f);
            g.setColour (enabled ? stepBorder : border);
            g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

            const juce::String topText = isPrev ? "PREV PATCH" : "NEXT PATCH";

            const float maxLabelWidth = juce::jmax (0.0f, area.getWidth() - (float) (iconSize + iconGap + 2 * textInset));
            const float labelWidth = juce::jmin (maxLabelWidth, juce::jmax (juce::GlyphArrangement::getStringWidth (topFont, topText),
                                                                            juce::GlyphArrangement::getStringWidth (bottomFont, bottomText)));
            const float groupWidth = (float) (iconSize + iconGap) + labelWidth;
            const float left = area.getCentreX() - groupWidth / 2.0f;

            const juce::Rectangle<float> icon (isPrev ? left : left + labelWidth + (float) iconGap,
                                               area.getCentreY() - (float) iconSize / 2.0f, (float) iconSize, (float) iconSize);
            const float labelX = isPrev ? left + (float) (iconSize + iconGap) : left;
            const float textTop = area.getCentreY() - (topFont.getHeight() + (float) textGap + bottomFont.getHeight()) / 2.0f;
            const auto justification = isPrev ? juce::Justification::centredLeft : juce::Justification::centredRight;

            g.setColour (enabled ? textLight : textMuted);
            drawTriangle (g, icon, isPrev);

            g.setColour (textMuted);
            g.setFont (topFont);
            g.drawText (topText, juce::Rectangle<float> (labelX, textTop, labelWidth, topFont.getHeight()), justification, true);

            g.setColour (enabled ? textLight : textMuted);
            g.setFont (bottomFont);
            g.drawText (bottomText, juce::Rectangle<float> (labelX, textTop + topFont.getHeight() + (float) textGap, labelWidth, bottomFont.getHeight()),
                        justification, true);
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
        // 파일은 여기서 읽지 않는다: 숨겨져 있으면 repaint 는 무시되고, 다시 보일 때 paint 에서 읽는다
        if (model.getPatchNumber() != shownPatch)
            repaint();
    }

    void PatchBar::refreshNeighbors()
    {
        if (model.getPatchNumber() == shownPatch)
            return;

        shownPatch = model.getPatchNumber();
        hasPrev = shownPatch > 1;
        hasNext = shownPatch < RigModel::numPatches;

        const auto describe = [] (int n) { return juce::String (n).paddedLeft ('0', 2) + "  " + RigModel::readPatchName (n); };
        prevText = hasPrev ? describe (shownPatch - 1) : emDash;
        nextText = hasNext ? describe (shownPatch + 1) : emDash;
    }

    void PatchBar::resized()
    {
        auto area = getLocalBounds().reduced (gap);
        liveBounds = area.removeFromLeft (area.getHeight());   // 정사각형, SceneBar 의 Live 버튼과 같은 자리
        area.removeFromLeft (gap);
        prevBounds = area.removeFromLeft ((area.getWidth() - gap) / 2);
        area.removeFromLeft (gap);
        nextBounds = area;
    }

    void PatchBar::paint (juce::Graphics& g)
    {
        refreshNeighbors();   // 패치가 바뀐 뒤 처음 그릴 때만 파일을 읽는다

        g.fillAll (barFill);
        g.setColour (border);
        g.drawRect (getLocalBounds(), 1);

        // Live ON: "Live" 14 px 아래 "ON" 10 px, 줄 간격 2 px, 묶음을 가운데에
        const auto liveArea = liveBounds.toFloat();
        g.setColour (liveFill);
        g.fillRoundedRectangle (liveArea, 5.0f);
        g.setColour (liveGreen);
        g.drawRoundedRectangle (liveArea.reduced (1.0f), 5.0f, 2.0f);

        const auto liveFont = boldFont (14.0f);
        const auto onFont = boldFont (10.0f);
        const float liveTop = liveArea.getCentreY() - (liveFont.getHeight() + (float) textGap + onFont.getHeight()) / 2.0f;
        g.setFont (liveFont);
        g.drawText ("Live", juce::Rectangle<float> (liveArea.getX(), liveTop, liveArea.getWidth(), liveFont.getHeight()),
                    juce::Justification::centred, false);
        g.setFont (onFont);
        g.drawText ("ON", juce::Rectangle<float> (liveArea.getX(), liveTop + liveFont.getHeight() + (float) textGap, liveArea.getWidth(), onFont.getHeight()),
                    juce::Justification::centred, false);

        // 두 버튼이 같은 글꼴을 쓰므로 한 번만 만든다
        const auto stepTopFont = boldFont (11.0f);
        const auto stepBottomFont = semiBoldFont (18.0f);
        drawStepButton (g, prevBounds, true, hasPrev, prevText, stepTopFont, stepBottomFont);
        drawStepButton (g, nextBounds, false, hasNext, nextText, stepTopFont, stepBottomFont);
    }

    void PatchBar::mouseDown (const juce::MouseEvent& e)
    {
        if (! isLeftClick (e))
            return;

        refreshNeighbors();   // 다시 그리기 전에 눌려도 hasPrev / hasNext 가 지금 패치 기준이도록

        const auto pos = e.getPosition();

        if (liveBounds.contains (pos))
        {
            if (onLiveOff)
                onLiveOff();
        }
        else if (prevBounds.contains (pos))
        {
            if (hasPrev && onPatchStep)
                onPatchStep (-1);
        }
        else if (nextBounds.contains (pos))
        {
            if (hasNext && onPatchStep)
                onPatchStep (1);
        }
    }
}
