#include "TunerDialog.h"

#include "tuner/TunerSettings.h"

#include <algorithm>
#include <cmath>

namespace rig
{
    namespace
    {
        // 동작 상수 (중앙값 개수는 TunerDialog::medianCount, 정확 범위는 TunerGauge::inTuneCents)
        constexpr int    refreshHz       = 20;
        constexpr int    silentAfter     = 8;       // 연속 실패 프레임 (약 0.4 초)
        constexpr double jumpCents       = 100.0;   // 이만큼 뛰면 새 음으로 보고 다시 모은다
        constexpr int    repeatFirstMs   = 400;     // ± 길게 누르기
        constexpr int    repeatEveryMs   = 60;

        // 배치 (내용물 420 x 318 안의 좌표, Figma)
        constexpr int contentWidth = 420, contentHeight = 318, tunerTitleBarHeight = 22;   // DocumentWindow::titleBarHeight 와 이름이 겹치지 않게
        const juce::Rectangle<int> gaugeBounds  { 8, 8, 404, 254 };
        const juce::Rectangle<int> barBounds    { 0, 270, 420, 48 };
        // 하단 바 안 (barBounds 기준)
        const juce::Rectangle<int> downBounds   { 12, 12, 24, 24 };    // 왼쪽 여백 12 에 붙인다
        const juce::Rectangle<int> pitchBounds  { 40, 12, 84, 24 };
        const juce::Rectangle<int> upBounds     { 128, 12, 24, 24 };
        const juce::Rectangle<int> ledBounds    { 288, 18, 8, 8 };
        const juce::Rectangle<int> sourceBounds { 300, 0, 108, 48 };

        const juce::Colour ledOnColour = juce::Colour (0xff32cd32);
        const juce::Colour ledOffColour = juce::Colours::grey;

        // DialogWindow::LaunchOptions::create 가 만드는 창과 같지만, 제목 표시줄 끌기도 왼쪽 클릭만 (SettingsDialog 와 같은 방식)
        struct TunerWindow final : public LeftClickOnly<juce::DialogWindow>
        {
            TunerWindow (std::unique_ptr<TunerDialog> content, juce::Component* parent)
                : LeftClickOnly ("Tuner",
                                 parent != nullptr ? parent->getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId)
                                                   : juce::Colours::darkgrey,
                                 false,   // ESC 로 닫히지 않게
                                 true,
                                 parent != nullptr ? juce::Component::getApproximateScaleFactorForComponent (parent) : 1.0f)
            {
                setUsingNativeTitleBar (false);
                setTitleBarHeight (tunerTitleBarHeight);
                setContentOwned (content.release(), true);
                setResizable (false, false);
                centreAroundComponent (parent, getWidth(), getHeight());
            }

