#include "RigModel.h"

#include "AppPaths.h"
#include "engine/IrBlock.h"
#include "engine/NamBlock.h"

namespace rig
{
    RigModel::RigModel (AudioEngine& e) : engine (e)
    {
        loadSettings();
    }

    RigModel::~RigModel()
    {
        saveSettings();   // 종료할 때 항상 저장 (드래그 끝 저장과 별개)
        loader.removeAllJobs (true, 10000);
    }

    //==============================================================================
    // 앱 전역 설정 : ~/Documents/aNAMorphic-Rig/settings.json  (JSON)
    //
    //  { "version": 1, "inputGainDb": 0.0, "outputGainDb": -3.5 }

    void RigModel::loadSettings()
    {
        const auto file = paths::settingsFile();

        if (file.existsAsFile())
        {
            const auto json = juce::JSON::parse (file);

            inputGainDb  = juce::jlimit (AudioEngine::inputMinDb, AudioEngine::inputMaxDb,
                                         (float) json.getProperty ("inputGainDb", 0.0));
            outputGainDb = juce::jlimit (AudioEngine::outputMinDb, AudioEngine::outputMaxDb,
                                         (float) json.getProperty ("outputGainDb", 0.0));
        }

        engine.setInputGainDb (inputGainDb);
        engine.setOutputGainDb (outputGainDb);
    }

    void RigModel::saveSettings() const
    {
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty ("version", 1);
        root->setProperty ("inputGainDb", inputGainDb);
        root->setProperty ("outputGainDb", outputGainDb);

        paths::settingsFile().replaceWithText (juce::JSON::toString (juce::var (root.release())));
    }

    //==============================================================================
    // 패치 저장/불러오기 : ~/Documents/aNAMorphic-Rig/patch/config<NN>  (JSON)
    //  "type": "none" 은 미지정 블록 (통과), "file" 은 "". "on" 이 없거나 bool 이 아니면 On. 기본 패치는 블록 없음
    //
    //  {
    //    "version": 1,
    //    "name": "config01",
    //    "blocks": [ { "type": "nam",  "file": "/path/amp.nam", "on": true },
    //                { "type": "none", "file": "",              "on": true },
    //                { "type": "ir",   "file": "/path/cab.wav", "on": false } ]
    //  }

    juce::String RigModel::sanitizePatchName (const juce::var& value, const juce::String& fallback)
    {
        if (! value.isString())
            return fallback;

        const auto name = value.toString().trim();
        if (name.isEmpty())
            return fallback;

        return name.substring (0, maxPatchNameLength).trimEnd();
    }

    void RigModel::setPatchName (const juce::String& name)
    {
        const auto trimmedName = name.trim().substring (0, maxPatchNameLength).trimEnd();
        if (trimmedName.isEmpty() || trimmedName == patchName)
            return;

        patchName = trimmedName;
        savePatch();
    }

    void RigModel::loadPatch (int number)
    {
        patchNumber = juce::jlimit (1, numPatches, number);
        slots.clear();
        selectedIndex = -1;

        const auto file = paths::patchFile (patchNumber);
        patchName = file.getFileName();

        if (file.existsAsFile())
        {
            const auto json = juce::JSON::parse (file);

            patchName = sanitizePatchName (json.getProperty ("name", {}), file.getFileName());

            if (auto* blocks = json.getProperty ("blocks", {}).getArray())
            {
                for (const auto& b : *blocks)
                {
                    Slot slot;
                    slot.id = nextSlotId++;
                    slot.type = blockTypeFromString (b.getProperty ("type", {}).toString());
                    const auto on = b.getProperty ("on", {});
                    slot.on = on.isBool() ? (bool) on : true;

                    // 파일이 필요한 종류인데 파일이 없으면 미지정 블록으로 (None / 파일 지정된 NAM·IR 만 존재)
                    const auto path = b.getProperty ("file", {}).toString();
                    if (usesFile (slot.type) && path.isNotEmpty())
                        slot.file = juce::File (path);
                    else
                        slot.type = BlockType::None;

                    slots.push_back (std::move (slot));
                }
            }
        }

        for (auto& slot : slots)
            startLoading (slot);

        if (! slots.empty())
            selectedIndex = 0;

        // 뮤트 중이면 새 패치의 출력 값은 멤버에만 두고 엔진에는 계속 무음
        engine.setInputGainDb (inputGainDb);
        engine.setOutputGainDb (outputMuted ? AudioEngine::outputMinDb : outputGainDb);

        rebuildChain();
        sendChangeMessage();
    }

    void RigModel::setInputGainDb (float db)
    {
        inputGainDb = juce::jlimit (AudioEngine::inputMinDb, AudioEngine::inputMaxDb, db);
        engine.setInputGainDb (inputGainDb);
    }

    void RigModel::setOutputGainDb (float db)
    {
        outputGainDb = juce::jlimit (AudioEngine::outputMinDb, AudioEngine::outputMaxDb, db);
        outputMuted = false;
        engine.setOutputGainDb (outputGainDb);
    }

    void RigModel::saveGains()
    {
        saveSettings();
    }

    void RigModel::setOutputMuted (bool shouldBeMuted)
    {
        // 저장하지 않는다: settings.json 에는 뮤트 전 원래 값(outputGainDb)이 남는다
        outputMuted = shouldBeMuted;
        engine.setOutputGainDb (outputMuted ? AudioEngine::outputMinDb : outputGainDb);
    }

