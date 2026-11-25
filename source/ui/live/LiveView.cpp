#include "LiveView.h"

namespace rig
{
    LiveView::LiveView (RigModel& m, AudioEngine& e, juce::AudioDeviceManager& dm)
        : model (m),
          patchBar (m),
          scenePads (m),
          statusBar (m, e, dm)
    {
        statusBar.onLiveOff = [this] { if (onLiveOff) onLiveOff(); };
        patchBar.onPatchStep = [this] (int delta) { if (onPatchStep) onPatchStep (delta); };

        addAndMakeVisible (patchBar);
        addAndMakeVisible (scenePads);
        addAndMakeVisible (statusBar);
    }

    void LiveView::setBarHeight (int height)
    {
        barHeight = height;
        resized();
    }

    void LiveView::enter()
    {
        statusBar.setTunerOn (false);
        model.setLiveRouting (model.getRouting());
    }

    void LiveView::leave()
    {
        statusBar.setTunerOn (false);
        model.setLiveRouting (std::nullopt);
    }

    void LiveView::resized()
    {
        auto area = getLocalBounds();
        patchBar.setBounds (area.removeFromTop (barHeight));
        statusBar.setBounds (area.removeFromBottom (barHeight));
        scenePads.setBounds (area);
    }
}