            void closeButtonPressed() override
            {
                if (auto* dialog = dynamic_cast<TunerDialog*> (getContentComponent()))
                    dialog->close();
            }
        };
    }

    void TunerDialog::show (juce::AudioDeviceManager& dm, const AudioEngine::Routing& routing, bool passInput,
                            juce::Component* parent, std::function<void()> onClosed)
    {
        auto content = std::make_unique<TunerDialog> (dm, routing, passInput);
        content->onClosed = std::move (onClosed);

        auto* window = new TunerWindow (std::move (content), parent);

        // X(닫기) 버튼만 남긴다. 이 호출은 창(peer)을 다시 만들기 때문에 반드시 모달로 만들기 "전에" 해야 한다 (SettingsDialog 참고)
        window->setTitleBarButtonsRequired (juce::DocumentWindow::closeButton, false);

        // 모달로 띄운다: 닫히기 전에는 메인 창을 조작할 수 없음 (닫히면 창은 스스로 삭제)
        window->enterModalState (true, nullptr, true);
    }

    TunerDialog::TunerDialog (juce::AudioDeviceManager& dm, const AudioEngine::Routing& routing, bool passInput)
        : deviceManager (dm),
          pitchTenths (TunerSettings::load()),
          loadedTenths (pitchTenths)
    {
        addAndMakeVisible (gauge);

        // 기준음 값: 클릭하면 Hz 숫자를 직접 입력 (Enter 또는 바깥 클릭 = 적용, Esc = 취소). 범위 밖은 끝 값으로
        pitchLabel.setJustificationType (juce::Justification::centred);
        pitchLabel.setColour (juce::Label::backgroundColourId, juce::Colours::black.withAlpha (0.25f));
        pitchLabel.setColour (juce::Label::outlineColourId, juce::Colours::grey);
        pitchLabel.setEditable (true, false, false);
        pitchLabel.onEditorShow = [this]
        {
            if (auto* ed = pitchLabel.getCurrentTextEditor())
            {
                ed->setInputRestrictions (5, "0123456789.");   // 예: 442.5
                ed->setJustification (juce::Justification::centred);
                ed->setText (juce::String (TunerSettings::toHz (pitchTenths), 1), false);
                ed->selectAll();
            }
        };
        pitchLabel.onTextChange = [this] { pitchTyped(); };
        addAndMakeVisible (pitchLabel);

        for (auto* button : { &downButton, &upButton })
        {
            button->setTriggeredOnMouseDown (true);
            button->setRepeatSpeed (repeatFirstMs, repeatEveryMs);
            button->setWantsKeyboardFocus (false);
            addAndMakeVisible (button);
        }

        downButton.onClick = [this] { stepPitch (-1); };
        upButton.onClick   = [this] { stepPitch (1); };

        sourceLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (sourceLabel);

        updatePitchControls();
        setSize (contentWidth, contentHeight);

        input.setInputChannel (routing.input);
        input.setOutput (routing.output, routing.stereoOutput);
        input.setPassThrough (passInput);
        deviceManager.addAudioCallback (&input);
        startTimerHz (refreshHz);
    }

    TunerDialog::~TunerDialog()
    {
        stopTimer();
        deviceManager.removeAudioCallback (&input);   // close() 에서 이미 했어도 안전하다
    }

    void TunerDialog::close()
    {
        if (closed)
            return;

        closed = true;
        auto callback = onClosed;

        stopTimer();

        // 입력 통과 소리를 페이드로 줄인 뒤에 콜백을 뗀다 (바로 떼면 딸깍 소리)
        // shortcut: 메시지 스레드를 페이드 시간만큼(약 30 ms) 멈춘다. 닫기가 느껴질 만큼 길어지면 타이머로 나눌 것
        if (input.isPassingThrough())
        {
            input.setPassThrough (false);
            juce::Thread::sleep (juce::roundToInt (TunerInput::fadeSeconds * 1000.0) + 10);
        }

        deviceManager.removeAudioCallback (&input);

        if (pitchTenths != loadedTenths)
            TunerSettings::save (pitchTenths);

        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState (0);   // 창은 이후 스스로 삭제됨

        if (callback)
            callback();
    }

    void TunerDialog::stepPitch (int direction)
    {
        pitchTenths = TunerSettings::clampTenths (pitchTenths + direction * TunerSettings::stepTenths);
        updatePitchControls();
    }

    void TunerDialog::pitchTyped()
    {
        const auto typed = pitchLabel.getText().retainCharacters ("0123456789.");
        const double hz = typed.getDoubleValue();

        if (typed.isNotEmpty() && hz > 0.0)
            pitchTenths = TunerSettings::clampTenths ((int) std::lround (hz * 10.0));

        updatePitchControls();   // 잘못된 값이면 원래 값으로 되돌려 보인다
    }

    void TunerDialog::updatePitchControls()
    {
        pitchLabel.setText (juce::String (TunerSettings::toHz (pitchTenths), 1) + " Hz", juce::dontSendNotification);
        downButton.setEnabled (pitchTenths > TunerSettings::minTenths);
        upButton.setEnabled (pitchTenths < TunerSettings::maxTenths);
    }

    void TunerDialog::timerCallback()
    {
        // 1. 샘플레이트가 바뀔 때만 다시 준비 (이때만 할당)
        const double sr = input.getSampleRate();

        if (! juce::approximatelyEqual (sr, preparedRate))
        {
            detector.prepare (sr);
            samples.assign ((size_t) detector.samplesNeeded(), 0.0f);
            preparedRate = sr;
            recentCount = 0;
        }

        // 2-3. 최근 입력을 가져와 분석
        PitchResult result;

        if (input.copyLatest (samples.data(), (int) samples.size()))
            result = detector.detect (samples.data(), (int) samples.size());

        if (result.valid)
        {
            // 4. 반음 이상 뛰면 새 음으로 보고 다시 모은다
            failedFrames = 0;

            if (recentCount > 0 && std::abs (1200.0 * std::log2 (result.hz / state.hz)) >= jumpCents)
                recentCount = 0;

            if (recentCount < medianCount)
            {
                recent[(size_t) recentCount++] = result.hz;
            }
            else
            {
                std::rotate (recent.begin(), recent.begin() + 1, recent.end());
                recent.back() = result.hz;
            }

            auto sorted = recent;
            std::sort (sorted.begin(), sorted.begin() + recentCount);
            const double hz = sorted[(size_t) recentCount / 2];
            const auto note = PitchDetector::toNote (hz, TunerSettings::toHz (pitchTenths));

            state = { true, hz, note.name, note.cents };
        }
        else if (++failedFrames >= silentAfter)
        {
            // 5. 8 프레임 연속이면 신호 없음. 그 전에는 마지막 값을 그대로 둔다
            state = {};
            recentCount = 0;
        }

        // 6. 화면 갱신
        gauge.setState (state);

        const int channel = input.getInputChannel();
        sourceLabel.setText (channel > 0 ? "Source: IN " + juce::String (channel) : juce::String ("Source: -"),
                             juce::dontSendNotification);
        repaint (barBounds.getX() + ledBounds.getX(), barBounds.getY() + ledBounds.getY(), ledBounds.getWidth(), ledBounds.getHeight());
    }

    void TunerDialog::paint (juce::Graphics& g)
    {
        const auto background = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
        g.fillAll (background);

        g.setColour (background.darker (0.3f));   // 하단 바 (상단 바와 같은 색)
        g.fillRect (barBounds);

        g.setColour (state.hasSignal ? ledOnColour : ledOffColour);
        g.fillEllipse (ledBounds.translated (barBounds.getX(), barBounds.getY()).toFloat());
    }

    void TunerDialog::resized()
    {
        gauge.setBounds (gaugeBounds);

        const auto origin = barBounds.getPosition();
        downButton.setBounds (downBounds + origin);
        pitchLabel.setBounds (pitchBounds + origin);
        upButton.setBounds (upBounds + origin);
        sourceLabel.setBounds (sourceBounds + origin);
    }
}
