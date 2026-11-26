#include "ScenePads.h"

namespace rig
{
    namespace
    {
        constexpr int columns = 4, rows = 2;
        constexpr int padGap = 10;      // 패드 사이 (가로·세로)
        constexpr int padMargin = padGap;   // 바깥 여백 (위·아래·좌·우): 패드 사이와 같게
        constexpr int padInset = 8;     // 패드 안 여백 (번호·이름 모두 사방 같은 값)
        constexpr float numberSize = 24.0f, nameSize = 32.0f;   // 이름이 주인공, 번호는 작게 (번호 = Stomp 패드 종류 글자, 이름 = Stomp 파일명과 같은 크기)
        constexpr float stompScale = 2.0f;  // Stomp 패드 = 블록 모양을 이만큼 키워서 (머리띠 40)
        constexpr float stompHeaderSize = 24.0f;   // Stomp 패드 종류 (굵게). 파일명은 씬 이름과 같은 글꼴 (nameSize, 보통)
        static_assert (RigModel::numScenes == columns * rows);

        // 색 (시안)
        const juce::Colour activeFill   { 0xffd0d0d0 };
        const juce::Colour activeBorder { 0xfff2f4f5 };
        const juce::Colour padFill      { 0xff3b454d };
        const juce::Colour padBorder    { 0xff4c5860 };
        const juce::Colour textDark     { 0xff1c2328 };
        const juce::Colour textLight    { 0xfff2f4f5 };

        juce::Font boldFont (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }
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
        repaint();   // 현재 씬 / 켜진 Stomp 강조와 이름
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
        const bool stomp = model.isStompMode();

        // 글꼴은 패드마다 만들지 않고 한 번만
        const auto numberFont = boldFont (numberSize);
        const auto nameFont = juce::Font (juce::FontOptions (nameSize));   // 씬 이름·Stomp 파일명 모두 보통 굵기

        for (int i = 0; i < RigModel::numScenes; ++i)
        {
            const auto pad = padBounds[(size_t) i];
            const auto area = pad.toFloat();

            // Stomp: 묶인 패드 = 블록 모양 (번호·중요 값 없음, 꺼지면 흐리게), 안 묶인 패드 = 빈 패드
            if (stomp)
            {
                if (const auto* bound = model.getStompSlot (i))
                {
                    block::drawStomp (g, pad, bound->type, RigModel::shortName (*bound), ! model.isStompOn (i), stompScale,
                                      boldFont (stompHeaderSize), nameFont);
                }
                else
                {
                    g.setColour (padFill);
                    g.fillRect (area);
                    g.setColour (padBorder);
                    strokeRect (g, area.reduced (0.5f), 1.0f);
                }

                continue;
            }

            const bool active = i == current;
            const float thickness = active ? 2.0f : 1.0f;

            g.setColour (active ? activeFill : padFill);
            g.fillRect (area);
            g.setColour (active ? activeBorder : padBorder);
            strokeRect (g, area.reduced (thickness / 2.0f), thickness);

            const auto inner = pad.reduced (padInset);
            const auto textColour = active ? textDark : textLight;
            g.setColour (textColour);
            block::drawBandLabel (g, juce::String (i + 1), numberFont, pad, stompScale);   // Stomp 패드 종류 글자와 같은 자리

            // 이름: 가운데 (고정 글꼴로 줄마다 채우고 넘치면 말줄임. 번호 줄만큼 위아래를 똑같이 비운다)
            drawWrappedText (g, model.getSceneName (i), inner.reduced (0, juce::roundToInt (numberFont.getHeight())).toFloat(), nameFont);
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
                if (model.isStompMode())
                    model.toggleStomp (i);
                else if (i != model.getCurrentScene())
                    model.selectScene (i);
                return;
            }
        }
    }
}
