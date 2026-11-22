#include "SceneBar.h"

#include <algorithm>
#include <cmath>

namespace rig
{
    namespace
    {
        // 실행 뒤 테두리: 자동차 할로겐 방향지시등처럼 부드럽게 두 번 깜빡인다 (한 번 = blinkMs, sin² 곡선)
        constexpr juce::uint32 blinkMs = 380, numBlinks = 2, flashMs = blinkMs * numBlinks;
        constexpr int gap = 6;                  // 바깥 여백, 버튼 사이, Copy / Swap / Reset 사이 모두 같은 폭
        constexpr int saveButtonSize = 24;      // 이름 입력칸 오른쪽 위 귀퉁이의 정사각형 체크 버튼
        constexpr float sceneCorner = 5.0f;     // 씬 버튼 둥근 모서리 반지름
        constexpr float nameFontSize = 13.0f;   // 씬 이름 (버튼 표시와 입력칸이 같은 크기와 위치)
        constexpr int nameInset = 6;
    }

    SceneBar::SceneBar (RigModel& m) : model (m)
    {
        setWantsKeyboardFocus (true);   // 모드 중 ESC 를 받는다
        shownPatch = model.getPatchNumber();

        liveButton.onClick = [this] { if (onLiveClicked) onLiveClicked(); };
        addAndMakeVisible (liveButton);

        for (auto* b : { &copyButton, &swapButton, &sceneResetButton })
        {
            // 눌러도 키보드 포커스를 가져가지 않는다 (ESC 는 씬 바가 받는다)
            b->setWantsKeyboardFocus (false);
            b->setMouseClickGrabsKeyboardFocus (false);
            b->setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange.darker (0.2f));
            addAndMakeVisible (b);
        }

        copyButton.setTooltip ("Copy a scene: pick the source scene, then the destination");
        swapButton.setTooltip ("Swap two scenes: pick one scene, then the other");
        copyButton.onClick = [this] { toggleMode (Mode::copy); };
        swapButton.onClick = [this] { toggleMode (Mode::swap); };

        // Copy / Swap 모드 중이면 바깥 클릭처럼 모드를 끝낸다. 경고창에서 Reset 을 눌러야 초기화
        sceneResetButton.setTooltip ("Reset all scenes: every block On, default names");
        sceneResetButton.onClick = [this]
        {
            cancelMode();
            cancelRename();

            juce::Component::SafePointer<SceneBar> safeThis (this);
            showResetWarning (*this, "Scene Reset",
                              "Reset all 8 scenes?\n\nEvery block will be On in every scene and all scene names "
                              "go back to \"Scene 1\" ... \"Scene 8\".",
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
    }

    SceneBar::~SceneBar()
    {
        juce::Desktop::getInstance().removeGlobalMouseListener (&outsideClickWatcher);
        model.removeChangeListener (this);
    }

    //==============================================================================
    void SceneBar::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        // 패치가 바뀌면 (씬 1 로 열린다) 진행 중인 모드와 이름 편집을 취소한다
        if (model.getPatchNumber() != shownPatch)
        {
            shownPatch = model.getPatchNumber();
            cancelMode();
            cancelRename();
        }

        repaint();   // 이름과 현재 씬 강조
    }

    int SceneBar::sceneAt (juce::Point<int> position) const
    {
        for (int i = 0; i < RigModel::numScenes; ++i)
            if (sceneBounds[(size_t) i].contains (position))
                return i;

        return -1;
    }

    void SceneBar::mouseDown (const juce::MouseEvent& e)
    {
        const int scene = sceneAt (e.getPosition());

        if (scene < 0)
            return;

        if (e.mods.isPopupMenu())
            showSceneMenu (scene, e);
        else if (isLeftClick (e))
            sceneClicked (scene);
    }

    void SceneBar::sceneClicked (int scene)
    {
        if (mode == Mode::none)
        {
            model.selectScene (scene);
            return;
        }

        if (sourceScene < 0)
        {
            sourceScene = scene;
            updateTimer();
            repaint();
            return;
        }

        if (scene == sourceScene)
            return;   // dst 를 계속 기다린다

        const int source = sourceScene;
        const bool swapping = mode == Mode::swap;
        cancelMode();

        if (swapping)
        {
            model.swapScene (source, scene);
            flash (source);
        }
        else
        {
            model.copyScene (source, scene);
        }

        flash (scene);
    }

