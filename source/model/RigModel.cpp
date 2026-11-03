#include "RigModel.h"

#include "AppPaths.h"
#include "engine/IrBlock.h"
#include "engine/NamBlock.h"

namespace rig
{
    RigModel::RigModel (AudioEngine& e) : engine (e) {}

    RigModel::~RigModel()
    {
        loader.removeAllJobs (true, 10000);
    }

    //==============================================================================
    // 패치 저장/불러오기 : ~/aNAMorphic-Rig/config<N>  (JSON)
    //
    //  {
    //    "version": 1,
    //    "inputGainDb": 0.0,
    //    "outputGainDb": -3.5,
    //    "blocks": [ { "type": "nam", "file": "/path/amp.nam", "bypass": false },
    //                { "type": "ir",  "file": "/path/cab.wav", "bypass": false } ]
    //  }

    void RigModel::loadPatch (int number)
    {
        patchNumber = juce::jlimit (1, numPatches, number);
        slots.clear();
        selectedIndex = -1;

        // config 가 없는 패치로 옮겨도 이전 패치의 볼륨이 남지 않게 먼저 0 dB 로
        inputGainDb = 0.0f;
        outputGainDb = 0.0f;

        const auto file = paths::patchFile (patchNumber);

        if (file.existsAsFile())
        {
            const auto json = juce::JSON::parse (file);

            // 이전 범위(-80 ~ +6 dB)로 저장된 값도 새 범위의 가장 가까운 끝값으로 맞춘다
            inputGainDb  = juce::jlimit (AudioEngine::inputMinDb, AudioEngine::inputMaxDb,
                                         (float) json.getProperty ("inputGainDb", 0.0));
            outputGainDb = juce::jlimit (AudioEngine::outputMinDb, AudioEngine::outputMaxDb,
                                         (float) json.getProperty ("outputGainDb", 0.0));

            if (auto* blocks = json.getProperty ("blocks", {}).getArray())
            {
                for (const auto& b : *blocks)
                {
                    Slot slot;
                    slot.id = nextSlotId++;
                    slot.type = blockTypeFromString (b.getProperty ("type", "nam").toString());
                    slot.bypassed = (bool) b.getProperty ("bypass", false);

                    const auto path = b.getProperty ("file", {}).toString();
                    if (path.isNotEmpty())
                        slot.file = juce::File (path);

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
        savePatch();
    }

    void RigModel::setOutputGainDb (float db)
    {
        outputGainDb = juce::jlimit (AudioEngine::outputMinDb, AudioEngine::outputMaxDb, db);
        outputMuted = false;
        engine.setOutputGainDb (outputGainDb);
        savePatch();
    }

    void RigModel::setOutputMuted (bool shouldBeMuted)
    {
        // 저장하지 않는다: config 에는 뮤트 전 원래 값(outputGainDb)이 남는다
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
            obj->setProperty ("bypass", slot.bypassed);
            blocks.add (juce::var (obj.release()));
        }

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty ("version", 1);
        root->setProperty ("inputGainDb", inputGainDb);
        root->setProperty ("outputGainDb", outputGainDb);
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

    void RigModel::addBlock (BlockType type)
    {
        Slot slot;
        slot.id = nextSlotId++;
        slot.type = type;
        slots.push_back (std::move (slot));
        selectedIndex = (int) slots.size() - 1;
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

    void RigModel::setBypassed (int index, bool shouldBeBypassed)
    {
        if (! juce::isPositiveAndBelow (index, (int) slots.size()))
            return;

        slots[(size_t) index].bypassed = shouldBeBypassed;
        changed();
    }

    void RigModel::setFile (int index, const juce::File& file)
    {
        if (! juce::isPositiveAndBelow (index, (int) slots.size()))
            return;

        auto& slot = slots[(size_t) index];

        if (slot.file == file && (slot.dsp != nullptr || slot.loading))
            return;

        slot.file = file;
        startLoading (slot);
        changed();
    }

    void RigModel::deviceChanged()
    {
        const auto sr = engine.getSampleRate();
        const auto bs = engine.getMaxBlockSize();

        // 체인에 들어 있는 블록은 엔진이 이미 다시 준비했으므로, 나머지(바이패스 등)만 처리
        for (auto& slot : slots)
            if (slot.dsp != nullptr && slot.bypassed
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

        if (slot.file == juce::File())
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
            if (slot.bypassed || slot.dsp == nullptr)
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
