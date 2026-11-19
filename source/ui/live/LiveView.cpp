#include "LiveView.h"

namespace rig
{
    LiveView::LiveView (RigModel& m)
        : scenePads (m),
          patchBar (m)
    {
        patchBar.onLiveOff = [this] { if (onLiveOff) onLiveOff(); };
        patchBar.onPatchStep = [this] (int delta) { if (onPatchStep) onPatchStep (delta); };

        addAndMakeVisible (scenePads);
        addAndMakeVisible (patchBar);
    }

    void LiveView::resized()
    {
        auto area = getLocalBounds();

        // 하단 줄 높이는 편집 화면과 같은 식 (getHeight() / 8): 모드를 바꿔도 Live 버튼이 같은 자리
        patchBar.setBounds (area.removeFromBottom (getHeight() / 8));
        scenePads.setBounds (area);
    }
}
