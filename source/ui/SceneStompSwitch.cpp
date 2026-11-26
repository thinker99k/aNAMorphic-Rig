#include "SceneStompSwitch.h"

#include "ui/live/LiveStyle.h"

namespace rig
{
    namespace
    {
        constexpr float dividerInset = 8.0f;   // 가운데 구분선 양 끝과 버튼 가장자리 사이
        constexpr float dividerAlpha = 0.5f;
        constexpr float textNudge = 1.0f;      // 두 글자를 구분선 쪽으로 (Scene 아래로, Stomp 위로)
    }

    SceneStompSwitch::SceneStompSwitch (RigModel& m)
        : LeftClickOnly ("Scene / Stomp"), model (m)
    {
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
        onClick = [this]
        {
            model.setPlayMode (model.isStompMode() ? RigModel::PlayMode::scene : RigModel::PlayMode::stomp);
        };
        model.addChangeListener (this);
    }

    SceneStompSwitch::~SceneStompSwitch()
    {
        model.removeChangeListener (this);
    }

    void SceneStompSwitch::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        repaint();   // 모드가 바뀌면 (Edit 씬 바·Live 아래 줄 어느 쪽에서 바꿔도)
    }

    // 마우스를 올려도 강조하지 않는다 (Live 에서 쓰는 버튼)
    void SceneStompSwitch::paintButton (juce::Graphics& g, bool, bool)
    {
        const auto area = getLocalBounds().toFloat();
        const bool scene = ! model.isStompMode();

        indicator::drawBox (g, area, scene ? indicator::redFill : indicator::blueFill, scene ? indicator::red : indicator::blue);

        const auto offColour = live::textMuted.withAlpha (0.5f);
        const float fontSize = live::buttonFontSize (area.getHeight());   // 같은 줄 버튼 글자와 같은 크기 (16)

        const auto drawMode = [&] (const char* name, juce::Rectangle<float> half, bool current, juce::Colour colour)
        {
            g.setColour (current ? colour : offColour);
            g.setFont (juce::FontOptions (fontSize, current ? juce::Font::bold : juce::Font::plain));
            g.drawText (name, half, juce::Justification::centred, false);
        };

        auto top = area;
        const auto bottom = top.removeFromBottom (area.getHeight() * 0.5f);
        drawMode ("Scene", top.translated (0.0f, textNudge), scene, indicator::red);
        drawMode ("Stomp", bottom.translated (0.0f, -textNudge), ! scene, indicator::blue);

        // 가운데 구분선: 지금 모드 색을 흐리게, 양옆은 테두리에서 띄운다
        g.setColour ((scene ? indicator::red : indicator::blue).withAlpha (dividerAlpha));
        g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX() + dividerInset, area.getRight() - dividerInset);
    }
}
