#include "PatchBar.h"

namespace rig
{
    namespace
    {
        constexpr int gap = 6;          // 바깥 여백, 버튼 사이 (SceneBar 와 같은 값)
        constexpr int iconSize = 34;
        constexpr int iconGap = 10;     // 아이콘과 번호 사이
        constexpr int textGap = 2;      // 위 글자와 아래 글자 사이
        constexpr int textInset = 12;   // 아이콘·글자가 버튼(칸) 가장자리에 닿지 않게
        constexpr float numberShare = 0.25f;   // 버튼 폭 중 아이콘 + 번호 칸 비율 (나머지는 글자 칸)

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

        // iconSize x iconSize 안에 채워진 삼각형 (◀ / ▶)
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

        // PREV / NEXT 버튼: 위치는 버튼 폭으로만 정해진다 (이름 길이와 무관)
        //  - PREV: [왼쪽 25%: ◀ 번호] [나머지 75%: 위 글자 / 아래 글자]
        //  - NEXT: [왼쪽 75%: 위 글자 / 아래 글자] [오른쪽 25%: 번호 ▶]
        //  - 아이콘은 칸의 바깥 가장자리에 붙이고 번호는 그 안쪽 옆. 글자는 두 줄 모두 왼쪽 정렬, 넘치면 "..."
        //  - 번호가 비어 있으면 (이웃 패치 없음) 아이콘만
        void drawStepButton (juce::Graphics& g, juce::Rectangle<int> bounds, bool isPrev, bool enabled,
                             const juce::String& numberText, const juce::String& bottomText,
                             const juce::Font& numberFont, const juce::Font& topFont, const juce::Font& bottomFont)
        {
            const auto area = bounds.toFloat();
            g.setColour (stepFill);
            g.fillRoundedRectangle (area, 6.0f);
            g.setColour (enabled ? stepBorder : border);
            g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

            auto labelArea = area;
            const auto numberArea = isPrev ? labelArea.removeFromLeft (area.getWidth() * numberShare)
                                           : labelArea.removeFromRight (area.getWidth() * numberShare);

            // 아이콘 + 번호 칸
            const float iconX = isPrev ? numberArea.getX() + (float) textInset : numberArea.getRight() - (float) textInset - (float) iconSize;
            const juce::Rectangle<float> icon (iconX, area.getCentreY() - (float) iconSize / 2.0f, (float) iconSize, (float) iconSize);
            const auto numberBox = isPrev ? numberArea.withLeft (icon.getRight() + (float) iconGap)
                                          : numberArea.withRight (icon.getX() - (float) iconGap);

            g.setColour (enabled ? textLight : textMuted);
            drawTriangle (g, icon, isPrev);

            g.setFont (numberFont);
            g.drawText (numberText, numberBox, isPrev ? juce::Justification::centredLeft : juce::Justification::centredRight, false);

            // 글자 칸
            const juce::String topText = isPrev ? "PREV PATCH" : "NEXT PATCH";
            const auto textArea = labelArea.reduced ((float) textInset, 0.0f);
            const float textTop = area.getCentreY() - (topFont.getHeight() + (float) textGap + bottomFont.getHeight()) / 2.0f;

            g.setColour (textMuted);
            g.setFont (topFont);
            g.drawText (topText, textArea.withY (textTop).withHeight (topFont.getHeight()), juce::Justification::centredLeft, true);

            g.setColour (enabled ? textLight : textMuted);
            g.setFont (bottomFont);
            g.drawText (bottomText, textArea.withY (textTop + topFont.getHeight() + (float) textGap).withHeight (bottomFont.getHeight()),
                        juce::Justification::centredLeft, true);
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

        prevNumber = hasPrev ? juce::String (shownPatch - 1).paddedLeft ('0', 2) : juce::String();
        nextNumber = hasNext ? juce::String (shownPatch + 1).paddedLeft ('0', 2) : juce::String();
        prevText = hasPrev ? RigModel::readPatchName (shownPatch - 1) : emDash;
        nextText = hasNext ? RigModel::readPatchName (shownPatch + 1) : emDash;
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
        const auto stepNumberFont = boldFont (40.0f);   // ScenePads 의 씬 번호와 같은 글꼴
        const auto stepTopFont = boldFont (11.0f);
        const auto stepBottomFont = semiBoldFont (28.0f);
        drawStepButton (g, prevBounds, true, hasPrev, prevNumber, prevText, stepNumberFont, stepTopFont, stepBottomFont);
        drawStepButton (g, nextBounds, false, hasNext, nextNumber, nextText, stepNumberFont, stepTopFont, stepBottomFont);
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
