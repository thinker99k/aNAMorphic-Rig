#include "BlockEditor.h"

#include "AppPaths.h"
#include "engine/IrBlock.h"

namespace rig
{
    namespace
    {
        juce::String patternFor (BlockType t) { return t == BlockType::Nam ? "*.nam" : "*.wav"; }
        juce::File folderFor (BlockType t)    { return t == BlockType::Nam ? paths::namDir() : paths::irDir(); }
    }

    BlockEditor::BlockEditor (RigModel& m, AudioEngine& e) : model (m), engine (e)
    {
        title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        folderHint.setColour (juce::Label::textColourId, juce::Colours::grey);
        status.setJustificationType (juce::Justification::topLeft);

        fileList.setModel (this);
        fileList.setRowHeight (24);
        fileList.setOutlineThickness (1);
        const int columnFlags = juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable;
        fileList.getHeader().addColumn ("File", 1, 360, 120, -1, columnFlags);
        fileList.getHeader().addColumn ("Gear Type", 3, 180, 100, -1, columnFlags);
        fileList.getHeader().addColumn ("Path", 2, 420, 160, -1, columnFlags);
        fileList.getHeader().setStretchToFitActive (true);

        for (auto* c : std::initializer_list<juce::Component*> { &title, &fileList, &folderHint, &status })
            addAndMakeVisible (c);

        model.addChangeListener (this);
        refresh();
    }

    BlockEditor::~BlockEditor()
    {
        model.removeChangeListener (this);
        fileList.setModel (nullptr);
    }

    void BlockEditor::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        refresh();
    }

    void BlockEditor::refresh()
    {
        const juce::ScopedValueSetter<bool> updating (refreshing, true);
        const auto* slot = model.getSelectedSlot();

        const bool hasSlot = slot != nullptr;
        fileList.setVisible (hasSlot);
        folderHint.setVisible (hasSlot);
        status.setVisible (hasSlot);

        if (! hasSlot)
        {
            title.setText ("No block selected. Click a block in the chain, or add one with \"+ Add block\".",
                           juce::dontSendNotification);
            return;
        }

        const bool isNam = slot->type == BlockType::Nam;
        fileList.getHeader().setColumnName (1, isNam ? "Name" : "Filename");
        fileList.getHeader().setColumnVisible (3, isNam);

        title.setText ("Block #" + juce::String (model.getSelectedIndex() + 1) + "  -  " + displayName (slot->type),
                       juce::dontSendNotification);

        // 파일 목록: 폴더 스캔 결과 + (목록에 없으면) 현재 파일
        const auto folder = folderFor (slot->type);
        folderHint.setText ("Files are listed from " + folder.getFullPathName(), juce::dontSendNotification);

        fileForItem.clearQuick();
        fileNames.clear();
        displayNames.clear();
        gearTypes.clear();

        auto found = folder.findChildFiles (juce::File::findFiles, true, patternFor (slot->type));
        found.sort();

        for (const auto& f : found)
        {
            fileForItem.add (f);
            fileNames.add (f.getRelativePathFrom (folder));
        }

        if (slot->file != juce::File() && ! fileForItem.contains (slot->file))
        {
            fileForItem.add (slot->file);
            fileNames.add (slot->file.getFullPathName());
        }

        for (const auto& file : fileForItem)
        {
            NamMetadata metadata;
            if (isNam)
            {
                auto& cached = metadataCache[file.getFullPathName()];
                const auto modified = file.getLastModificationTime();
                const auto size = file.getSize();
                if (cached.size != size || cached.modified != modified)
                {
                    juce::String error;
                    cached.metadata = readNamMetadata (file, error).value_or (NamMetadata {});
                    cached.modified = modified;
                    cached.size = size;
                }
                metadata = cached.metadata;
            }

            displayNames.add (metadata.name.isNotEmpty() ? metadata.name : file.getFileName());
            gearTypes.add (metadata.gearType);
        }

        fileList.updateContent();
        fileList.deselectAllRows();
        const int current = fileForItem.indexOf (slot->file);
        if (current >= 0)
            fileList.selectRow (current);
        fileList.repaint();

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

    int BlockEditor::getNumRows()
    {
        return fileForItem.size();
    }

    void BlockEditor::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
    {
        if (selected)
            g.fillAll (findColour (juce::TextEditor::highlightColourId));
        else if (row % 2 != 0)
            g.fillAll (juce::Colours::white.withAlpha (0.04f));

        g.setColour (juce::Colours::grey.withAlpha (0.25f));
        g.drawHorizontalLine (height - 1, 0.0f, (float) width);
    }

    void BlockEditor::paintCell (juce::Graphics& g, int row, int columnId, int width, int height, bool selected)
    {
        if (! juce::isPositiveAndBelow (row, fileForItem.size()))
            return;

        g.setColour (selected ? findColour (juce::TextEditor::highlightedTextColourId)
                              : findColour (juce::ListBox::textColourId));
        g.setFont (14.0f);
        const auto text = columnId == 1 ? displayNames[row]
                        : columnId == 3 ? gearTypes[row] : fileNames[row];
        g.drawText (text, 6, 0, juce::jmax (0, width - 12), height,
                    juce::Justification::centredLeft, true);
        g.setColour (juce::Colours::grey.withAlpha (0.25f));
        g.drawVerticalLine (width - 1, 0.0f, (float) height);
    }

    void BlockEditor::selectedRowsChanged (int row)
    {
        if (refreshing || ! juce::isPositiveAndBelow (row, fileForItem.size()))
            return;

        const auto* slot = model.getSelectedSlot();
        if (slot != nullptr && slot->file != fileForItem[row])
            model.setFile (model.getSelectedIndex(), fileForItem[row]);
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

        status.setBounds (area.removeFromBottom (60));
        folderHint.setBounds (area.removeFromBottom (20));
        area.removeFromBottom (4);
        fileList.setBounds (area);
    }
}
