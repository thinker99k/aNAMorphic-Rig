#include "WaveformWindow.h"

namespace rig
{
    namespace
    {
        constexpr double windowSeconds = 0.05;   // 표시 길이 약 50 ms
        constexpr float axisRange = 2.0f;        // 세로 축 ±2.0 (약 +6 dBFS), 두 패널 공통
        constexpr float clipLevel = 1.0f;

        const juce::Colour waveColour = juce::Colours::limegreen;
        const juce::Colour clipColour = juce::Colours::red;
        const juce::Colour clipLineColour = juce::Colours::red.withAlpha (0.6f);
        const juce::Colour gridColour = juce::Colours::darkgrey.darker (0.3f);
        const juce::Colour panelColour = juce::Colour (0xff141414);
    }

    WaveformView::WaveformView (AudioEngine& e) : engine (e)
    {
        setOpaque (true);
    }

    WaveformView::~WaveformView()
    {
        engine.setScopeEnabled (false);
    }

    void WaveformView::setRunning (bool shouldRun)
    {
        engine.setScopeEnabled (shouldRun);

        if (shouldRun)
            startTimerHz (60);
        else
            stopTimer();
    }

    void WaveformView::timerCallback()
    {
        // 샘플레이트가 바뀌어도 항상 약 50 ms. 2N 이 링 크기의 절반을 넘지 않게 제한 (192 kHz 에서는 약 43 ms)
        const int n = juce::jlimit (1, AudioEngine::ScopeRing::size / 4,
                                    juce::roundToInt (engine.getSampleRate() * windowSeconds));

        capture (true, n, inputSamples);
        capture (false, n, outputSamples);
        repaint();
    }

    void WaveformView::capture (bool input, int n, std::vector<float>& dest)
    {
        fetchBuffer.resize ((size_t) n * 2);

        const bool ok = input ? engine.copyLatestInput (fetchBuffer.data(), n * 2)
                              : engine.copyLatestOutput (fetchBuffer.data(), n * 2);
        if (! ok)
            return;   // 이번 프레임은 건너뛰고 이전 표시를 유지

        // 앞쪽 N 개 안에서 상승 영점 통과를 찾는다. 없으면(무음, 잡음) 가장 최근 N 개
        int start = n;

        for (int i = 1; i < n; ++i)
        {
            if (fetchBuffer[(size_t) i - 1] < 0.0f && fetchBuffer[(size_t) i] >= 0.0f)
            {
                start = i;
                break;
            }
        }

        dest.assign (fetchBuffer.begin() + start, fetchBuffer.begin() + start + n);
    }

