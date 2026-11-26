#include "SceneBar.h"

#include <algorithm>
#include <cmath>

namespace rig
{
    namespace
    {
        // 실행 뒤 테두리: 자동차 할로겐 방향지시등처럼 부드럽게 두 번 깜빡인다 (한 번 = blinkMs, sin² 곡선)
        constexpr juce::uint32 blinkMs = 400, numBlinks = 2, flashMs = blinkMs * numBlinks;
        constexpr juce::uint32 pulseMs = 1200;   // 고른 src 밝기가 한 번 오르내리는 시간
        constexpr int animationHz = 60;         // src 밝기 오르내림·실행 뒤 깜빡임 다시 그리기
        constexpr int gap = bottomBarGap;       // 바깥 여백 (버튼 사이도 같은 값, 14칸 격자 bottomBarCell)
        constexpr int saveButtonSize = 24;      // 이름 입력칸 오른쪽 위 귀퉁이의 정사각형 체크 버튼
        constexpr float sceneCorner = 5.0f;     // 칸 버튼 둥근 모서리 반지름
        constexpr float nameFontSize = 13.0f;   // 칸 이름 (버튼 표시와 입력칸이 같은 크기와 위치)
        constexpr int nameInset = 6;
        constexpr float unboundAlpha = 0.4f;    // 안 묶인 Stomp 칸의 번호

        // 14칸 격자 칸 번호 (0부터)
        constexpr int resetCell = 0, actionCell = 2, firstCell = 3, copyCell = 11;

        // 한 칸을 위아래 두 버튼으로 (사이 gap)
        void stack (juce::Rectangle<int> cell, juce::Component& top, juce::Component& bottom)
        {
            top.setBounds (cell.removeFromTop ((cell.getHeight() - gap) / 2));
            cell.removeFromTop (gap);
            bottom.setBounds (cell);
        }
    }

