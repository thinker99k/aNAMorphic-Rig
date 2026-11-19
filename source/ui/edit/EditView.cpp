#include "EditView.h"

namespace rig
{
    EditView::EditView (RigModel& m, juce::AudioDeviceManager& dm)
        : chainView (m, dm),
          paramBar (m),
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
}
