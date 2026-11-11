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
        updateFromModel();
    }

    BlockEditor::Entry BlockEditor::makeEntry (const juce::File& file, const juce::String& path, bool readMetadata)
    {
        NamMetadata metadata;

        if (readMetadata)
        {
            juce::String error;
            metadata = readNamMetadata (file, error).value_or (NamMetadata {});
        }

        return { file, path, metadata.name.isNotEmpty() ? metadata.name : file.getFileName(), metadata.gearType };
    }

    // 추후 설정창 refresh library 버튼에서 호출 예정
    // nam/, ir/ 폴더를 스캔하고 NAM 메타데이터를 읽어 캐시에 둔다 (디스크를 읽는 곳은 여기뿐)
    void BlockEditor::refresh()
    {
        for (const auto type : { BlockType::Nam, BlockType::Ir })
        {
            const auto folder = folderFor (type);
            auto& entries = type == BlockType::Nam ? namEntries : irEntries;
            entries.clear();

            auto found = folder.findChildFiles (juce::File::findFiles, true, patternFor (type));
            found.sort();

            for (const auto& f : found)
                entries.push_back (makeEntry (f, f.getRelativePathFrom (folder), type == BlockType::Nam));
        }

        outsideEntry = {};
        shownSlotId = 0;   // 목록이 바뀌었으니 강조도 다시 맞춘다
        updateFromModel();
    }

    // 모델 변경 알림마다 호출: 캐시에서 선택된 블록 종류의 목록을 고른다 (폴더 스캔 없음)
    void BlockEditor::updateFromModel()
    {
        const auto* slot = model.getSelectedSlot();

        const bool hasSlot = slot != nullptr;
        fileList.setVisible (hasSlot);
        folderHint.setVisible (hasSlot);
        status.setVisible (hasSlot);

        if (! hasSlot)
        {
            title.setText ("No block selected. Click a block in the chain, or add one with \"+ Add block\".",
                           juce::dontSendNotification);
            shownSlotId = 0;
            return;
        }

        const bool isNam = slot->type == BlockType::Nam;
        fileList.getHeader().setColumnName (1, isNam ? "Name" : "Filename");
        fileList.getHeader().setColumnVisible (3, isNam);

        title.setText ("Block #" + juce::String (model.getSelectedIndex() + 1) + "  -  " + displayName (slot->type),
                       juce::dontSendNotification);

        // 파일 목록: 폴더 스캔 결과 + (목록에 없으면) 현재 파일
        folderHint.setText ("Files are listed from " + folderFor (slot->type).getFullPathName(), juce::dontSendNotification);

        rows = isNam ? namEntries : irEntries;

        const auto findRow = [this] (const juce::File& f)
        {
            for (size_t i = 0; i < rows.size(); ++i)
                if (rows[i].file == f)
                    return (int) i;

            return -1;
        };

        if (slot->file != juce::File() && findRow (slot->file) < 0)
        {
            if (outsideEntry.file != slot->file)
                outsideEntry = makeEntry (slot->file, slot->file.getFullPathName(), isNam);

            rows.push_back (outsideEntry);
        }

        fileList.updateContent();

        if (slot->id != shownSlotId || slot->file != shownFile)
        {
            shownSlotId = slot->id;
            shownFile = slot->file;

            fileList.deselectAllRows();
            const int current = findRow (slot->file);
            if (current >= 0)
                fileList.selectRow (current);
        }

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
        return (int) rows.size();
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
        if (! juce::isPositiveAndBelow (row, (int) rows.size()))
            return;

        g.setColour (selected ? findColour (juce::TextEditor::highlightedTextColourId)
                              : findColour (juce::ListBox::textColourId));
        g.setFont (14.0f);
        const auto& entry = rows[(size_t) row];
        const auto text = columnId == 1 ? entry.displayName
                        : columnId == 3 ? entry.gearType : entry.path;
        g.drawText (text, 6, 0, juce::jmax (0, width - 12), height,
                    juce::Justification::centredLeft, true);
        g.setColour (juce::Colours::grey.withAlpha (0.25f));
        g.drawVerticalLine (width - 1, 0.0f, (float) height);
    }

    void BlockEditor::cellDoubleClicked (int row, int, const juce::MouseEvent&)
    {
        if (model.getSelectedSlot() == nullptr || ! juce::isPositiveAndBelow (row, (int) rows.size()))
            return;

        const auto file = rows[(size_t) row].file;

        // 목록은 다시 스캔하지 않으므로, 그 사이 지워졌거나 옮겨진 파일일 수 있다
        if (! file.existsAsFile())
        {
            showFileNotFound (file);
            return;
        }

        // 같은 파일이어도 막지 않는다 (로딩 실패 후 다시 시도). 중복 로딩은 RigModel::setFile 이 막는다
        model.setFile (model.getSelectedIndex(), file);
    }

    void BlockEditor::showFileNotFound (const juce::File& file)
    {
        // 설정창처럼 버튼으로만 닫히게: ESC 로 닫히지 않고, 닫기(X) 버튼도 없다
        auto* alert = new juce::AlertWindow ("File not found",
                                             file.getFullPathName()
                                                 + "\n\nThe file may have been moved or deleted. "
                                                   "It will disappear from the list after the library is refreshed.",
                                             juce::MessageBoxIconType::WarningIcon,
                                             this);
        alert->addButton ("OK", 1);
        alert->setEscapeKeyCancels (false);
        alert->enterModalState (true, nullptr, true);
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