    SceneBar::SceneBar (RigModel& m) : model (m), modeSwitch (m)
    {
        addAndMakeVisible (modeSwitch);
        setWantsKeyboardFocus (true);   // 모드 중 ESC 를 받는다
        shownPatch = model.getPatchNumber();
        shownStomp = stomp();

        liveButton.onClick = [this] { if (onLiveClicked) onLiveClicked(); };
        disableHoverHighlight (liveButton);   // Live 와 연관된 버튼
        addAndMakeVisible (liveButton);

        for (auto* b : { &copyButton, &swapButton, &renameButton, &sceneResetButton, &bindButton, &unbindButton, &resetAllButton })
        {
            // 눌러도 키보드 포커스를 가져가지 않는다 (ESC 는 ssbar 가 받는다)
            b->setWantsKeyboardFocus (false);
            b->setMouseClickGrabsKeyboardFocus (false);
            addChildComponent (b);
        }

        for (auto* b : { &copyButton, &swapButton, &bindButton, &unbindButton })
            indicator::setColours (*b, indicator::amberFill, indicator::amber);   // 모드 중 = 켜짐 표시 (주황)

        copyButton.onClick = [this] { toggleMode (Mode::copy); };
        swapButton.onClick = [this] { toggleMode (Mode::swap); };
        bindButton.onClick = [this] { toggleMode (Mode::bind); };
        unbindButton.onClick = [this] { toggleMode (Mode::unbind); };
        bindButton.setTooltip ("Bind the selected block: then pick a stomp");
        unbindButton.setTooltip ("Unbind a stomp: pick the stomp");

        renameButton.setTooltip ("Rename the current scene");
        renameButton.onClick = [this]
        {
            cancelMode();
            startRename (model.getCurrentScene());
        };

        // 씬 하나 = 중간 리셋이라 경고 없이 바로
        sceneResetButton.setTooltip ("Reset the current scene: every block On with default parameters, branches -3 dB, default name");
        sceneResetButton.onClick = [this]
        {
            cancelMode();
            cancelRename();
            model.resetScene (model.getCurrentScene());
        };

        // 고르는 모드 중이면 바깥 클릭처럼 모드를 끝낸다. 경고창에서 Reset 을 눌러야 초기화
        resetAllButton.onClick = [this]
        {
            cancelMode();
            cancelRename();

            juce::Component::SafePointer<SceneBar> safeThis (this);

            if (stomp())
                showResetWarning (*this, "Stomp Reset", "Unbind all 8 stomps?",
                                  [safeThis] { if (safeThis != nullptr) safeThis->model.resetStomps(); });
            else
                showResetWarning (*this, "Scene Reset",
                                  "Reset all 8 scenes?\n\nIn every scene, every block will be On with default parameters, "
                                  "parallel branches will be On at -3 dB, and the scene names go back to \"Scene 1\" ... \"Scene 8\".",
                                  [safeThis] { if (safeThis != nullptr) safeThis->model.resetAllScenes(); });
        };

        // 이름 편집 (평소에는 숨김)
        // 입력칸 자체는 투명: 둥근 바탕과 테두리는 paint() 가 씬 버튼 모양대로 그린다 (귀퉁이 체크 버튼과 모양이 맞도록)
        nameEditor.setJustificationType (juce::Justification::centredBottom);
        nameEditor.setFont (juce::FontOptions (nameFontSize));
        for (const auto id : { juce::Label::backgroundColourId, juce::Label::outlineColourId,
                               juce::Label::backgroundWhenEditingColourId, juce::Label::outlineWhenEditingColourId })
            nameEditor.setColour (id, juce::Colours::transparentBlack);
        nameEditor.setEditable (false, false, true);   // 포커스를 잃으면(바깥 클릭) 편집 취소
        nameEditor.onEditorShow = [this]
        {
            if (auto* ed = nameEditor.getCurrentTextEditor())
            {
                ed->setInputRestrictions (RigModel::maxSceneNameLength);
                // 버튼에 이름을 그리는 위치와 같게: 아래 가운데, 버튼 가장자리에서 nameInset 안쪽
                ed->setJustification (juce::Justification::centredBottom);
                ed->setIndents (0, 0);
                ed->setBorder ({ 0, nameInset, nameInset, nameInset });
            }
        };
        nameEditor.onEditorHide = [this]
        {
            renamingScene = -1;
            nameEditor.setVisible (false);
            saveNameButton.setVisible (false);
            repaint();
        };
        nameEditor.onTextChange = [this]
        {
            // Label 은 편집기를 닫은 뒤(onEditorHide 다음)에 이 알림을 보낸다
            if (nameTargetScene >= 0)
                model.renameScene (nameTargetScene, nameEditor.getText());
        };
        nameEditor.allowWhileEditing = [this] (const juce::Component* target) { return target == &saveNameButton; };
        addChildComponent (nameEditor);

        // 체크 버튼을 눌러도 입력칸의 포커스를 빼앗지 않아야 저장된다 (포커스를 잃으면 취소되므로)
        saveNameButton.setTooltip ("Save scene name (Enter to save, Escape to cancel)");
        saveNameButton.setWantsKeyboardFocus (false);
        saveNameButton.setMouseClickGrabsKeyboardFocus (false);
        saveNameButton.onClick = [this] { nameEditor.hideEditor (false); };
        addChildComponent (saveNameButton);

        model.addChangeListener (this);
        updateButtons();
    }

    SceneBar::~SceneBar()
    {
        juce::Desktop::getInstance().removeGlobalMouseListener (&outsideClickWatcher);
        model.removeChangeListener (this);
    }

    //==============================================================================
    void SceneBar::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        // 패치나 Scene / Stomp 가 바뀌면 진행 중인 모드와 이름 편집을 취소한다
        if (model.getPatchNumber() != shownPatch || stomp() != shownStomp)
        {
            shownPatch = model.getPatchNumber();
            shownStomp = stomp();
            cancelMode();
            cancelRename();
            flashStart.fill (0);
        }