    void WaveformView::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colours::black);

        auto area = getLocalBounds();
        auto left = area.removeFromLeft (area.getWidth() / 2);

        drawPanel (g, left.reduced (4), "INPUT", inputSamples);
        drawPanel (g, area.reduced (4), "OUTPUT", outputSamples);

        // 두 패널 사이 구분선
        g.setColour (juce::Colours::grey);
        g.drawVerticalLine (left.getRight(), 0.0f, (float) getHeight());
    }

    void WaveformView::drawPanel (juce::Graphics& g, juce::Rectangle<int> bounds,
                                  const juce::String& title, const std::vector<float>& samples)
    {
        g.setColour (panelColour);
        g.fillRect (bounds);

        const auto left = (float) bounds.getX();
        const auto right = (float) bounds.getRight();
        const auto centreY = bounds.toFloat().getCentreY();
        const auto halfHeight = (float) bounds.getHeight() * 0.5f;

        // 값 -> y (±2.0 이 패널 위·아래 끝). 넘는 값은 가장자리에 붙인다
        auto toY = [&] (float v) { return centreY - juce::jlimit (-axisRange, axisRange, v) / axisRange * halfHeight; };

        // 기준선: 0, ±0.5 (회색), ±1.0 (빨강 점선)
        g.setColour (gridColour);
        for (auto v : { 0.0f, 0.5f, -0.5f })
            g.drawHorizontalLine (juce::roundToInt (toY (v)), left, right);

        const float dashes[] = { 4.0f, 4.0f };
        g.setColour (clipLineColour);
        for (auto v : { clipLevel, -clipLevel })
            g.drawDashedLine ({ left, toY (v), right, toY (v) }, dashes, 2, 1.0f);

        g.setFont (juce::FontOptions (11.0f));
        g.setColour (juce::Colours::grey);
        g.drawText ("+1.0", juce::Rectangle<float> (left + 4.0f, toY (clipLevel) - 14.0f, 40.0f, 13.0f), juce::Justification::centredLeft);
        g.drawText ("0", juce::Rectangle<float> (left + 4.0f, centreY - 14.0f, 40.0f, 13.0f), juce::Justification::centredLeft);
        g.drawText ("-1.0", juce::Rectangle<float> (left + 4.0f, toY (-clipLevel) + 1.0f, 40.0f, 13.0f), juce::Justification::centredLeft);

        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (juce::Colours::lightgrey);
        g.drawText (title, bounds.reduced (6, 4).removeFromTop (16), juce::Justification::topLeft);

        if (samples.empty())
            return;   // 첫 몇 프레임: 중앙선만

        // 가로 픽셀 열마다 그 열의 샘플 최솟값~최댓값을 세로 선으로 (앞 열 마지막 샘플을 포함해 끊기지 않게)
        const int width = bounds.getWidth();
        const auto count = (int) samples.size();

        for (int col = 0; col < width; ++col)
        {
            const int from = juce::jmax (0, (int) ((juce::int64) col * count / width) - 1);
            const int to = juce::jmax (from + 1, (int) ((juce::int64) (col + 1) * count / width));

            auto lo = samples[(size_t) from], hi = lo;
            for (int i = from + 1; i < juce::jmin (to, count); ++i)
            {
                lo = juce::jmin (lo, samples[(size_t) i]);
                hi = juce::jmax (hi, samples[(size_t) i]);
            }

            const auto x = (float) (bounds.getX() + col);

            // ±1.0 안쪽은 초록 (최소 1 px 높이)
            const auto greenTop = toY (juce::jmin (hi, clipLevel));
            const auto greenBottom = toY (juce::jmax (lo, -clipLevel));
            g.setColour (waveColour);
            g.fillRect (x, juce::jmin (greenTop, greenBottom), 1.0f, juce::jmax (1.0f, std::abs (greenBottom - greenTop)));

            // ±1.0 을 넘는 부분은 빨강으로 덧그린다 (자르지 않고 넘는 값 그대로)
            g.setColour (clipColour);
            if (hi > clipLevel)
                g.fillRect (x, toY (hi), 1.0f, juce::jmax (1.0f, toY (clipLevel) - toY (hi)));
            if (lo < -clipLevel)
                g.fillRect (x, toY (-clipLevel), 1.0f, juce::jmax (1.0f, toY (lo) - toY (-clipLevel)));
        }
    }

    //==============================================================================
    WaveformWindow::WaveformWindow (AudioEngine& engine)
        : DocumentWindow ("Waveform",
                          juce::Desktop::getInstance().getDefaultLookAndFeel()
                              .findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::closeButton)
    {
        view = new WaveformView (engine);

        setUsingNativeTitleBar (true);
        setContentOwned (view, false);
        setResizable (true, false);
        setResizeLimits (480, 160, 4096, 4096);
        setSize (960, 320);
    }

    WaveformWindow::~WaveformWindow()
    {
        clearContentComponent();   // view 소멸자가 엔진 기록을 끈다
    }

    void WaveformWindow::closeButtonPressed()
    {
        // 이벤트 처리 중에 자신을 지우지 않는다. 다시 열 때 같은 창을 보여준다
        setVisible (false);
    }

    void WaveformWindow::visibilityChanged()
    {
        DocumentWindow::visibilityChanged();

        // 창이 숨겨질 때 안쪽 컴포넌트에는 visibilityChanged 가 오지 않으므로 여기서 알려준다
        if (view != nullptr)
            view->setRunning (isVisible());
    }
}
