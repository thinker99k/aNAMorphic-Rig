#include "BlockEditor.h"

#include "AppPaths.h"
#include "engine/IrBlock.h"

namespace rig
{
    namespace
    {
        constexpr int browseId = 100000;

        juce::String patternFor (BlockType t) { return t == BlockType::Nam ? "*.nam" : "*.wav"; }
        juce::File folderFor (BlockType t)    { return t == BlockType::Nam ? paths::capturesDir() : paths::irsDir(); }
    }

    BlockEditor::BlockEditor (RigModel& m, AudioEngine& e) : model (m), engine (e)
    {
        title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        folderHint.setColour (juce::Label::textColourId, juce::Colours::grey);
        status.setJustificationType (juce::Justification::topLeft);

        fileBox.onChange = [this] { fileChosen(); };

        for (auto* c : std::initializer_list<juce::Component*> { &title, &fileLabel, &fileBox, &folderHint, &status })
            addAndMakeVisible (c);

        model.addChangeListener (this);
        refresh();
    }

    BlockEditor::~BlockEditor()
    {
        model.removeChangeListener (this);
    }

    void BlockEditor::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        refresh();
    }

    void BlockEditor::refresh()
    {
        const auto* slot = model.getSelectedSlot();

        const bool hasSlot = slot != nullptr;
        fileLabel.setVisible (hasSlot);
        fileBox.setVisible (hasSlot);
        folderHint.setVisible (hasSlot);
        status.setVisible (hasSlot);

        if (! hasSlot)
        {
            title.setText ("No block selected. Click a block in the chain, or add one with \"+ Add block\".",
                           juce::dontSendNotification);
            return;
        }

        title.setText ("Block #" + juce::String (model.getSelectedIndex() + 1) + "  -  " + displayName (slot->type),
                       juce::dontSendNotification);

        // 콤보박스 목록: 폴더 스캔 결과 + (목록에 없으면) 현재 파일 + Browse...
        const auto folder = folderFor (slot->type);
        folderHint.setText ("Files are listed from " + folder.getFullPathName(), juce::dontSendNotification);

        fileForItem.clearQuick();
        fileBox.clear (juce::dontSendNotification);

        auto found = folder.findChildFiles (juce::File::findFiles, true, patternFor (slot->type));
        found.sort();

        for (const auto& f : found)
        {
            fileForItem.add (f);
            fileBox.addItem (f.getRelativePathFrom (folder), fileForItem.size());
        }

        if (slot->file != juce::File() && ! fileForItem.contains (slot->file))
        {
            fileForItem.add (slot->file);
            fileBox.addItem (slot->file.getFullPathName(), fileForItem.size());
        }

        fileBox.addSeparator();
        fileBox.addItem ("Browse...", browseId);

        const int current = fileForItem.indexOf (slot->file);
        if (current >= 0)
            fileBox.setSelectedId (current + 1, juce::dontSendNotification);
        else
            fileBox.setTextWhenNothingSelected ("(choose a " + patternFor (slot->type).fromFirstOccurrenceOf (".", true, false) + " file)");

        // 상태 표시
        juce::String text;

        if (slot->file == juce::File())
            text = "No file assigned. The block passes audio through unchanged.";
        else if (slot->loading)
            text = "Loading...";
        else if (slot->error.isNotEmpty())
            text = "Error: " + slot->error;
        else if (slot->dsp != nullptr)
        {
            text = "Loaded.";

            const auto deviceRate = engine.getSampleRate();
            const auto wanted = slot->dsp->expectedSampleRate();

            if (wanted > 0.0 && std::abs (wanted - deviceRate) > 1.0)
                text << "\nWarning: this model was trained at " << juce::String (wanted, 0)
                     << " Hz but the device runs at " << juce::String (deviceRate, 0)
                     << " Hz. Resampling is not implemented yet, so the tone will be off.";

            if (auto* ir = dynamic_cast<IrBlock*> (slot->dsp.get()))
                text << "\nIR: " << juce::String (ir->irLengthSeconds() * 1000.0, 1) << " ms @ "
                     << juce::String (ir->irSampleRate(), 0) << " Hz";
        }

        if (slot->bypassed)
            text << "\n(bypassed)";

        status.setText (text, juce::dontSendNotification);
        status.setColour (juce::Label::textColourId,
                          slot->error.isNotEmpty() ? juce::Colours::red.brighter() : juce::Colours::lightgrey);
    }

    void BlockEditor::fileChosen()
    {
        const int id = fileBox.getSelectedId();

        if (id == browseId)
        {
            browse();
            return;
        }

        if (juce::isPositiveAndBelow (id - 1, fileForItem.size()))
            model.setFile (model.getSelectedIndex(), fileForItem[id - 1]);
    }

    void BlockEditor::browse()
    {
        const auto* slot = model.getSelectedSlot();
        if (slot == nullptr)
            return;

        const int index = model.getSelectedIndex();
        const auto start = slot->file.existsAsFile() ? slot->file : folderFor (slot->type);

        chooser = std::make_unique<juce::FileChooser> ("Choose " + displayName (slot->type), start, patternFor (slot->type));

        juce::Component::SafePointer<BlockEditor> safeThis (this);

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis, index] (const juce::FileChooser& fc)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  const auto result = fc.getResult();

                                  if (result.existsAsFile())
                                      safeThis->model.setFile (index, result);
                                  else
                                      safeThis->refresh();   // 취소: 콤보 선택을 원래대로
                              });
    }

    void BlockEditor::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    }

    void BlockEditor::resized()
    {
        auto area = getLocalBounds().reduced (12, 10);

        title.setBounds (area.removeFromTop (24));
        area.removeFromTop (6);

        auto row = area.removeFromTop (28);
        fileLabel.setBounds (row.removeFromLeft (40));
        fileBox.setBounds (row.removeFromLeft (juce::jmin (520, row.getWidth())));

        folderHint.setBounds (area.removeFromTop (20).withTrimmedLeft (40));
        area.removeFromTop (4);
        status.setBounds (area.withTrimmedLeft (40));
    }
}
