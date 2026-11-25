#include "ScenePads.h"

namespace rig
{
    namespace
    {
        constexpr int columns = 4, rows = 2;
        constexpr int padMargin = 13;   // 바깥 여백 (위·아래·좌·우)
        constexpr int padGap = 10;      // 패드 사이 (가로·세로)
        constexpr int padInset = 8;     // 패드 안 여백 (번호·이름 모두 사방 같은 값)
        constexpr float padCorner = 6.0f;   // 둥근 모서리 반지름
        static_assert (RigModel::numScenes == columns * rows);

        // 색 (시안)
        const juce::Colour activeFill   { 0xffd0d0d0 };
        const juce::Colour activeBorder { 0xfff2f4f5 };
        const juce::Colour padFill      { 0xff3b454d };
        const juce::Colour padBorder    { 0xff4c5860 };
        const juce::Colour textDark     { 0xff1c2328 };
        const juce::Colour textLight    { 0xfff2f4f5 };

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
    }

    ScenePads::ScenePads (RigModel& m) : model (m)
    {
        model.addChangeListener (this);
    }

    ScenePads::~ScenePads()
    {
        model.removeChangeListener (this);
    }

    void ScenePads::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        repaint();   // 현재 씬 강조와 씬 이름
    }

    void ScenePads::resized()
    {
        // 여백과 간격은 고정, 남는 크기를 나눠 갖는다. float 로 계산한 뒤 가장자리를 반올림 (양 끝은 정확히 여백에 닿는다)
        const auto area = getLocalBounds().reduced (padMargin);
        const float cellW = (float) (area.getWidth() - (columns - 1) * padGap) / (float) columns;
        const float cellH = (float) (area.getHeight() - (rows - 1) * padGap) / (float) rows;

        for (int i = 0; i < RigModel::numScenes; ++i)
        {
            const int c = i % columns, r = i / columns;
            const float x = (float) area.getX() + (float) c * (cellW + (float) padGap);
            const float y = (float) area.getY() + (float) r * (cellH + (float) padGap);
            padBounds[(size_t) i] = juce::Rectangle<int>::leftTopRightBottom (juce::roundToInt (x), juce::roundToInt (y),
                                                                              juce::roundToInt (x + cellW), juce::roundToInt (y + cellH));
        }
    }

    void ScenePads::paint (juce::Graphics& g)
    {
        const int current = model.getCurrentScene();

        // 글꼴은 패드마다 만들지 않고 한 번만
        // 이름이 주인공, 번호는 작게
        const auto numberFont = boldFont (24.0f);
        const auto nameFont = semiBoldFont (40.0f);

        for (int i = 0; i < RigModel::numScenes; ++i)
        {
            const auto pad = padBounds[(size_t) i];
            const auto area = pad.toFloat();
            const bool active = i == current;
            const float thickness = active ? 2.0f : 1.0f;

            g.setColour (active ? activeFill : padFill);
            g.fillRoundedRectangle (area, padCorner);
            g.setColour (active ? activeBorder : padBorder);
            g.drawRoundedRectangle (area.reduced (thickness / 2.0f), padCorner, thickness);

            const auto inner = pad.reduced (padInset);
            g.setColour (active ? textDark : textLight);
            g.setFont (numberFont);
            g.drawText (juce::String (i + 1), inner, juce::Justification::topLeft, false);

            g.setFont (nameFont);
            g.drawText (model.getSceneName (i), inner, juce::Justification::centred, true);
        }
    }

    void ScenePads::mouseDown (const juce::MouseEvent& e)
    {
        if (! isLeftClick (e))
            return;

        for (int i = 0; i < RigModel::numScenes; ++i)
        {
            if (padBounds[(size_t) i].contains (e.getPosition()))
            {
                if (i != model.getCurrentScene())
                    model.selectScene (i);
                return;
            }
        }
    }
}
