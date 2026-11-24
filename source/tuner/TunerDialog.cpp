#include "TunerDialog.h"

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

        // 배치 (내용물 420 x 270 안의 좌표). 게이지는 도면 크기 그대로 (값 상자 자리 pitchArea 가 맞도록)
        constexpr int contentWidth = 420, contentHeight = 270, tunerTitleBarHeight = 22;   // DocumentWindow::titleBarHeight 와 이름이 겹치지 않게
        const juce::Rectangle<int> gaugeBounds  { 8, 8, TunerGauge::panelWidth, TunerGauge::panelHeight };

        constexpr int pixelsPerHz = 2;   // 값 상자를 위아래로 끌 때 1 Hz 당 픽셀 (위 = 올림)

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

    void TunerDialog::show (AudioEngine& engine, RigModel& model, juce::Component* parent, std::function<void()> onClosed)
    {
        auto content = std::make_unique<TunerDialog> (engine, model);
        content->onClosed = std::move (onClosed);

        auto* window = new TunerWindow (std::move (content), parent);

        // X(닫기) 버튼만 남긴다. 이 호출은 창(peer)을 다시 만들기 때문에 반드시 모달로 만들기 "전에" 해야 한다 (SettingsDialog 참고)
        window->setTitleBarButtonsRequired (juce::DocumentWindow::closeButton, false);

        // 모달로 띄운다: 닫히기 전에는 메인 창을 조작할 수 없음 (닫히면 창은 스스로 삭제)
        window->enterModalState (true, nullptr, true);
    }

    TunerDialog::TunerDialog (AudioEngine& e, RigModel& m)
        : engine (e),
          model (m)
    {
        addAndMakeVisible (gauge);

        // 기준음 값: 클릭하면 Hz 숫자를 직접 입력 (Enter 또는 바깥 클릭 = 적용, Esc = 취소). 범위 밖은 끝 값으로
        //          위아래로 끌면 값이 바뀐다 (끌었으면 Label 이 입력 칸을 열지 않는다). 끌기는 mouseDown / mouseDrag 에서
        //          게이지의 음 이름 왼쪽에 글자만 (Logic 튜너처럼 상자 테두리 없음)
        pitchLabel.setJustificationType (juce::Justification::centred);
        pitchLabel.setFont (juce::FontOptions (TunerGauge::valueFontSize));
        pitchLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        pitchLabel.setEditable (true, false, false);
        pitchLabel.onEditorShow = [this]
        {
            if (auto* ed = pitchLabel.getCurrentTextEditor())
            {
                ed->setPopupMenuEnabled (false);   // 입력칸의 오른쪽 클릭 메뉴는 쓰지 않는다 (마우스 규칙)
                ed->setInputRestrictions (3, "0123456789");   // 예: 442
                ed->setJustification (juce::Justification::centred);
                ed->setText (juce::String (model.getTunerPitchHz()), false);
                ed->selectAll();
            }
        };
        pitchLabel.onTextChange = [this] { pitchTyped(); };
        pitchLabel.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        pitchLabel.addMouseListener (this, false);
        addAndMakeVisible (pitchLabel);

        updatePitchLabel();
        setSize (contentWidth, contentHeight);

        engine.setTunerActive (true);
        startTimerHz (refreshHz);
    }

    TunerDialog::~TunerDialog()
    {
        stopTimer();
        engine.setTunerActive (false);   // close() 에서 이미 했어도 안전하다
    }

    void TunerDialog::close()
    {
        if (closed)
            return;

        closed = true;
        auto callback = onClosed;

        stopTimer();
        engine.setTunerActive (false);   // 소리를 되살리는 페이드는 엔진이 한다

        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState (0);   // 창은 이후 스스로 삭제됨

        if (callback)
            callback();
    }

    void TunerDialog::pitchTyped()
    {
        const auto typed = pitchLabel.getText().retainCharacters ("0123456789");

        if (typed.isNotEmpty())
        {
            setPitchHz (typed.getIntValue());
            model.saveAppSettings();   // 입력을 마치면 바로 저장 (끌기 끝과 같이)
        }
        else
            updatePitchLabel();   // 빈 값이면 원래 값으로 되돌려 보인다
    }

    void TunerDialog::setPitchHz (int hz)
    {
        model.setTunerPitchHz (hz);   // 범위 밖은 RigModel 이 끝 값으로
        updatePitchLabel();
    }

    void TunerDialog::updatePitchLabel()
    {
        pitchLabel.setText (juce::String (model.getTunerPitchHz()) + " Hz", juce::dontSendNotification);
    }

    void TunerDialog::timerCallback()
    {
        // 1. 샘플레이트가 바뀔 때만 다시 준비 (이때만 할당)
        const double sr = engine.getSampleRate();

        if (! juce::approximatelyEqual (sr, preparedRate))
        {
            detector.prepare (sr);
            samples.assign ((size_t) detector.samplesNeeded(), 0.0f);
            preparedRate = sr;
            recentCount = 0;
        }

        // 2-3. 최근 입력을 가져와 분석
        PitchResult result;

        if (engine.copyLatestInput (samples.data(), (int) samples.size()))
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
            const auto note = PitchDetector::toNote (hz, model.getTunerPitchHz());

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
    }

    // 값 상자 끌기 (pitchLabel 의 마우스 이벤트를 받는다). 왼쪽 버튼만, 입력 중에는 무시
    void TunerDialog::mouseDown (const juce::MouseEvent& e)
    {
        if (e.eventComponent == &pitchLabel && isLeftClick (e))
            dragStartHz = model.getTunerPitchHz();
    }

    void TunerDialog::mouseDrag (const juce::MouseEvent& e)
    {
        if (e.eventComponent != &pitchLabel || ! isLeftClick (e) || pitchLabel.isBeingEdited())
            return;

        setPitchHz (dragStartHz - e.getDistanceFromDragStartY() / pixelsPerHz);
    }

    void TunerDialog::mouseUp (const juce::MouseEvent& e)
    {
        if (e.eventComponent == &pitchLabel && isLeftClick (e) && e.mouseWasDraggedSinceMouseDown() && ! pitchLabel.isBeingEdited())
            model.saveAppSettings();
    }

    void TunerDialog::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void TunerDialog::resized()
    {
        gauge.setBounds (gaugeBounds);

        pitchLabel.setBounds (TunerGauge::pitchArea() + gaugeBounds.getPosition());   // 게이지 위에 겹친다
    }
}