        updateButtons();
        repaint();   // 이름과 현재 씬 / 켜진 Stomp 강조
    }

    void SceneBar::updateButtons()
    {
        const bool s = stomp();

        renameButton.setVisible (! s);
        sceneResetButton.setVisible (! s);
        bindButton.setVisible (s);
        unbindButton.setVisible (s);
        copyButton.setVisible (true);
        swapButton.setVisible (true);
        resetAllButton.setVisible (true);
        resetAllButton.setButtonText (s ? "Stomp\nReset..." : "Scene\nReset...");

        copyButton.setTooltip (s ? "Copy a stomp: pick the source stomp, then the destination"
                                 : "Copy a scene: pick the source scene, then the destination");
        swapButton.setTooltip (s ? "Swap two stomps: pick one stomp, then the other"
                                 : "Swap two scenes: pick one scene, then the other");
        resetAllButton.setTooltip (s ? "Unbind all stomps"
                                     : "Reset all scenes: every block On with default parameters, branches -3 dB, default names");

        bool anyBound = false;
        for (int i = 0; i < RigModel::numStomps; ++i)
            anyBound = anyBound || model.getStompSlot (i) != nullptr;

        // 고르는 중인 모드 버튼은 끄지 않는다 (누르면 모드가 끝나야 하므로)
        bindButton.setEnabled (model.getSelectedSlot() != nullptr || mode == Mode::bind);
        unbindButton.setEnabled (anyBound || mode == Mode::unbind);
    }

    int SceneBar::cellAt (juce::Point<int> position) const
    {
        for (int i = 0; i < (int) cellBounds.size(); ++i)
            if (cellBounds[(size_t) i].contains (position))
                return i;

        return -1;
    }

    void SceneBar::mouseDown (const juce::MouseEvent& e)
    {
        if (const int cell = cellAt (e.getPosition()); cell >= 0 && isLeftClick (e))
            cellClicked (cell);
    }

    void SceneBar::cellClicked (int cell)
    {
        switch (mode)
        {
            case Mode::none:
                if (stomp())
                    model.toggleStomp (cell);
                else
                    model.selectScene (cell);
                return;

            case Mode::bind:
                cancelMode();
                model.bindStomp (cell, model.getSelectedSlotId());
                flash (cell);
                return;

            case Mode::unbind:
                if (model.getStompSlot (cell) == nullptr)
                    return;   // 묶인 칸을 계속 기다린다

                cancelMode();
                model.unbindStomp (cell);
                flash (cell);
                return;

            case Mode::copy:
            case Mode::swap:
                break;
        }

        if (sourceCell < 0)
        {
            sourceCell = cell;
            updateTimer();
            repaint();
            return;
        }

        if (cell == sourceCell)
            return;   // dst 를 계속 기다린다

        const int source = sourceCell;
        const bool swapping = mode == Mode::swap;
        cancelMode();

        if (swapping)
        {
            stomp() ? model.swapStomp (source, cell) : model.swapScene (source, cell);
            flash (source);
        }
        else
        {
            stomp() ? model.copyStomp (source, cell) : model.copyScene (source, cell);
        }

        flash (cell);
    }

    //==============================================================================
    void SceneBar::toggleMode (Mode newMode)
    {
        if (mode == newMode)
        {
            cancelMode();
            return;
        }

        cancelRename();

        if (mode == Mode::none)
            juce::Desktop::getInstance().addGlobalMouseListener (&outsideClickWatcher);

        mode = newMode;
        sourceCell = -1;
        copyButton.setToggleState (mode == Mode::copy, juce::dontSendNotification);
        swapButton.setToggleState (mode == Mode::swap, juce::dontSendNotification);
        bindButton.setToggleState (mode == Mode::bind, juce::dontSendNotification);
        unbindButton.setToggleState (mode == Mode::unbind, juce::dontSendNotification);
        grabKeyboardFocus();
        updateTimer();
        repaint();
    }

    void SceneBar::cancelMode()
    {
        if (mode == Mode::none)
            return;

        juce::Desktop::getInstance().removeGlobalMouseListener (&outsideClickWatcher);
        mode = Mode::none;
        sourceCell = -1;

        for (auto* b : { &copyButton, &swapButton, &bindButton, &unbindButton })
            b->setToggleState (false, juce::dontSendNotification);

        updateButtons();
        updateTimer();
        repaint();
    }

    void SceneBar::OutsideClickWatcher::mouseDown (const juce::MouseEvent& e)
    {
        if (e.eventComponent != &owner && ! owner.isParentOf (e.eventComponent))
            owner.cancelMode();
    }

    bool SceneBar::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey && mode != Mode::none)
        {
            cancelMode();
            return true;
        }

        return false;
    }

    //==============================================================================
    void SceneBar::startRename (int scene)
    {
        cancelRename();
        renamingScene = nameTargetScene = scene;

        layoutNameEditor();
        nameEditor.setText (model.getSceneName (scene), juce::dontSendNotification);
        nameEditor.setVisible (true);
        saveNameButton.setVisible (true);
        nameEditor.showEditor();
    }

    // 입력칸은 씬 버튼 전체, 정사각형 체크 버튼은 그 오른쪽 위 귀퉁이에 겹쳐 둔다
    void SceneBar::layoutNameEditor()
    {
        const auto area = cellBounds[(size_t) renamingScene];
        nameEditor.setBounds (area);
        saveNameButton.setBounds (area.getRight() - saveButtonSize, area.getY(), saveButtonSize, saveButtonSize);
        saveNameButton.toFront (false);
    }

    void SceneBar::cancelRename()
    {
        if (renamingScene >= 0)
            nameEditor.hideEditor (true);
    }

    void SceneBar::cancelEditing()
    {
        cancelMode();
        cancelRename();
    }

    //==============================================================================
    void SceneBar::flash (int cell)
    {
        flashStart[(size_t) cell] = juce::jmax<juce::uint32> (1, juce::Time::getMillisecondCounter());
        updateTimer();
        repaint();
    }

    void SceneBar::updateTimer()
    {
        const bool flashing = std::any_of (flashStart.begin(), flashStart.end(), [] (auto t) { return t != 0; });
        const bool pulsing = sourceCell >= 0;

        if (flashing || pulsing)
        {
            if (! isTimerRunning())
                startTimerHz (animationHz);
        }
        else
        {
            stopTimer();
        }
    }

    void SceneBar::timerCallback()
    {
        const auto now = juce::Time::getMillisecondCounter();

        for (auto& start : flashStart)
            if (start != 0 && now - start > flashMs)
                start = 0;

        updateTimer();
        repaint();
    }

    //==============================================================================
    void SceneBar::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.15f));

        const auto now = juce::Time::getMillisecondCounter();
        // 위상은 정수 나머지로 (ms 카운터를 통째로 float 로 바꾸면 부팅 뒤 시간이 길수록 정밀도가 떨어져 계단이 된다)
        const float phase = (float) (now % pulseMs) / (float) pulseMs;
        const float pulse = 0.5f + 0.5f * std::sin (phase * juce::MathConstants<float>::twoPi);
        const bool s = stomp();
        const auto& touched = s ? model.getHighlight().stomps : model.getHighlight().scenes;

        for (int i = 0; i < (int) cellBounds.size(); ++i)
        {
            const auto bounds = cellBounds[(size_t) i].toFloat();
            const auto* bound = s ? model.getStompSlot (i) : nullptr;
            const bool lit = mode == Mode::none && (s ? model.isStompOn (i) : i == model.getCurrentScene());
            const bool source = i == sourceCell;

            auto fill = lit ? juce::Colours::lightgrey : juce::Colour (0xff3a4148);
            if (source)
                fill = fill.interpolatedWith (juce::Colours::white, 0.1f + 0.35f * pulse);   // 흰 글자가 읽히는 밝기까지만

            if (i == renamingScene)
                fill = juce::Colours::black.withAlpha (0.4f).overlaidWith (juce::Colour (0xff1e2328));   // 입력칸 바탕

            g.setColour (fill);
            g.fillRoundedRectangle (bounds, sceneCorner);

            g.setColour (i == renamingScene ? juce::Colours::grey : juce::Colours::grey.withAlpha (0.6f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), sceneCorner, 1.0f);

            // 실행 뒤 테두리 강조: 부드럽게 두 번 깜빡인다 (켜졌다 꺼지는 한 번 = sin² 의 반 주기)
            if (const auto start = flashStart[(size_t) i]; start != 0 && now - start < flashMs)
            {
                const float sine = std::sin (juce::MathConstants<float>::pi * (float) (now - start) / (float) blinkMs);
                const float alpha = sine * sine;
                g.setColour (juce::Colours::orange.brighter (0.5f * alpha).withAlpha (alpha));
                g.drawRoundedRectangle (bounds.reduced (1.5f), sceneCorner, 3.0f);
            }

            // Undo / Redo 로 되돌아간 칸
            if (std::find (touched.begin(), touched.end(), i) != touched.end())
                drawHighlight (g, bounds.reduced (2.0f), highlightAlpha (model.getHighlightStart()), sceneCorner);

            // src 는 밝기가 오르내리는 동안 글자색을 바꾸지 않는다 (중간에 흑백이 뒤집히면 뚝 끊겨 보인다)
            const auto textColour = lit ? juce::Colours::black : juce::Colours::white;

            // 번호: 왼쪽 위, 크게 / 이름: 아래 가운데. 안 묶인 Stomp 칸은 번호만 흐리게
            g.setColour (textColour.withAlpha (s && bound == nullptr ? unboundAlpha : 0.7f));
            g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
            g.drawText (juce::String (i + 1), cellBounds[(size_t) i].reduced (7, 4), juce::Justification::topLeft);

            if (i == renamingScene)
                continue;   // 이름 자리는 입력칸이 같은 위치에 쓴다

            const auto name = s ? (bound != nullptr ? RigModel::shortName (*bound) : juce::String()) : model.getSceneName (i);
            g.setColour (textColour);
            g.setFont (juce::FontOptions (nameFontSize));
            g.drawText (name, cellBounds[(size_t) i].reduced (nameInset), juce::Justification::centredBottom, true);
        }
    }

    void SceneBar::resized()
    {
        const auto area = getLocalBounds().reduced (gap);

        // 14칸 격자 (bottomBarCell, Live statbar 와 같은 격자):
        //   [Scene / Stomp Reset] [ ] [Rename / Reset | Bind / Unbind] [1] ~ [8] [Copy / Swap] [Scene|Stomp] [Live]
        // 전체 리셋은 왼쪽 끝, Live 는 오른쪽 끝. 버튼은 모두 같은 한 칸 (바깥 여백·버튼 사이 6 px). 위아래 두 버튼은 한 칸을 나눈다 (사이 6 px)
        resetAllButton.setBounds (bottomBarCell (area, resetCell));
        liveButton.setBounds (bottomBarCell (area, bottomBarCells - 1));
        modeSwitch.setBounds (bottomBarCell (area, bottomBarCells - 2));

        for (int i = 0; i < (int) cellBounds.size(); ++i)
            cellBounds[(size_t) i] = bottomBarCell (area, firstCell + i);

        stack (bottomBarCell (area, actionCell), renameButton, sceneResetButton);
        stack (bottomBarCell (area, actionCell), bindButton, unbindButton);
        stack (bottomBarCell (area, copyCell), copyButton, swapButton);

        if (renamingScene >= 0)
            layoutNameEditor();
    }
}
