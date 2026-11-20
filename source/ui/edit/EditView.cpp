#include "EditView.h"

namespace rig
{
    EditView::EditView (RigModel& m, juce::AudioDeviceManager& dm)
        : chainView (m, dm),
          paramBar (m, dm),
          blockEditor (m),
          sceneBar (m)
    {
        sceneBar.onLiveClicked = [this] { if (onLiveClicked) onLiveClicked(); };

        addAndMakeVisible (chainView);
        addAndMakeVisible (paramBar);
        addAndMakeVisible (blockEditor);
        addAndMakeVisible (sceneBar);
    }

    void EditView::resized()
    {
        auto area = getLocalBounds();

        // 패치 맵 : 파라미터 바 : 블록 선택기 : 씬 바 = 3 : 1 : 3 : 1
        const int unit = area.getHeight() / 8;
        chainView.setBounds (area.removeFromTop (unit * 3));
        paramBar.setBounds (area.removeFromTop (unit));
        sceneBar.setBounds (area.removeFromBottom (unit));
        blockEditor.setBounds (area);
    }

    // 자식에게는 visibilityChanged 가 오지 않으므로 (setVisible 은 EditView 에 불린다) 여기서 넘겨준다.
    // ParamBar, SceneBar 는 갱신이 가벼워 그대로 둔다
    void EditView::visibilityChanged()
    {
        const bool hidden = ! isVisible();
        chainView.setUpdatesPaused (hidden);
        blockEditor.setUpdatesPaused (hidden);
    }
}
