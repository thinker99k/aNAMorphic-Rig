#include "TopBar.h"

#include "model/RigModel.h"
#include "ui/Widgets.h"

namespace rig
{
    TopBar::TopBar (int numPatches)
    {
        for (int i = 1; i <= numPatches; ++i)
            patchBox.addItem (juce::String (i).paddedLeft ('0', 2), i);

        const auto arrowColour = previousPatchButton.findColour (juce::TextButton::textColourOffId);

        juce::DrawablePath previousPatchIcon;
        juce::Path previousPatchPath;
        previousPatchPath.addTriangle (10.0f, 0.0f, 10.0f, 10.0f, 0.0f, 5.0f);
        previousPatchIcon.setPath (previousPatchPath);
        previousPatchIcon.setFill (arrowColour);
        previousPatchButton.setImages (&previousPatchIcon);

        juce::DrawablePath nextPatchIcon;
        juce::Path nextPatchPath;
        nextPatchPath.addTriangle (0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 5.0f);
        nextPatchIcon.setPath (nextPatchPath);
        nextPatchIcon.setFill (arrowColour);
        nextPatchButton.setImages (&nextPatchIcon);

        patchBox.onChange = [this]
        {
            patchNameBox.hideEditor (true);   // 패치를 바꾸면 편집 중이던 이름은 버린다
            updateNavigationButtons();

            if (onPatchChanged)
                onPatchChanged (patchBox.getSelectedId());
        };

        previousPatchButton.onClick = [this] { selectAdjacentPatch (-1); };
        nextPatchButton.onClick = [this] { selectAdjacentPatch (1); };

        previousPatchButton.setTooltip ("Previous patch");
        nextPatchButton.setTooltip ("Next patch");

        settingsButton.onClick = [this]
        {
            if (onSettingsClicked)
                onSettingsClicked();
        };

        patchLabel.setJustificationType (juce::Justification::centredRight);
        patchNameBox.setJustificationType (juce::Justification::centred);
        patchNameBox.setColour (juce::Label::backgroundColourId, juce::Colours::black.withAlpha (0.25f));
        patchNameBox.setColour (juce::Label::outlineColourId, juce::Colours::grey);
        patchNameBox.setEditable (false, false, true);   // 포커스를 잃으면(바깥 클릭) 편집 취소
        patchNameBox.onEditorShow = [this]
        {
            if (auto* ed = patchNameBox.getCurrentTextEditor())
            {
                // 평소 표시와 같은 자리에 글자가 오도록: Label 과 같은 가운데 정렬, 같은 테두리 여백
                ed->setInputRestrictions (RigModel::maxPatchNameLength);
                ed->setJustification (juce::Justification::centred);
                ed->setIndents (0, 0);
                ed->setBorder (patchNameBox.getBorderSize());
            }

            saveNameButton.setVisible (true);
            resized();
        };
        patchNameBox.onEditorHide = [this] { saveNameButton.setVisible (false); };
        patchNameBox.onTextChange = [this]
        {
            if (onPatchNameChanged)
                onPatchNameChanged (patchNameBox.getText());
        };

        // 체크 버튼을 눌러도 입력칸의 키보드 포커스를 빼앗지 않아야 저장된다 (포커스를 잃으면 취소되므로)
        saveNameButton.setTooltip ("Save patch name (Enter to save, Escape to cancel)");
        saveNameButton.setWantsKeyboardFocus (false);
        saveNameButton.setMouseClickGrabsKeyboardFocus (false);
        saveNameButton.onClick = [this] { patchNameBox.hideEditor (false); };
        patchNameBox.allowWhileEditing = [this] (const juce::Component* target) { return target == &saveNameButton; };
        patchNameBox.addMouseListener (this, false);   // 오른쪽 클릭 메뉴는 TopBar::mouseDown 에서

        addAndMakeVisible (patchLabel);
        addAndMakeVisible (patchBox);
        addAndMakeVisible (previousPatchButton);
        addAndMakeVisible (nextPatchButton);
        addAndMakeVisible (patchNameBox);
        addChildComponent (saveNameButton);
        addAndMakeVisible (settingsButton);

        updateNavigationButtons();
    }

