#include "SpectrumWindow.h"

// [SPECTRUM] 테스트용 보조 기능 (제거 방법은 SpectrumWindow.h 맨 위)

#include "engine/param/nam/NamParams.h"   // EQ 대역 경계 (H1, H2) 표시용

#include <cmath>

namespace rig
{
    namespace
    {
        // 그래프 영역 여백 (왼쪽은 dB 글자, 아래는 Hz 글자 자리)
        constexpr float marginLeft = 44.0f, marginRight = 12.0f, marginTop = 10.0f, marginBottom = 22.0f;

        const juce::Colour backgroundColour (0xff141414);
        const juce::Colour gridColour = juce::Colours::darkgrey.darker (0.3f);
        const juce::Colour curveColour = juce::Colours::limegreen;
        const juce::Colour bandColour = juce::Colours::orange.withAlpha (0.6f);   // EQ 대역 경계선과 이름
    }

    SpectrumView::SpectrumView (AudioEngine& e) : engine (e)
    {
        setOpaque (true);

        // 주기형 Hann 창: 합이 정확히 fftSize / 2 라서 아래 진폭 보정식이 단순해진다
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);

        shown.fill (floorDb);
    }

    SpectrumView::~SpectrumView()
    {
        engine.setScopeEnabled (false);
    }

    void SpectrumView::setRunning (bool shouldRun)
    {
        engine.setScopeEnabled (shouldRun);

        if (shouldRun)
            startTimerHz (refreshHz);
        else
            stopTimer();
    }

    void SpectrumView::visibilityChanged()
    {
        setRunning (isShowing());
    }

    void SpectrumView::timerCallback()
    {
        if (! engine.copyLatestOutput (input.data(), fftSize))
            return;   // 데이터가 부족하거나 덮어쓰였으면 이번 프레임은 건너뛴다

        for (int i = 0; i < fftSize; ++i)
            work[(size_t) i] = input[(size_t) i] * window[(size_t) i];

        std::fill (work.begin() + fftSize, work.end(), 0.0f);

        // JUCE 9.0.4 juce_FFT.h: 2N 배열을 받아, true 면 앞쪽에 최소 N/2+1 개의 크기(0번 ~ 나이퀴스트 빈)를 돌려준다
        fft.performFrequencyOnlyForwardTransform (work.data(), true);

        for (int k = 0; k < numBins; ++k)
        {
            // 진폭 A 사인파는 Hann 창(가운데 이득 0.5)을 거쳐 한 빈에 A * N / 4 로 나타난다 -> 4 / N 을 곱하면 A
            const auto amp = work[(size_t) k] * 4.0f / (float) fftSize;
            const auto db = juce::jlimit (floorDb, topDb, 20.0f * std::log10 (juce::jmax (amp, 1.0e-6f)));

            auto& s = shown[(size_t) k];
            s = db >= s ? db : juce::jmax (db, s - fallDbPerFrame);
        }

        repaint();
    }

    float SpectrumView::dbAtBin (float bin) const
    {
        bin = juce::jlimit (0.0f, (float) (numBins - 1), bin);
        const auto i = (int) bin;
        const auto next = juce::jmin (i + 1, numBins - 1);
        const auto frac = bin - (float) i;
        return shown[(size_t) i] + (shown[(size_t) next] - shown[(size_t) i]) * frac;
    }

    void SpectrumView::paint (juce::Graphics& g)
    {
        g.fillAll (backgroundColour);

        const auto plot = getLocalBounds().toFloat()
                              .withTrimmedLeft (marginLeft).withTrimmedRight (marginRight)
                              .withTrimmedTop (marginTop).withTrimmedBottom (marginBottom);

        if (plot.getWidth() <= 1.0f || plot.getHeight() <= 1.0f)
            return;

        const auto hzRatio = std::log (maxHz / minHz);
        auto xForHz = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / minHz) / hzRatio; };
        auto yForDb = [&] (float db) { return plot.getY() + plot.getHeight() * (topDb - db) / (topDb - floorDb); };

        g.setFont (juce::FontOptions (11.0f));

        // 가로 눈금: 0 ~ floorDb, gridStepDb 간격
        for (float db = topDb; db >= floorDb; db -= gridStepDb)
        {
            const auto y = yForDb (db);
            g.setColour (gridColour);
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
            g.setColour (juce::Colours::grey);
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (0.0f, y - 7.0f, marginLeft - 6.0f, 14.0f),
                        juce::Justification::centredRight);
        }

        // 세로 눈금: 20 Hz ~ 20 kHz, 글자는 20 / 100 / 1k / 10k / 20k 만
        struct Tick { float hz; const char* label; };

        for (auto [hz, label] : { Tick { 20.0f, "20" }, Tick { 50.0f, nullptr }, Tick { 100.0f, "100" },
                                  Tick { 200.0f, nullptr }, Tick { 500.0f, nullptr }, Tick { 1000.0f, "1k" },
                                  Tick { 2000.0f, nullptr }, Tick { 5000.0f, nullptr }, Tick { 10000.0f, "10k" },
                                  Tick { 20000.0f, "20k" } })
        {
            const auto x = xForHz (hz);
            g.setColour (gridColour);
            g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());

            if (label != nullptr)
            {
                g.setColour (juce::Colours::grey);
                g.drawText (label, juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 3.0f, 40.0f, 14.0f),
                            juce::Justification::centred);
            }
        }

        g.setColour (juce::Colours::grey);
        g.drawText ("Hz", plot.reduced (4.0f).removeFromBottom (14.0f), juce::Justification::bottomRight);

        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("OUTPUT", plot.reduced (6.0f, 4.0f).removeFromTop (16.0f), juce::Justification::topLeft);

        // EQ 대역 범위: H1 / H2 점선과 각 구간 가운데(로그)에 이름. 노브 범위가 아니라 대역 경계다
        {
            const float h1 = (float) NamParams::h1Hz, h2 = (float) NamParams::h2Hz;
            const float dashes[] = { 4.0f, 4.0f };

            g.setColour (bandColour);
            for (auto hz : { h1, h2 })
                g.drawDashedLine ({ xForHz (hz), plot.getY(), xForHz (hz), plot.getBottom() }, dashes, 2, 1.0f);

            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            const float labelY = plot.getY() + 22.0f;   // OUTPUT 글자 아래

            struct Band { float lo, hi; const char* name; };
            for (auto [lo, hi, name] : { Band { minHz, h1, "BASS" }, Band { h1, h2, "MIDDLE" }, Band { h2, maxHz, "TREBLE" } })
            {
                const auto x = xForHz (std::sqrt (lo * hi));
                g.drawText (name, juce::Rectangle<float> (x - 40.0f, labelY, 80.0f, 14.0f), juce::Justification::centred);
            }

            // 경계선 오른쪽에 주파수 (상수에서 만든다: 250, 2.5k)
            g.setFont (juce::FontOptions (10.0f));
            for (auto hz : { h1, h2 })
            {
                const auto text = hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (hz));
                g.drawText (text, juce::Rectangle<float> (xForHz (hz) + 3.0f, labelY + 14.0f, 40.0f, 12.0f), juce::Justification::centredLeft);
            }
        }

        // 그래프: 픽셀 열마다 값 하나. 좁은 구간(저음)은 보간, 넓은 구간(고음)은 구간 안 최댓값
        const auto sampleRate = (float) engine.getSampleRate();
        const auto nyquist = sampleRate * 0.5f;
        const auto hzPerBin = sampleRate / (float) fftSize;
        const int columns = (int) plot.getWidth();

        juce::Path curve;
        bool started = false;   // Path::isEmpty() 는 시작점(moveTo)만 있으면 비었다고 보므로 따로 센다
        float lastX = plot.getX();

        for (int c = 0; c < columns; ++c)
        {
            const auto fLo = minHz * std::exp (hzRatio * (float) c / (float) columns);
            const auto fHi = minHz * std::exp (hzRatio * (float) (c + 1) / (float) columns);

            if (fLo >= nyquist)
                break;   // 샘플레이트가 낮으면 나이퀴스트 오른쪽은 비워 둔다

            const auto bLo = fLo / hzPerBin, bHi = juce::jmin (fHi, nyquist) / hzPerBin;
            float db;

            if (bHi - bLo < 1.0f)
            {
                db = dbAtBin ((bLo + bHi) * 0.5f);
            }
            else
            {
                db = floorDb;
                for (int k = (int) std::ceil (bLo); k <= juce::jmin ((int) bHi, numBins - 1); ++k)
                    db = juce::jmax (db, shown[(size_t) k]);
            }

            const auto x = plot.getX() + (float) c + 0.5f;
            const auto y = yForDb (db);

            if (started)
                curve.lineTo (x, y);
            else
                curve.startNewSubPath (x, y);

            started = true;
            lastX = x;
        }

        if (! started)
            return;

        auto fill = curve;
        fill.lineTo (lastX, plot.getBottom());
        fill.lineTo (plot.getX() + 0.5f, plot.getBottom());
        fill.closeSubPath();

        g.setColour (curveColour.withAlpha (0.25f));
        g.fillPath (fill);
        g.setColour (curveColour);
        g.strokePath (curve, juce::PathStrokeType (1.5f));
    }

    //==============================================================================
    SpectrumWindow::SpectrumWindow (AudioEngine& engine)
        : DocumentWindow ("Spectrum",
                          juce::Desktop::getInstance().getDefaultLookAndFeel()
                              .findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::closeButton)
    {
        view = new SpectrumView (engine);

        setUsingNativeTitleBar (true);
        setContentOwned (view, false);
        setResizable (true, false);
        setResizeLimits (480, 200, 4096, 4096);
        setSize (800, 360);
    }

    SpectrumWindow::~SpectrumWindow()
    {
        clearContentComponent();   // view 소멸자가 엔진 기록을 끈다
    }

    void SpectrumWindow::closeButtonPressed()
    {
        // 이벤트 처리 중에 자신을 지우지 않는다. 다시 열 때 같은 창을 보여준다
        setVisible (false);
    }

    void SpectrumWindow::visibilityChanged()
    {
        DocumentWindow::visibilityChanged();

        // 창을 숨기거나 보여도 안쪽 컴포넌트에는 visibilityChanged 가 오지 않으므로 여기서 알려준다
        if (view != nullptr)
            view->setRunning (isVisible());
    }
}
