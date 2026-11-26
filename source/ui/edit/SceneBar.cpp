#include "SceneBar.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace rig
{
    namespace
    {
        // 놓은 뒤 테두리: 자동차 할로겐 방향지시등처럼 부드럽게 두 번 깜빡인다 (한 번 = blinkMs, sin² 곡선)
        constexpr juce::uint32 blinkMs = 400, numBlinks = 2, flashMs = blinkMs * numBlinks;
        constexpr int animationHz = 60;         // 놓은 뒤 깜빡임 다시 그리기
        constexpr int gap = bottomBarGap;       // 바깥 여백 (버튼 사이도 같은 값, 14칸 격자 bottomBarCell)
        constexpr int saveButtonSize = 24;      // 이름 입력칸 오른쪽 위 귀퉁이의 정사각형 체크 버튼
        constexpr float nameFontSize = 12.0f;   // 씬 이름 (입력칸도 같은 크기)
        constexpr float numberFontSize = 14.0f; // 씬 번호 (왼쪽 위, 굵게. Stomp 칸 머리띠 글자와 같은 크기·자리)
        constexpr int nameInset = 6;            // 이름 칸 좌우 여백
        constexpr int numberRowHeight = 20;     // 씬 번호 줄 (왼쪽 위). 이름 칸은 위아래를 이만큼 똑같이 비워 가운데에 둔다
        constexpr float stompNameSize = block::bodyFontSize;   // Stomp 칸 파일명 (블록 본문과 같게 12, 머리띠는 블록과 같은 14)
        constexpr int dragStartDistance = 4;    // 칸을 이만큼 움직여야 끌기 (클릭과 구분, 패치맵과 같다)
        constexpr int keepMargin = 22;          // 끌던 Stomp 칸의 둘레 = 원래 칸 + 사방 이만큼 (패치맵 keepMargin 과 같은 값). 밖에서 떼면 삭제
        constexpr float markArm = 6.0f;         // 놓을 자리 힌트의 표시 기호 크기 (가운데에서 팔 끝까지)

        // 놓을 자리 표시 (ChainView 와 같은 색): 놓을 수 있는 칸 전부 희미하게, 나뉜 칸의 선·기호, 커서 밑 자리 진하게
        const juce::Colour dropHint (juce::Colours::white.withAlpha (0.12f));
        const juce::Colour dropHighlight (juce::Colours::white.withAlpha (0.35f));
        const juce::Colour dropHover (juce::Colours::white.withAlpha (0.8f));
        constexpr float hoverThickness = 3.0f;

        // 14칸 격자 칸 번호 (0부터)
        constexpr int resetCell = 0, actionCell = 2, firstCell = 3;

        // 한 칸을 위아래 두 버튼으로 (사이 gap)
        void stack (juce::Rectangle<int> cell, juce::Component& top, juce::Component& bottom)
        {
            top.setBounds (cell.removeFromTop ((cell.getHeight() - gap) / 2));
            cell.removeFromTop (gap);
            bottom.setBounds (cell);
        }

        // 나뉜 칸의 위 / 아래 절반
        juce::Rectangle<int> halfOf (juce::Rectangle<int> cell, bool upper)
        {
            return upper ? cell.withHeight (cell.getHeight() / 2) : cell.withTrimmedTop (cell.getHeight() / 2);
        }
    }

    SceneBar::SceneBar (RigModel& m) : model (m), modeSwitch (m)
    {
        addAndMakeVisible (modeSwitch);
        shownPatch = model.getPatchNumber();
        shownStomp = stomp();

        liveButton.onClick = [this] { if (onLiveClicked) onLiveClicked(); };
        disableHoverHighlight (liveButton);   // Live 와 연관된 버튼
        addAndMakeVisible (liveButton);

        for (auto* b : { &renameButton, &sceneResetButton, &resetAllButton })
        {
            b->setWantsKeyboardFocus (false);
            b->setMouseClickGrabsKeyboardFocus (false);
            addChildComponent (b);
        }

        renameButton.setTooltip ("Rename the current scene");
        renameButton.onClick = [this] { startRename (model.getCurrentScene()); };

        // 씬 하나 = 중간 리셋이라 경고 없이 바로
        sceneResetButton.setTooltip ("Reset the current scene: every block On with default parameters, branches -3 dB, default name");
        sceneResetButton.onClick = [this]
        {
            cancelRename();
            model.resetScene (model.getCurrentScene());
        };

        // 경고창에서 Reset 을 눌러야 초기화
        resetAllButton.onClick = [this]
        {
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
        // 입력칸 자체는 투명: 바탕과 테두리는 paint() 가 씬 버튼 모양대로 그린다 (귀퉁이 체크 버튼과 모양이 맞도록)
        nameEditor.setJustificationType (juce::Justification::centred);
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
                // 버튼에 이름을 그리는 위치와 같게: 칸 가운데, 좌우 nameInset 안쪽
                ed->setJustification (juce::Justification::centred);
                ed->setIndents (0, 0);
                ed->setBorder ({ 0, nameInset, 0, nameInset });
            }
        };
        nameEditor.onEditorHide = [this]
        {
            renamingScene = -1;
            nameEditor.setVisible (false);
            saveNameButton.setVisible (false);
            repaint();
        };
        // 체크 버튼·Enter 로 저장하면 이름이 그대로여도 저장한다 (조작했으면 Undo). 편집기를 닫은 뒤(onEditorHide 다음)에 온다
        nameEditor.onCommit = [this] (const juce::String& name)
        {
            if (nameTargetScene >= 0)
                model.renameScene (nameTargetScene, name);
        };
        nameEditor.allowWhileEditing = [this] (const juce::Component* target) { return target == &saveNameButton; };
        addChildComponent (nameEditor);

        // 체크 버튼을 눌러도 입력칸의 포커스를 빼앗지 않아야 저장된다 (포커스를 잃으면 취소되므로)
        saveNameButton.setTooltip ("Save scene name (Enter to save, Escape to cancel)");
        saveNameButton.setWantsKeyboardFocus (false);
        saveNameButton.setMouseClickGrabsKeyboardFocus (false);
        saveNameButton.onClick = [this] { nameEditor.commit(); };
        addChildComponent (saveNameButton);

        model.addChangeListener (this);
        updateButtons();
    }

    SceneBar::~SceneBar()
    {
        model.removeChangeListener (this);
    }

    //==============================================================================
    void SceneBar::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        // 패치나 Scene / Stomp 가 바뀌면 이름 편집을 취소한다
        if (model.getPatchNumber() != shownPatch || stomp() != shownStomp)
        {
            shownPatch = model.getPatchNumber();
            shownStomp = stomp();
            cancelRename();
            flashStart.fill (0);
        }

        updateButtons();
        repaint();   // 이름과 현재 씬 / Stomp 칸
    }

    void SceneBar::updateButtons()
    {
        const bool s = stomp();

        renameButton.setVisible (! s);
        sceneResetButton.setVisible (! s);
        resetAllButton.setVisible (true);
        resetAllButton.setButtonText (s ? "Stomp\nReset..." : "Scene\nReset...");
        resetAllButton.setTooltip (s ? "Unbind all stomps"
                                     : "Reset all scenes: every block On with default parameters, branches -3 dB, default names");
    }

    int SceneBar::cellAt (juce::Point<int> position) const
    {
        for (int i = 0; i < (int) cellBounds.size(); ++i)
            if (cellBounds[(size_t) i].contains (position))
                return i;

        return -1;
    }

    //==============================================================================
    // 클릭 = 누른 칸에서 끌지 않고 뗐을 때. 끌기 = 누른 칸에서 dragStartDistance 이상 움직였을 때 (빈 Stomp 칸은 끌 수 없다)
    void SceneBar::mouseDown (const juce::MouseEvent& e)
    {
        pressedCell = isLeftClick (e) ? cellAt (e.getPosition()) : -1;
    }

    void SceneBar::mouseDrag (const juce::MouseEvent& e)
    {
        if (pressedCell < 0 || ! isLeftClick (e) || e.getDistanceFromDragStart() < dragStartDistance)
            return;

        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
        if (container == nullptr)
            return;

        // 끄는 동안 커서의 칸 그림 표시: 떼면 일어날 일 (놓을 자리 = + / ⇄ / 없음, 둘레 밖 Stomp = X)
        if (container->isDragAndDropActive())
        {
            if (! dragSource.has_value() || dragSource->cell != pressedCell)
                return;

            const auto drop = dropAt (*dragSource, e.getPosition());
            const auto mark = drop.has_value() ? markFor (*drop)
                            : isDeleteAt (e.getScreenPosition()) ? block::DragMark::remove
                                                                 : block::DragMark::none;

            if (mark != shownMark)
            {
                shownMark = mark;
                container->setCurrentDragImage (juce::ScaledImage (block::createDragImage (dragSnapshot, mark)));
            }

            return;
        }

        if (stomp() && ! bound (pressedCell))
            return;

        cancelRename();
        dragSnapshot = createComponentSnapshot (cellBounds[(size_t) pressedCell]);
        shownMark = block::DragMark::none;
        const auto image = block::createDragImage (dragSnapshot, shownMark);

        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("source", "ssbar");
        obj->setProperty ("cell", pressedCell);

        // 커서 = 칸 가운데 = 끄는 그림 가운데
        const juce::Point<int> centre (-image.getWidth() / 2, -image.getHeight() / 2);
        container->startDragging (juce::var (obj.release()), this, juce::ScaledImage (image), false, &centre);
    }

    // 끌다 뗐을 때: 놓을 자리 위면 itemDropped 가 처리 (이 mouseUp 이 먼저 불린다), Stomp 칸을 둘레 밖에서 떼면 여기서 삭제.
    // Esc 로 취소했으면 끌기가 이미 끝나 있다
    void SceneBar::mouseUp (const juce::MouseEvent& e)
    {
        const int pressed = std::exchange (pressedCell, -1);

        if (pressed < 0 || ! isLeftClick (e))
            return;

        if (! e.mouseWasDraggedSinceMouseDown())
        {
            if (cellAt (e.getPosition()) == pressed)
                cellClicked (pressed);

            return;
        }

        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
        const bool remove = container != nullptr && container->isDragAndDropActive() && dragSource.has_value()
                         && dragSource->cell == pressed && isDeleteAt (e.getScreenPosition());
        dragEnded();   // 끄는 그림이 사라지는 동안 힌트가 남지 않게 (컨테이너의 끝 알림은 그림이 사라진 뒤에 온다)

        if (remove)
            model.unbindStomp (pressed);
    }

    void SceneBar::cellClicked (int cell)
    {
        if (stomp())
            model.toggleStomp (cell);   // 안 묶인 칸은 아무 일 없음
        else
            model.selectScene (cell);
    }

    //==============================================================================
    std::optional<SceneBar::DragSource> SceneBar::readSource (const juce::var& description) const
    {
        const auto source = description.getProperty ("source", {}).toString();

        if (source == "ssbar")
            return DragSource { (int) description.getProperty ("cell", -1), 0 };

        // 패치맵 블록은 Stomp 모드에서만 받는다 (Scene 모드는 패치맵과 ssbar 사이 동작 없음)
        if (source == "map" && stomp())
            return DragSource { -1, (juce::uint64) description.getProperty ("slot", {}).toString().getLargeIntValue() };

        return std::nullopt;
    }

    std::optional<SceneBar::Drop> SceneBar::dropAt (const DragSource& source, juce::Point<int> position) const
    {
        const int cell = cellAt (position);
        if (cell < 0 || cell == source.cell)
            return std::nullopt;   // 자기 칸은 놓을 자리가 아니다

        // 패치맵 블록 → Stomp 칸: 칸을 나누지 않는다
        if (source.cell < 0)
            return Drop { cell, false, false, bound (cell) ? Action::replace : Action::add };

        const bool upper = position.y < cellBounds[(size_t) cell].getCentreY();
        const bool filled = ! stomp() || bound (cell);   // Scene 칸은 늘 차 있다

        if (upper)
            return Drop { cell, true, true, filled ? Action::copy : Action::add };

        return Drop { cell, false, true, filled ? Action::swap : Action::move };
    }

    block::DragMark SceneBar::markFor (const Drop& drop)
    {
        switch (drop.action)
        {
            case Action::copy:
            case Action::add:     return block::DragMark::add;
            case Action::swap:    return block::DragMark::swap;
            case Action::replace: return block::DragMark::replace;
            case Action::move:    break;
        }

        return block::DragMark::none;   // 이동은 표시 없음 (패치맵 이동과 같다)
    }

    void SceneBar::perform (const DragSource& source, const Drop& drop)
    {
        const int s = source.cell, d = drop.cell;

        switch (drop.action)
        {
            case Action::copy:
                stomp() ? model.copyStomp (s, d) : model.copyScene (s, d);
                break;

            case Action::add:
                if (s >= 0)
                    model.copyStomp (s, d);
                else
                    model.bindStomp (d, source.slotId);
                break;

            case Action::replace:
                model.bindStomp (d, source.slotId);
                break;

            case Action::swap:
            case Action::move:   // 빈 칸과 맞바꾸면 그 칸으로 옮겨진다
                stomp() ? model.swapStomp (s, d) : model.swapScene (s, d);
                flash (s);
                break;
        }

        flash (d);
    }

    bool SceneBar::isDeleteAt (juce::Point<int> screen) const
    {
        if (! stomp() || ! dragSource.has_value() || dragSource->cell < 0)
            return false;

        const auto local = getLocalPoint (nullptr, screen);
        return ! cellBounds[(size_t) dragSource->cell].expanded (keepMargin).contains (local)
            && ! dropAt (*dragSource, local).has_value();
    }

    block::DragMark SceneBar::mapDropMark (juce::Point<int> screen) const
    {
        if (! dragSource.has_value() || dragSource->cell >= 0)
            return block::DragMark::none;

        const auto drop = dropAt (*dragSource, getLocalPoint (nullptr, screen));
        return drop.has_value() ? markFor (*drop) : block::DragMark::none;
    }

    juce::Image SceneBar::stompDragImage (juce::uint64 slotId, block::DragMark mark) const
    {
        const auto area = cellBounds[0].withZeroOrigin();
        juce::Image image (juce::Image::ARGB, juce::jmax (1, area.getWidth()), juce::jmax (1, area.getHeight()), true);

        if (const auto* slot = model.findSlot (slotId))
        {
            juce::Graphics g (image);
            block::drawStomp (g, area, slot->type, RigModel::shortName (*slot), false, 1.0f, block::headerFont(),
                              juce::Font (juce::FontOptions (stompNameSize)));
        }

        return block::createDragImage (image, mark);
    }

    //==============================================================================
    void SceneBar::dragStarted (const juce::var& description)
    {
        dragSource = readSource (description);
        hoverDrop.reset();
        repaint();
    }

    void SceneBar::dragEnded()
    {
        dragSource.reset();
        hoverDrop.reset();
        repaint();
    }

    bool SceneBar::isInterestedInDragSource (const SourceDetails& details)
    {
        return readSource (details.description).has_value();
    }

    void SceneBar::itemDragMove (const SourceDetails& details)
    {
        const auto source = readSource (details.description);
        const auto drop = source.has_value() ? dropAt (*source, details.localPosition) : std::nullopt;
        const bool same = drop.has_value() == hoverDrop.has_value()
                       && (! drop.has_value() || (drop->cell == hoverDrop->cell && drop->upper == hoverDrop->upper));

        if (! same)
        {
            hoverDrop = drop;
            repaint();
        }
    }

    void SceneBar::itemDragExit (const SourceDetails&)
    {
        hoverDrop.reset();
        repaint();
    }

    void SceneBar::itemDropped (const SourceDetails& details)
    {
        dragEnded();

        const auto source = readSource (details.description);
        if (! source.has_value())
            return;

        if (const auto drop = dropAt (*source, details.localPosition))
            perform (*source, *drop);
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
        if (std::any_of (flashStart.begin(), flashStart.end(), [] (auto t) { return t != 0; }))
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
        const bool s = stomp();
        const auto& touched = s ? model.getHighlight().stomps : model.getHighlight().scenes;

        for (int i = 0; i < (int) cellBounds.size(); ++i)
        {
            const auto cell = cellBounds[(size_t) i];
            const auto bounds = cell.toFloat();
            const auto* slot = s ? model.getStompSlot (i) : nullptr;

            if (slot != nullptr)
            {
                // 묶인 Stomp 칸 = 블록 모양 (꺼지면 흐리게)
                block::drawStomp (g, cell, slot->type, RigModel::shortName (*slot), ! model.isStompOn (i), 1.0f, block::headerFont(),
                                  juce::Font (juce::FontOptions (stompNameSize)));
            }
            else
            {
                // 씬 칸 / 빈 Stomp 칸
                const bool lit = ! s && i == model.getCurrentScene();
                auto fill = lit ? juce::Colours::lightgrey : juce::Colour (0xff3a4148);

                if (i == renamingScene)
                    fill = juce::Colours::black.withAlpha (0.4f).overlaidWith (juce::Colour (0xff1e2328));   // 입력칸 바탕

                g.setColour (fill);
                g.fillRect (bounds);
                g.setColour (i == renamingScene ? juce::Colours::grey : juce::Colours::grey.withAlpha (0.6f));
                strokeRect (g, bounds.reduced (0.5f), 1.0f);

                if (! s)
                {
                    // 번호: 왼쪽 위 / 이름: 가운데 (고정 글꼴로 줄마다 채우고 넘치면 말줄임. 번호 줄만큼 위아래를 똑같이 비운다)
                    const auto textColour = lit ? juce::Colours::black : juce::Colours::white;
                    g.setColour (textColour.withAlpha (0.7f));
                    block::drawBandLabel (g, juce::String (i + 1), juce::Font (juce::FontOptions (numberFontSize, juce::Font::bold)), cell, 1.0f);

                    if (i != renamingScene)   // 이름 자리는 입력칸이 같은 위치에 쓴다
                    {
                        g.setColour (textColour);
                        drawWrappedText (g, model.getSceneName (i), cell.reduced (nameInset, numberRowHeight).toFloat(),
                                        juce::Font (juce::FontOptions (nameFontSize)));
                    }
                }
            }

            // 놓은 뒤 테두리 강조: 부드럽게 두 번 깜빡인다 (켜졌다 꺼지는 한 번 = sin² 의 반 주기)
            if (const auto start = flashStart[(size_t) i]; start != 0 && now - start < flashMs)
            {
                const float sine = std::sin (juce::MathConstants<float>::pi * (float) (now - start) / (float) blinkMs);
                const float alpha = sine * sine;
                g.setColour (juce::Colours::orange.brighter (0.5f * alpha).withAlpha (alpha));
                strokeRect (g, bounds.reduced (1.5f), 3.0f);
            }

            // Undo / Redo 로 되돌아간 칸
            if (std::find (touched.begin(), touched.end(), i) != touched.end())
                drawHighlight (g, bounds.reduced (2.0f), highlightAlpha (model.getHighlightStart()));
        }

        paintDropHints (g);
    }

    // 끄는 동안: 놓을 수 있는 칸 전부 희미하게 + 표시 기호 (나뉜 칸은 가운데 선, 위아래 반쪽마다), 커서 밑 자리는 진한 테두리
    void SceneBar::paintDropHints (juce::Graphics& g)
    {
        if (! dragSource.has_value())
            return;

        for (int i = 0; i < (int) cellBounds.size(); ++i)
        {
            const auto cell = cellBounds[(size_t) i];
            const auto upper = dropAt (*dragSource, halfOf (cell, true).getCentre());
            if (! upper.has_value())
                continue;   // 자기 칸

            g.setColour (dropHint);
            g.fillRect (cell.toFloat());
            g.setColour (dropHighlight);

            if (! upper->split)
            {
                block::drawMarkSymbol (g, cell.toFloat().getCentre(), markFor (*upper), markArm);
                continue;
            }

            const auto lower = dropAt (*dragSource, halfOf (cell, false).getCentre());
            const float midY = (float) cell.getCentreY();
            g.drawLine ((float) cell.getX() + (float) nameInset, midY, (float) cell.getRight() - (float) nameInset, midY, 1.0f);
            block::drawMarkSymbol (g, halfOf (cell, true).toFloat().getCentre(), markFor (*upper), markArm);

            if (lower.has_value())
                block::drawMarkSymbol (g, halfOf (cell, false).toFloat().getCentre(), markFor (*lower), markArm);
        }

        if (hoverDrop.has_value())
        {
            const auto cell = cellBounds[(size_t) hoverDrop->cell];
            const auto area = hoverDrop->split ? halfOf (cell, hoverDrop->upper) : cell;
            g.setColour (dropHover);
            strokeRect (g, area.toFloat().reduced (hoverThickness * 0.5f), hoverThickness);
        }
    }

    void SceneBar::resized()
    {
        const auto area = getLocalBounds().reduced (gap);

        // 14칸 격자 (bottomBarCell, Live statbar 와 같은 격자):
        //   [Scene / Stomp Reset] [ ] [Rename / Reset (Scene)] [1] ~ [8] [ ] [Scene|Stomp] [Live]
        // 전체 리셋은 왼쪽 끝, Live 는 오른쪽 끝. 버튼은 모두 같은 한 칸 (바깥 여백·버튼 사이 6 px). 위아래 두 버튼은 한 칸을 나눈다 (사이 6 px)
        resetAllButton.setBounds (bottomBarCell (area, resetCell));
        liveButton.setBounds (bottomBarCell (area, bottomBarCells - 1));
        modeSwitch.setBounds (bottomBarCell (area, bottomBarCells - 2));

        for (int i = 0; i < (int) cellBounds.size(); ++i)
            cellBounds[(size_t) i] = bottomBarCell (area, firstCell + i);

        stack (bottomBarCell (area, actionCell), renameButton, sceneResetButton);

        if (renamingScene >= 0)
            layoutNameEditor();
    }
}