    void TopBar::setPatchNumber (int number)
    {
        patchBox.setSelectedId (number, juce::dontSendNotification);
        updateNavigationButtons();
    }

    void TopBar::selectAdjacentPatch (int delta)
    {
        const auto nextPatch = patchBox.getSelectedId() + delta;

        if (nextPatch >= 1 && nextPatch <= patchBox.getNumItems())
            patchBox.setSelectedId (nextPatch, juce::sendNotificationSync);
    }

    void TopBar::updateNavigationButtons()
    {
        const auto patchNumber = patchBox.getSelectedId();
        previousPatchButton.setEnabled (patchNumber > 1);
        nextPatchButton.setEnabled (patchNumber > 0 && patchNumber < patchBox.getNumItems());
    }

    void TopBar::setPatchName (const juce::String& name)
    {
        patchNameBox.setText (name, juce::dontSendNotification);
        patchNameBox.setTooltip (name);
    }

    void TopBar::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.3f));
    }

    void TopBar::resized()
    {
        auto area = getLocalBounds().reduced (8, 6);
        patchLabel.setBounds (area.removeFromLeft (50));
        area.removeFromLeft (4);
        patchBox.setBounds (area.removeFromLeft (80));
        area.removeFromLeft (4);
        previousPatchButton.setBounds (area.removeFromLeft (28));
        area.removeFromLeft (4);
        nextPatchButton.setBounds (area.removeFromLeft (28));
        settingsButton.setBounds (area.removeFromRight (110));

        const int sideMargin = juce::jmax (area.getX(), getWidth() - area.getRight()) + 48;
        const int nameWidth = juce::jlimit (0, 420, getWidth() - 2 * sideMargin);
        patchNameBox.setBounds ((getWidth() - nameWidth) / 2, area.getY(), nameWidth, area.getHeight());

        // 체크 버튼: 입력칸 오른쪽 끝, 입력칸 높이의 정사각형 (세로 가운데)
        const auto nameBounds = patchNameBox.getBounds();
        const int checkSize = nameBounds.getHeight();
        saveNameButton.setBounds (nameBounds.getRight() - checkSize, nameBounds.getCentreY() - checkSize / 2, checkSize, checkSize);
    }

    void TopBar::mouseDown (const juce::MouseEvent& e)
    {
        // 패치 이름 오른쪽 클릭만: 메뉴 외 다른 동작 없음
        if (e.eventComponent == &patchNameBox && e.mods.isPopupMenu() && ! patchNameBox.isBeingEdited())
            showPatchNameMenu();
    }

    void TopBar::showPatchNameMenu()
    {
        juce::PopupMenu menu;   // 씬 버튼 메뉴와 같은 모양 (머리글 없음)
        menu.addItem ("Rename", [this] { patchNameBox.showEditor(); });
        menu.addItem ("Reset", [this] { resetPatchWithWarning(); });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (patchNameBox.getScreenBounds()));
    }

    // 블록 전체와 씬 8개를 함께 초기화하는 큰 리셋이므로 경고를 띄운다 (기준은 Widgets.h showResetWarning)
    void TopBar::resetPatchWithWarning()
    {
        juce::Component::SafePointer<TopBar> safeThis (this);
        showResetWarning (*this, "Reset patch",
                          "Reset patch " + juce::String (patchBox.getSelectedId()).paddedLeft ('0', 2) + " (\""
                              + patchNameBox.getText() + "\")?\n\nAll blocks are removed, the 8 scenes go back to "
                              "their defaults and the name goes back to the file name.",
                          [safeThis]
                          {
                              if (safeThis != nullptr && safeThis->onPatchReset)
                                  safeThis->onPatchReset();
                          });
    }
}
