#include "StatusBar.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace rig
{
    namespace
    {
        constexpr float meterFloorDb = -60.0f;
        constexpr float decayDbPerTick = 1.5f;   // 30Hz 기준 약 45dB/s
        constexpr int gaugeWidth = 240;
    }

    Gauge::Gauge (juce::String c, juce::Colour col) : caption (std::move (c)), colour (col) {}

    void Gauge::setValue (float f, const juce::String& text)
    {
        f = juce::jlimit (0.0f, 1.0f, f);

        if (! juce::approximatelyEqual (f, fraction) || text != valueText)
        {
            fraction = f;
            valueText = text;
            repaint();
        }
    }

    void Gauge::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds();

        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (caption, area.removeFromLeft (34), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (valueText, area.removeFromRight (64), juce::Justification::centredRight);

        auto bar = area.reduced (2, 6).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (bar, 3.0f);

        // 끝부분(위험 구간)은 빨간색
        const auto c = fraction > 0.9f ? juce::Colours::red : colour;
        g.setColour (c);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * fraction), 3.0f);
    }

    //==============================================================================
    StatusBar::StatusBar()
    {
        addAndMakeVisible (input);
        addAndMakeVisible (cpu);
        addAndMakeVisible (output);
    }

    float StatusBar::toMeterFraction (float dB)
    {
        return juce::jmap (juce::jlimit (meterFloorDb, 0.0f, dB), meterFloorDb, 0.0f, 0.0f, 1.0f);
    }

    void StatusBar::update (float inputPeak, double cpuUsage, float outputPeak)
    {
        auto follow = [] (float& shown, float peak)
        {
            const auto db = juce::Decibels::gainToDecibels (peak, -100.0f);
            shown = db > shown ? db : juce::jmax (db, shown - decayDbPerTick);
        };

        follow (inputDb, inputPeak);
        follow (outputDb, outputPeak);

        auto dbText = [] (float db) { return db <= meterFloorDb ? juce::String ("-inf dB") : juce::String (db, 1) + " dB"; };

        input.setValue (toMeterFraction (inputDb), dbText (inputDb));
        output.setValue (toMeterFraction (outputDb), dbText (outputDb));
        cpu.setValue ((float) cpuUsage, juce::String (cpuUsage * 100.0, 1) + " %");
    }

    void StatusBar::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.3f));
    }

    void StatusBar::resized()
    {
        auto area = getLocalBounds().reduced (8, 0);
        const int w = juce::jmin (gaugeWidth, area.getWidth() / 3 - 8);

        input.setBounds (area.removeFromLeft (w));                                  // 왼쪽 붙임
        output.setBounds (area.removeFromRight (w));                                // 오른쪽 붙임
        cpu.setBounds (getLocalBounds().withSizeKeepingCentre (w, getHeight()));    // 정중앙
    }
}