    void SceneBar::showSceneMenu (int scene, const juce::MouseEvent& click)
    {
        // 오른쪽 클릭은 전환하지 않는다
        cancelMode();

        juce::PopupMenu menu;
        menu.addItem ("Rename", [this, scene] { startRename (scene); });
        menu.addItem ("Reset", [this, scene] { model.resetScene (scene); });   // 씬 하나라 경고 없이 바로
        showContextMenu (menu, click);
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
        sourceScene = -1;
        copyButton.setToggleState (mode == Mode::copy, juce::dontSendNotification);
        swapButton.setToggleState (mode == Mode::swap, juce::dontSendNotification);
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
        sourceScene = -1;
        copyButton.setToggleState (false, juce::dontSendNotification);
        swapButton.setToggleState (false, juce::dontSendNotification);
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
        const auto area = sceneBounds[(size_t) renamingScene];
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
    void SceneBar::flash (int scene)
    {
        flashStart[(size_t) scene] = juce::jmax<juce::uint32> (1, juce::Time::getMillisecondCounter());
        updateTimer();
        repaint();
    }

    void SceneBar::updateTimer()
    {
        const bool flashing = std::any_of (flashStart.begin(), flashStart.end(), [] (auto t) { return t != 0; });
        const bool pulsing = sourceScene >= 0;

        if (flashing || pulsing)
        {
            if (! isTimerRunning())
                startTimerHz (30);
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
        const float pulse = 0.5f + 0.5f * std::sin ((float) now * 0.001f * juce::MathConstants<float>::twoPi * 1.5f);

        for (int i = 0; i < RigModel::numScenes; ++i)
        {
            const auto bounds = sceneBounds[(size_t) i].toFloat();
            const bool current = mode == Mode::none && i == model.getCurrentScene();
            const bool source = i == sourceScene;

            auto fill = current ? juce::Colours::lightgrey : juce::Colour (0xff3a4148);
            if (source)
                fill = fill.interpolatedWith (juce::Colours::white, 0.15f + 0.55f * pulse);

            if (i == renamingScene)
                fill = juce::Colours::black.withAlpha (0.4f).overlaidWith (juce::Colour (0xff1e2328));   // 입력칸 바탕

            g.setColour (fill);
            g.fillRoundedRectangle (bounds, sceneCorner);

            g.setColour (i == renamingScene ? juce::Colours::grey : juce::Colours::grey.withAlpha (0.6f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), sceneCorner, 1.0f);

            // 실행 뒤 테두리 강조: 부드럽게 두 번 깜빡인다 (켜졌다 꺼지는 한 번 = sin² 의 반 주기)
            if (const auto start = flashStart[(size_t) i]; start != 0 && now - start < flashMs)
            {
                const float s = std::sin (juce::MathConstants<float>::pi * (float) (now - start) / (float) blinkMs);
                const float alpha = s * s;
                g.setColour (juce::Colours::orange.brighter (0.5f * alpha).withAlpha (alpha));
                g.drawRoundedRectangle (bounds.reduced (1.5f), sceneCorner, 3.0f);
            }

            const auto textColour = current || (source && pulse > 0.5f) ? juce::Colours::black : juce::Colours::white;

            // 번호: 왼쪽 위, 크게 / 이름: 아래 가운데
            g.setColour (textColour.withAlpha (0.7f));
            g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
            g.drawText (juce::String (i + 1), sceneBounds[(size_t) i].reduced (7, 4), juce::Justification::topLeft);

            if (i == renamingScene)
                continue;   // 이름 자리는 입력칸이 같은 위치에 쓴다

            g.setColour (textColour);
            g.setFont (juce::FontOptions (nameFontSize));
            g.drawText (model.getSceneName (i), sceneBounds[(size_t) i].reduced (nameInset), juce::Justification::centredBottom, true);
        }
    }

    void SceneBar::resized()
    {
        auto area = getLocalBounds().reduced (gap);

        // [Scene Reset] 는 왼쪽 끝 (전체 리셋은 왼쪽), [1] ~ [8] [Copy/Swap] 묶음은 가운데, [Live] 는 오른쪽 끝.
        // 버튼 폭은 모두 같은 정사각형 한 칸 (= 높이). Copy / Swap 은 한 칸을 위아래로 나눈다. 사이는 모두 gap
        const int size = area.getHeight();

        sceneResetButton.setBounds (area.getX(), area.getY(), size, size);
        liveButton.setBounds (area.getRight() - size, area.getY(), size, size);

        constexpr int columns = RigModel::numScenes + 1;   // 씬 8개 + Copy / Swap
        const int groupWidth = columns * size + (columns - 1) * gap;
        const int groupLeft = getLocalBounds().getCentreX() - groupWidth / 2;

        for (int i = 0; i < RigModel::numScenes; ++i)
            sceneBounds[(size_t) i] = { groupLeft + i * (size + gap), area.getY(), size, size };

        auto modeArea = juce::Rectangle<int> (groupLeft + RigModel::numScenes * (size + gap), area.getY(), size, size);
        copyButton.setBounds (modeArea.removeFromTop ((size - gap) / 2));
        modeArea.removeFromTop (gap);
        swapButton.setBounds (modeArea);

        if (renamingScene >= 0)
            layoutNameEditor();
    }
}