    void RigModel::savePatch() const
    {
        juce::Array<juce::var> blocks;

        for (const auto& slot : slots)
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty ("type", toString (slot.type));
            obj->setProperty ("file", slot.file.getFullPathName());
            obj->setProperty ("on", slot.on);
            blocks.add (juce::var (obj.release()));
        }

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty ("version", 1);
        root->setProperty ("name", patchName);
        root->setProperty ("blocks", blocks);

        paths::patchFile (patchNumber).replaceWithText (juce::JSON::toString (juce::var (root.release())));
    }

    //==============================================================================
    const RigModel::Slot* RigModel::getSelectedSlot() const noexcept
    {
        return juce::isPositiveAndBelow (selectedIndex, (int) slots.size()) ? &slots[(size_t) selectedIndex] : nullptr;
    }

    void RigModel::select (int index)
    {
        selectedIndex = juce::isPositiveAndBelow (index, (int) slots.size()) ? index : -1;
        sendChangeMessage();
    }

    void RigModel::insertBlock (int index)
    {
        index = juce::jlimit (0, (int) slots.size(), index);

        Slot slot;
        slot.id = nextSlotId++;
        slots.insert (slots.begin() + index, std::move (slot));
        selectedIndex = index;
        changed();
    }

    void RigModel::removeBlock (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) slots.size()))
            return;

        slots.erase (slots.begin() + index);
        selectedIndex = juce::jmin (selectedIndex, (int) slots.size() - 1);
        changed();
    }

    void RigModel::moveBlock (int index, int delta)
    {
        const int target = index + delta;

        if (! juce::isPositiveAndBelow (index, (int) slots.size())
            || ! juce::isPositiveAndBelow (target, (int) slots.size()))
            return;

        std::swap (slots[(size_t) index], slots[(size_t) target]);
        selectedIndex = target;
        changed();
    }

    void RigModel::setOn (int index, bool shouldBeOn)
    {
        if (! juce::isPositiveAndBelow (index, (int) slots.size()))
            return;

        slots[(size_t) index].on = shouldBeOn;
        changed();
    }

    void RigModel::assignBlock (int index, BlockType type, const juce::File& file)
    {
        if (! juce::isPositiveAndBelow (index, (int) slots.size()))
            return;

        const auto newFile = usesFile (type) ? file : juce::File();

        if (usesFile (type) && newFile == juce::File())
            return;

        auto& slot = slots[(size_t) index];

        // 같은 지정이면 무시. 단 로딩 실패한 파일은 다시 시도한다
        if (slot.type == type && slot.file == newFile
            && (type == BlockType::None || slot.dsp != nullptr || slot.loading))
            return;

        slot.type = type;
        slot.file = newFile;
        startLoading (slot);
        changed();
    }

    void RigModel::deviceChanged()
    {
        const auto sr = engine.getSampleRate();
        const auto bs = engine.getMaxBlockSize();

        // 체인에 들어 있는 블록(On)은 엔진이 이미 다시 준비했으므로, 꺼진 블록만 처리
        for (auto& slot : slots)
            if (slot.dsp != nullptr && ! slot.on
                && slot.dsp->needsPrepare (sr, bs))
                slot.dsp->prepare (sr, bs);

        sendChangeMessage();
    }

    //==============================================================================
    void RigModel::startLoading (Slot& slot)
    {
        ++slot.loadGeneration;
        slot.dsp.reset();
        slot.error.clear();
        slot.loading = false;

        if (! usesFile (slot.type) || slot.file == juce::File())
            return;

        slot.loading = true;

        const auto slotId = slot.id;
        const auto generation = slot.loadGeneration;
        const auto type = slot.type;
        const auto file = slot.file;
        const auto sr = engine.getSampleRate();
        const auto bs = engine.getMaxBlockSize();
        juce::WeakReference<RigModel> weakThis (this);

        // 무거운 작업(파일 파싱, 모델 생성, Reset/prewarm)은 로더 스레드에서
        loader.addJob ([weakThis, slotId, generation, type, file, sr, bs]
        {
            juce::String error;
            std::shared_ptr<Block> block;

            if (type == BlockType::Nam)
                block = NamBlock::load (file, error);
            else
                block = IrBlock::load (file, error);

            if (block != nullptr)
                block->prepare (sr, bs);

            // 결과는 메시지 스레드로 넘겨서 반영
            juce::MessageManager::callAsync ([weakThis, slotId, generation, block, error]
            {
                if (auto* self = weakThis.get())
                    self->onLoaded (slotId, generation, block, error);
            });
        });
    }

    void RigModel::onLoaded (juce::uint64 slotId, int generation, std::shared_ptr<Block> block, juce::String error)
    {
        auto it = std::find_if (slots.begin(), slots.end(), [&] (const Slot& s) { return s.id == slotId; });

        if (it == slots.end() || it->loadGeneration != generation)
            return; // 그 사이 슬롯이 지워졌거나 다른 파일로 바뀜

        it->loading = false;
        it->dsp = block;
        it->error = error;

        rebuildChain();
        sendChangeMessage();
    }

    void RigModel::rebuildChain()
    {
        const auto sr = engine.getSampleRate();
        const auto bs = engine.getMaxBlockSize();

        auto chain = std::make_unique<AudioEngine::Chain>();

        for (auto& slot : slots)
        {
            if (! slot.on || slot.dsp == nullptr)
                continue;

            // 로딩 중에 장치 설정이 바뀌었다면, 체인에 넣기 전에 다시 준비
            if (slot.dsp->needsPrepare (sr, bs))
                slot.dsp->prepare (sr, bs);

            chain->blocks.push_back (slot.dsp);
        }

        engine.setChain (std::move (chain));
    }

    void RigModel::changed()
    {
        savePatch();
        rebuildChain();
        sendChangeMessage();
    }
}
