#include "RigModel.h"

#include "AppPaths.h"
#include "engine/BlockFactory.h"

namespace rig
{
    RigModel::RigModel (AudioEngine& e) : engine (e)
    {
        for (int i = 0; i < numScenes; ++i)
            sceneNames[(size_t) i] = defaultSceneName (i);

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
    //  "blocks" : 본 줄. 블록 또는 병렬. 기본 패치는 블록 없음
    //    블록   : "type" nam / ir / none (미지정 = 통과, "file" 은 ""), "on" = 씬별 On/Off 8개 (없거나 bool 이 아닌 값은 On)
    //    병렬   : "type" "parallel", "branches" = 위 / 아래 두 갈래. 갈래마다 "on" (씬별 갈래 On/Off, 이상하면 On),
    //             "level" (씬별 dB, 숫자가 아니면 -3, -60 ~ 0 으로 자름), "blocks" (블록만. 갈래 안 병렬은 버린다).
    //             갈래가 두 개가 아니면 병렬을 버리고, 블록이 없는 갈래가 있으면 남은 갈래 블록을 본 줄에 그대로 둔다
    //  "scenes" : 씬 8개의 이름. 이름이 없거나 문자열이 아니거나 비면 "Scene n", 16자까지
    //  "io"     : 장치 채널 번호(0부터). "in" 은 하나, "out" 은 스테레오 쌍 [짝수, 짝수 + 1] 또는 단일 [n].
    //             이상한 값이면 기본값(in 0, out [0, 1]). 장치에 없는 번호여도 그대로 둔다 (소리는 엔진이 기본 채널로)
    //  현재 씬 번호는 저장하지 않는다 (항상 씬 1 로 열린다)
    //
    //  {
    //    "version": 1,
    //    "name": "config01",
    //    "scenes": [ { "name": "Clean" }, { "name": "Scene 2" }, ... 8개 ],
    //    "io": { "in": 0, "out": [ 0, 1 ] },
    //    "blocks": [ { "type": "nam", "file": "/path/drive.nam", "on": [ true, false, true, true, true, true, true, true ] },
    //                { "type": "parallel",
    //                  "branches": [ { "on": [ true, ... 8개 ], "level": [ -3.0, ... 8개 ],
    //                                  "blocks": [ { "type": "nam", "file": "/path/clean.nam", "on": [ ... ] } ] },
    //                                { "on": [ ... ], "level": [ ... ],
    //                                  "blocks": [ { "type": "nam", "file": "/path/lead.nam", "on": [ ... ] } ] } ] },
    //                { "type": "ir", "file": "/path/cab.wav", "on": [ false, true, true, true, true, true, true, true ],
    //                  "params": { "lowCutOn": false, "lowCutHz": 20, ... } } ]
    //  블록의 "params" = 블록 파라미터 (NAM / IR 만). 필드는 각 BlockParams 가 정한다. 없거나 이상한 값은 기본값
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

    juce::String RigModel::readPatchName (int number)
    {
        const auto file = paths::patchFile (juce::jlimit (1, numPatches, number));

        if (! file.existsAsFile())
            return file.getFileName();                      // 예: "config02"

        return sanitizePatchName (juce::JSON::parse (file).getProperty ("name", {}), file.getFileName());
    }

    juce::String RigModel::sanitizeSceneName (const juce::var& value, int scene)
    {
        const auto name = value.isString() ? value.toString().trim().substring (0, maxSceneNameLength).trimEnd()
                                           : juce::String();
        return name.isNotEmpty() ? name : defaultSceneName (scene);
    }

    AudioEngine::Routing RigModel::readRouting (const juce::var& io)
    {
        AudioEngine::Routing r;

        if (const auto in = io.getProperty ("in", {}); in.isInt() && (int) in >= 0)
            r.input = (int) in;

        if (const auto* out = io.getProperty ("out", {}).getArray())
        {
            const auto isChannel = [] (const juce::var& v) { return v.isInt() && (int) v >= 0; };

            if (out->size() == 1 && isChannel ((*out)[0]))
            {
                r.output = (*out)[0];
                r.stereoOutput = false;
            }
            else if (out->size() == 2 && isChannel ((*out)[0]) && (int) (*out)[0] % 2 == 0
                     && isChannel ((*out)[1]) && (int) (*out)[1] == (int) (*out)[0] + 1)
            {
                r.output = (*out)[0];
            }
        }

        return r;
    }

    void RigModel::setPatchName (const juce::String& name)
    {
        const auto trimmedName = name.trim().substring (0, maxPatchNameLength).trimEnd();
        if (trimmedName.isEmpty() || trimmedName == patchName)
            return;

        remember();
        patchName = trimmedName;
        savePatch();
    }

    void RigModel::loadPatch (int number)
    {
        patchNumber = juce::jlimit (1, numPatches, number);
        chain.clear();
        undoStack.clear();   // 되돌리기 이력은 패치마다 (패치를 바꾸면 비운다)
        redoStack.clear();
        levelEditOpen = false;
        selectedSlotId = selectedParallelId = 0;
        selectedEndpoint = Endpoint::none;
        currentScene = 0;
        routing = {};

        const auto file = paths::patchFile (patchNumber);
        patchName = file.getFileName();

        for (int i = 0; i < numScenes; ++i)
            sceneNames[(size_t) i] = defaultSceneName (i);

        if (file.existsAsFile())
        {
            const auto json = juce::JSON::parse (file);

            patchName = sanitizePatchName (json.getProperty ("name", {}), file.getFileName());
            routing = readRouting (json.getProperty ("io", {}));

            const auto* scenes = json.getProperty ("scenes", {}).getArray();

            for (int i = 0; i < numScenes; ++i)
                sceneNames[(size_t) i] = sanitizeSceneName (scenes != nullptr ? (*scenes)[i].getProperty ("name", {}) : juce::var(), i);

            if (auto* blocks = json.getProperty ("blocks", {}).getArray())
            {
                for (const auto& b : *blocks)
                {
                    if (b.getProperty ("type", {}).toString() != "parallel")
                    {
                        chain.emplace_back (readSlot (b, nextId++));
                        continue;
                    }

                    const auto* branches = b.getProperty ("branches", {}).getArray();
                    if (branches == nullptr || branches->size() != 2)
                        continue;   // 갈래가 두 개가 아니면 버린다

                    auto parallel = makeParallel();

                    for (size_t k = 0; k < 2; ++k)
                    {
                        const auto& branch = (*branches)[(int) k];
                        const auto* on = branch.getProperty ("on", {}).getArray();
                        const auto* level = branch.getProperty ("level", {}).getArray();

                        for (int i = 0; i < numScenes; ++i)
                        {
                            parallel.branchOn[k][(size_t) i] = on == nullptr || ! (*on)[i].isBool() || (bool) (*on)[i];

                            const auto v = level != nullptr ? (*level)[i] : juce::var();
                            parallel.levelDb[k][(size_t) i] = (v.isDouble() || v.isInt())
                                                                  ? juce::jlimit (mergeMinDb, mergeMaxDb, (float) v)
                                                                  : mergeDefaultDb;
                        }

                        if (auto* slots = branch.getProperty ("blocks", {}).getArray())
                            for (const auto& s : *slots)
                                if (s.getProperty ("type", {}).toString() != "parallel")   // 갈래 안 병렬은 버린다
                                    parallel.branches[k].push_back (readSlot (s, nextId++));
                    }

                    // 블록이 없는 갈래가 있으면 병렬이 아니다: 남은 갈래 블록만 본 줄에
                    if (parallel.branches[0].empty() || parallel.branches[1].empty())
                    {
                        for (auto& branch : parallel.branches)
                            for (auto& slot : branch)
                                chain.emplace_back (std::move (slot));

                        continue;
                    }

                    chain.emplace_back (std::move (parallel));
                }
            }
        }

        forEachSlot ([this] (Slot& slot) { startLoading (slot); });

        if (! chain.empty())
        {
            const auto& first = chain.front();
            selectedSlotId = std::holds_alternative<Slot> (first) ? std::get<Slot> (first).id
                                                                  : std::get<Parallel> (first).branches[0].front().id;
        }

        // 새 패치의 Merge 는 아직 오디오 스레드가 쓰지 않으므로 바로 맞춘다
        forEachParallel ([this] (Parallel& p) { applyMerge (p, true); });

        // 뮤트 중이면 새 패치의 출력 값은 멤버에만 두고 엔진에는 계속 무음
        engine.setInputGainDb (inputGainDb);
        engine.setOutputGainDb (outputMuted ? AudioEngine::outputMinDb : outputGainDb);

        // 새 패치의 블록이 모두 준비(또는 실패)될 때까지 옛 패치 소리를 유지했다가 한 번에 크로스페이드
        waitingForLoads = true;
        rebuildChain();
        sendChangeMessage();
    }

    void RigModel::resetPatch()
    {
        remember();
        chain.clear();
        selectedSlotId = selectedParallelId = 0;
        selectedEndpoint = Endpoint::none;
        currentScene = 0;
        routing = {};
        patchName = paths::patchFile (patchNumber).getFileName();

        for (int i = 0; i < numScenes; ++i)
            sceneNames[(size_t) i] = defaultSceneName (i);

        waitingForLoads = false;
        changed();   // 저장 + 빈 체인으로 크로스페이드 + UI 알림
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


    RigModel::Slot RigModel::readSlot (const juce::var& json, juce::uint64 id)
    {
        Slot slot;
        slot.id = id;
        slot.type = blockTypeFromString (json.getProperty ("type", {}).toString());
        const auto* on = json.getProperty ("on", {}).getArray();

        for (int i = 0; i < numScenes; ++i)
            slot.on[(size_t) i] = on == nullptr || ! (*on)[i].isBool() || (bool) (*on)[i];

        // 파일이 필요한 종류인데 파일이 없으면 미지정 블록으로 (None / 파일 지정된 NAM·IR 만 존재)
        const auto path = json.getProperty ("file", {}).toString();
        if (usesFile (slot.type) && path.isNotEmpty())
            slot.file = juce::File (path);
        else
            slot.type = BlockType::None;

        slot.params = BlockParams::create (slot.type);
        if (slot.params != nullptr)
            slot.params->fromVar (json.getProperty ("params", {}));

        return slot;
    }

    juce::var RigModel::writeSlot (const Slot& slot)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("type", toString (slot.type));
        obj->setProperty ("file", slot.file.getFullPathName());

        juce::Array<juce::var> on;
        for (const bool b : slot.on)
            on.add (b);

        obj->setProperty ("on", on);

        if (slot.params != nullptr)
            obj->setProperty ("params", slot.params->toVar());

        return juce::var (obj.release());
    }

    juce::var RigModel::writeParallel (const Parallel& parallel)
    {
        juce::Array<juce::var> branches;

        for (size_t k = 0; k < 2; ++k)
        {
            juce::Array<juce::var> on, level, blocks;

            for (int i = 0; i < numScenes; ++i)
            {
                on.add (parallel.branchOn[k][(size_t) i]);
                level.add (std::round ((double) parallel.levelDb[k][(size_t) i] * 10.0) / 10.0);   // 0.1 dB 단위 (float 꼬리 없이)
            }

            for (const auto& slot : parallel.branches[k])
                blocks.add (writeSlot (slot));

            auto branch = std::make_unique<juce::DynamicObject>();
            branch->setProperty ("on", on);
            branch->setProperty ("level", level);
            branch->setProperty ("blocks", blocks);
            branches.add (juce::var (branch.release()));
        }

        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("type", "parallel");
        obj->setProperty ("branches", branches);
        return juce::var (obj.release());
    }

    void RigModel::savePatch() const
    {
        juce::Array<juce::var> blocks;

        for (const auto& item : chain)
        {
            if (const auto* slot = std::get_if<Slot> (&item))
                blocks.add (writeSlot (*slot));
            else
                blocks.add (writeParallel (std::get<Parallel> (item)));
        }

        juce::Array<juce::var> scenes;

        for (const auto& name : sceneNames)
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty ("name", name);
            scenes.add (juce::var (obj.release()));
        }

        auto io = std::make_unique<juce::DynamicObject>();
        io->setProperty ("in", routing.input);

        juce::Array<juce::var> out { routing.output };
        if (routing.stereoOutput)
            out.add (routing.output + 1);

        io->setProperty ("out", out);

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty ("version", 1);
        root->setProperty ("name", patchName);
        root->setProperty ("scenes", scenes);
        root->setProperty ("io", juce::var (io.release()));
        root->setProperty ("blocks", blocks);

        // 소수 자릿수 0 = 필요한 만큼 (기본 15 자리면 -23.3 이 -23.300000000000001 로 써진다)
        paths::patchFile (patchNumber).replaceWithText (juce::JSON::toString (juce::var (root.release()), false, 0));
    }

    std::shared_ptr<BlockParams> RigModel::getSelectedParams() const
    {
        const auto* slot = getSelectedSlot();
        return slot != nullptr ? slot->params : nullptr;
    }

    void RigModel::saveParams()
    {
        savePatch();
    }


    void RigModel::setInputChannel (int channel)
    {
        if (channel < 0 || channel == routing.input)
            return;

        remember();
        routing.input = channel;
        changed();   // 저장 + 엔진이 옛/새 채널을 크로스페이드 + UI 알림
    }

    void RigModel::setOutputChannel (int firstChannel, bool stereo)
    {
        if (firstChannel < 0 || (firstChannel == routing.output && stereo == routing.stereoOutput))
            return;

        remember();
        routing.output = firstChannel;
        routing.stereoOutput = stereo;
        changed();
    }

    bool RigModel::isChannelMissing (Endpoint endpoint, const juce::AudioIODevice* device) const
    {
        const int numInputs  = device != nullptr ? device->getActiveInputChannels().countNumberOfSetBits() : 0;
        const int numOutputs = device != nullptr ? device->getActiveOutputChannels().countNumberOfSetBits() : 0;

        if (endpoint == Endpoint::input)
            return routing.input >= numInputs;

        if (endpoint == Endpoint::output)
            return routing.output + (routing.stereoOutput ? 2 : 1) > numOutputs;

        return false;
    }

    juce::BigInteger RigModel::getUsedInputChannels (Endpoint except) const
    {
        juce::BigInteger used;

        if (except != Endpoint::input)
            used.setBit (routing.input);

        return used;
    }

    juce::BigInteger RigModel::getUsedOutputChannels (Endpoint except) const
    {
        juce::BigInteger used;

        if (except != Endpoint::output)
            used.setRange (routing.output, routing.stereoOutput ? 2 : 1, true);

        return used;
    }


    //==============================================================================
    template <typename Fn>
    void RigModel::forEachSlot (Fn&& fn)
    {
        for (auto& item : chain)
        {
            if (auto* slot = std::get_if<Slot> (&item))
                fn (*slot);
            else
                for (auto& branch : std::get<Parallel> (item).branches)
                    for (auto& s : branch)
                        fn (s);
        }
    }

    template <typename Fn>
    void RigModel::forEachParallel (Fn&& fn)
    {
        for (auto& item : chain)
            if (auto* p = std::get_if<Parallel> (&item))
                fn (*p);
    }

    RigModel::Slot* RigModel::editSlot (juce::uint64 id)
    {
        Slot* found = nullptr;

        if (id != 0)
            forEachSlot ([&] (Slot& s) { if (s.id == id) found = &s; });

        return found;
    }

    const RigModel::Slot* RigModel::findSlot (juce::uint64 id) const
    {
        return const_cast<RigModel*> (this)->editSlot (id);
    }

    RigModel::Parallel* RigModel::editParallel (juce::uint64 id)
    {
        Parallel* found = nullptr;

        if (id != 0)
            forEachParallel ([&] (Parallel& p) { if (p.id == id) found = &p; });

        return found;
    }

    const RigModel::Parallel* RigModel::findParallel (juce::uint64 id) const
    {
        return const_cast<RigModel*> (this)->editParallel (id);
    }

    std::optional<RigModel::Place> RigModel::findPlace (juce::uint64 slotId) const
    {
        for (int i = 0; i < (int) chain.size(); ++i)
        {
            if (const auto* slot = std::get_if<Slot> (&chain[(size_t) i]))
            {
                if (slot->id == slotId)
                    return Place { i, -1, 0 };

                continue;
            }

            const auto& branches = std::get<Parallel> (chain[(size_t) i]).branches;

            for (int b = 0; b < 2; ++b)
                for (int k = 0; k < (int) branches[(size_t) b].size(); ++k)
                    if (branches[(size_t) b][(size_t) k].id == slotId)
                        return Place { i, b, k };
        }

        return std::nullopt;
    }

    RigModel::Slot RigModel::makeSlot (BlockType type, const juce::File& file)
    {
        Slot slot;
        slot.id = nextId++;
        slot.type = usesFile (type) && file != juce::File() ? type : BlockType::None;
        slot.file = slot.type == BlockType::None ? juce::File() : file;
        slot.params = BlockParams::create (slot.type);   // 기본값
        return slot;
    }

    RigModel::Parallel RigModel::makeParallel()
    {
        Parallel p;
        p.id = nextId++;

        for (size_t i = 0; i < (size_t) numScenes; ++i)
            resetSceneValues (p, i);

        p.merge = std::make_shared<Merge>();
        return p;
    }

    bool RigModel::isValidInsertPlace (const Place& where) const
    {
        if (where.branch < 0)
            return where.item >= 0 && where.item <= (int) chain.size();

        if (! juce::isPositiveAndBelow (where.item, (int) chain.size()) || where.branch > 1)
            return false;

        const auto* p = std::get_if<Parallel> (&chain[(size_t) where.item]);
        return p != nullptr && where.index >= 0 && where.index <= (int) p->branches[(size_t) where.branch].size();
    }

    void RigModel::insertSlot (Place where, Slot slot)
    {
        if (where.branch < 0)
        {
            chain.emplace (chain.begin() + where.item, std::move (slot));
            return;
        }

        auto& branch = std::get<Parallel> (chain[(size_t) where.item]).branches[(size_t) where.branch];
        branch.insert (branch.begin() + where.index, std::move (slot));
    }

    void RigModel::eraseSlot (juce::uint64 slotId)
    {
        const auto place = findPlace (slotId);
        if (! place.has_value())
            return;

        if (place->branch < 0)
        {
            chain.erase (chain.begin() + place->item);
            return;
        }

        auto& parallel = std::get<Parallel> (chain[(size_t) place->item]);
        auto& branch = parallel.branches[(size_t) place->branch];
        branch.erase (branch.begin() + place->index);

        if (! branch.empty())
            return;

        // 갈래가 비면 병렬 해제: 남은 갈래의 블록을 병렬 자리에 본 줄로. 자리가 바뀌므로 새 인스턴스로 다시 로딩
        auto rest = std::move (parallel.branches[(size_t) (1 - place->branch)]);

        if (selectedParallelId == parallel.id)
            selectedParallelId = 0;

        chain.erase (chain.begin() + place->item);

        for (size_t i = 0; i < rest.size(); ++i)
            chain.emplace (chain.begin() + place->item + (int) i, std::move (rest[i]));

        for (size_t i = 0; i < rest.size(); ++i)
            startLoading (std::get<Slot> (chain[(size_t) place->item + i]));
    }

    //==============================================================================
    // 되돌리기 / 다시 하기. 스냅샷의 블록은 DSP 인스턴스(shared_ptr)를 함께 들고 있어서 되돌려도 다시 로딩하지 않는다.
    // 자리가 바뀌는 편집은 늘 새 인스턴스를 쓰므로, 되돌린 체인의 인스턴스가 지금 소리와 다른 자리에서 겹치는 일은 없다
    RigModel::Snapshot RigModel::snapshot (int scene) const
    {
        return { chain, routing, sceneNames, patchName, selectedSlotId, selectedParallelId, selectedEndpoint, scene };
    }

    void RigModel::remember()
    {
        undoStack.push_back (snapshot (currentScene));

        if ((int) undoStack.size() > maxUndoSteps)
            undoStack.erase (undoStack.begin());

        redoStack.clear();
    }

    void RigModel::undo() { step (undoStack, redoStack); }
    void RigModel::redo() { step (redoStack, undoStack); }

    // from 의 마지막 상태로 돌아가고, 지금 상태는 to 에 쌓는다. 그 편집을 한 씬으로 함께 바꾼다
    void RigModel::step (std::vector<Snapshot>& from, std::vector<Snapshot>& to)
    {
        if (from.empty())
            return;

        auto target = std::move (from.back());
        from.pop_back();
        to.push_back (snapshot (target.scene));
        levelEditOpen = false;

        chain = std::move (target.chain);
        routing = target.routing;
        sceneNames = target.sceneNames;
        patchName = target.patchName;
        currentScene = target.scene;
        selectedSlotId = findSlot (target.selectedSlotId) != nullptr ? target.selectedSlotId : 0;
        selectedParallelId = findParallel (target.selectedParallelId) != nullptr ? target.selectedParallelId : 0;
        selectedEndpoint = target.selectedEndpoint;

        // 스냅샷을 찍을 때 로딩 중이던 블록은 그 결과가 이미 다른 상태로 갔을 수 있으므로 다시 로딩
        forEachSlot ([this] (Slot& slot)
        {
            if (slot.loading)
                startLoading (slot);
        });

        applyCurrentScene();
        changed();   // 저장 + 바뀐 구간 크로스페이드 + UI 알림
    }

    //==============================================================================
    void RigModel::setSelectedSlot (juce::uint64 id)
    {
        selectedSlotId = findSlot (id) != nullptr ? id : 0;
        selectedParallelId = 0;
        selectedEndpoint = Endpoint::none;
    }

    void RigModel::selectSlot (juce::uint64 id)
    {
        setSelectedSlot (id);
        sendChangeMessage();
    }

    void RigModel::selectParallel (juce::uint64 id)
    {
        selectedParallelId = findParallel (id) != nullptr ? id : 0;
        selectedSlotId = 0;
        selectedEndpoint = Endpoint::none;
        sendChangeMessage();
    }

    void RigModel::selectEndpoint (Endpoint endpoint)
    {
        selectedEndpoint = endpoint;
        selectedSlotId = selectedParallelId = 0;
        sendChangeMessage();
    }

    void RigModel::insertBlock (Place where, BlockType type, const juce::File& file)
    {
        if (! isValidInsertPlace (where))
            return;

        remember();
        auto slot = makeSlot (type, file);
        const auto id = slot.id;
        insertSlot (where, std::move (slot));
        startLoading (*editSlot (id));
        setSelectedSlot (id);
        changed();
    }

    bool RigModel::canWrapInParallel (juce::uint64 belowSlotId) const
    {
        // 병렬은 한 단계만: 본 줄의 블록 아래에만 만든다
        const auto place = findPlace (belowSlotId);
        return place.has_value() && place->branch < 0;
    }

    bool RigModel::wrapInParallel (juce::uint64 belowSlotId, Slot lower)
    {
        if (! canWrapInParallel (belowSlotId))
            return false;

        const auto place = findPlace (belowSlotId);

        auto parallel = makeParallel();
        parallel.branches[0].push_back (std::move (std::get<Slot> (chain[(size_t) place->item])));
        parallel.branches[1].push_back (std::move (lower));
        applyMerge (parallel, true);   // 아직 오디오 스레드가 쓰지 않는다
        chain[(size_t) place->item] = std::move (parallel);

        // 두 갈래 블록 모두 자리가 바뀌었으므로 새 인스턴스로 (이미 로딩 중이면 그 결과가 새 인스턴스)
        for (auto& branch : std::get<Parallel> (chain[(size_t) place->item]).branches)
            if (! branch.front().loading)
                startLoading (branch.front());

        return true;
    }

    void RigModel::addParallel (juce::uint64 belowSlotId, BlockType type, const juce::File& file)
    {
        if (! canWrapInParallel (belowSlotId))
            return;

        remember();
        auto slot = makeSlot (type, file);
        const auto id = slot.id;

        if (! wrapInParallel (belowSlotId, std::move (slot)))
            return;

        setSelectedSlot (id);
        changed();
    }

    void RigModel::moveBlockBelow (juce::uint64 slotId, juce::uint64 belowSlotId)
    {
        if (slotId == belowSlotId || findSlot (slotId) == nullptr || ! canWrapInParallel (belowSlotId))
            return;

        remember();

        // moveBlock 과 같이 먼저 새 자리(병렬 아래 갈래)에 넣고 나중에 옛 자리를 지운다. 새 자리의 블록은 새 id, 새 인스턴스
        auto moved = *findSlot (slotId);
        moved.id = nextId++;
        moved.loading = false;   // 옛 id 로 진행 중이던 로딩 결과는 버려진다
        const auto movedId = moved.id;

        if (! wrapInParallel (belowSlotId, std::move (moved)))
            return;

        eraseSlot (slotId);
        setSelectedSlot (movedId);
        changed();
    }

    void RigModel::moveBlock (juce::uint64 slotId, Place where)
    {
        const auto from = findPlace (slotId);
        if (! from.has_value() || ! isValidInsertPlace (where))
            return;

        // 같은 목록에서 바로 앞이나 바로 뒤 자리 = 제자리 (아무 일도 없다)
        const bool sameList = from->branch < 0 ? where.branch < 0
                                               : where.branch == from->branch && where.item == from->item;
        const int position = from->branch < 0 ? from->item : from->index;
        const int target = where.branch < 0 ? where.item : where.index;

        if (sameList && (target == position || target == position + 1))
            return;

        // 먼저 새 자리에 넣고 나중에 옛 자리를 지운다 (지우면서 병렬이 풀려도 넣을 자리가 사라지지 않게).
        // 새 자리의 블록은 새 id, 새 인스턴스
        auto moved = *findSlot (slotId);
        moved.id = nextId++;
        moved.loading = false;   // 옛 id 로 진행 중이던 로딩 결과는 버려진다
        const auto movedId = moved.id;

        insertSlot (where, std::move (moved));
        eraseSlot (slotId);

        if (auto* slot = editSlot (movedId); ! slot->loading)   // 병렬 해제로 이미 다시 로딩 중일 수 있다
            startLoading (*slot);

        setSelectedSlot (movedId);
        changed();
    }

    void RigModel::moveMerge (juce::uint64 parallelId, int delta)
    {
        int item = -1;

        for (int i = 0; i < (int) chain.size(); ++i)
            if (const auto* p = std::get_if<Parallel> (&chain[(size_t) i]); p != nullptr && p->id == parallelId)
                item = i;

        if (item < 0 || delta == 0)
            return;

        remember();   // 옮길 블록이 하나도 없으면 아래에서 되돌린다
        std::vector<juce::uint64> moved;

        // 넣기: 병렬 바로 뒤 본 줄 블록부터 (병렬을 만나면 멈춘다)
        for (; delta > 0 && item + 1 < (int) chain.size() && std::holds_alternative<Slot> (chain[(size_t) item + 1]); --delta)
        {
            auto slot = std::move (std::get<Slot> (chain[(size_t) item + 1]));
            chain.erase (chain.begin() + item + 1);
            moved.push_back (slot.id);
            std::get<Parallel> (chain[(size_t) item]).branches[0].push_back (std::move (slot));
        }

        // 빼기: 위 갈래 끝 블록부터 (위 갈래에 1개는 남긴다). 뺀 순서대로 병렬 바로 뒤에 넣으면 원래 순서가 유지된다
        // (chain 에 넣으면 벡터가 다시 잡힐 수 있으므로 위 갈래는 매번 다시 찾는다)
        for (; delta < 0 && std::get<Parallel> (chain[(size_t) item]).branches[0].size() > 1; ++delta)
        {
            auto& upper = std::get<Parallel> (chain[(size_t) item]).branches[0];
            auto slot = std::move (upper.back());
            upper.pop_back();
            moved.push_back (slot.id);
            chain.emplace (chain.begin() + item + 1, std::move (slot));
        }

        if (moved.empty())
        {
            undoStack.pop_back();
            return;
        }

        // 합류 앞뒤로 옮겨진 블록은 처리 자리가 바뀌었으므로 새 인스턴스로 (이미 로딩 중이면 그 결과가 새 인스턴스)
        for (const auto id : moved)
            if (auto* slot = editSlot (id); ! slot->loading)
                startLoading (*slot);

        changed();
    }

    void RigModel::removeBlock (juce::uint64 slotId)
    {
        const auto place = findPlace (slotId);
        if (! place.has_value())
            return;

        remember();

        // 지운 뒤 선택: 신호 흐름상 바로 앞 블록 (갈래 첫 블록이면 병렬 앞), 앞에 블록이 없으면 IN
        juce::uint64 previous = 0;

        if (place->branch >= 0 && place->index > 0)
        {
            previous = std::get<Parallel> (chain[(size_t) place->item]).branches[(size_t) place->branch][(size_t) place->index - 1].id;
        }
        else if (place->item > 0)
        {
            const auto& before = chain[(size_t) place->item - 1];
            previous = std::holds_alternative<Slot> (before) ? std::get<Slot> (before).id
                                                              : std::get<Parallel> (before).branches[0].back().id;
        }

        eraseSlot (slotId);

        setSelectedSlot (previous);

        if (previous == 0)
            selectedEndpoint = Endpoint::input;
        changed();
    }

    void RigModel::setOn (juce::uint64 slotId, bool shouldBeOn)
    {
        auto* slot = editSlot (slotId);
        if (slot == nullptr)
            return;

        remember();

        // 프로그램은 그대로 두고 블록의 wet/dry 목표만 바꾼다 (오디오 스레드가 크로스페이드)
        slot->on[(size_t) currentScene] = shouldBeOn;

        if (slot->dsp != nullptr)
            slot->dsp->setOn (shouldBeOn);

        savePatch();
        sendChangeMessage();
    }

    //==============================================================================
    void RigModel::setBranchOn (juce::uint64 parallelId, int branch, bool shouldBeOn)
    {
        auto* p = editParallel (parallelId);
        if (p == nullptr || ! juce::isPositiveAndBelow (branch, 2))
            return;

        remember();
        p->branchOn[(size_t) branch][(size_t) currentScene] = shouldBeOn;
        mergeEdited (*p);
    }

    void RigModel::setBranchLevel (juce::uint64 parallelId, int branch, float db, bool save)
    {
        auto* p = editParallel (parallelId);
        if (p == nullptr || ! juce::isPositiveAndBelow (branch, 2))
            return;

        // 드래그 한 번 = 되돌리기 한 단계: 드래그 중 첫 값에서 기억하고 끝(save)에서 닫는다
        if (! levelEditOpen)
            remember();

        levelEditOpen = ! save;
        p->levelDb[(size_t) branch][(size_t) currentScene] = juce::jlimit (mergeMinDb, mergeMaxDb, db);

        if (save)
        {
            mergeEdited (*p);
            return;
        }

        applyMerge (*p);   // 드래그 중: 소리만 따라가고 저장·알림은 드래그 끝에
    }

    void RigModel::resetLevels (juce::uint64 parallelId)
    {
        auto* p = editParallel (parallelId);
        if (p == nullptr)
            return;

        remember();
        resetLevelValues (*p, (size_t) currentScene);
        mergeEdited (*p);
    }

    void RigModel::applyMerge (Parallel& p, bool immediately)
    {
        // 갈래 Off = 게인 0, 레벨은 mergeMinDb 이하가 -inf
        const auto gain = [&] (int b)
        {
            return isBranchOn (p, b) ? juce::Decibels::decibelsToGain (getBranchLevel (p, b), mergeMinDb) : 0.0f;
        };

        p.merge->setGains (gain (0), gain (1), immediately);
    }

    void RigModel::mergeEdited (Parallel& p)
    {
        // 프로그램은 그대로 두고 Merge 의 게인 목표만 바꾼다 (오디오 스레드가 따라간다)
        applyMerge (p);
        savePatch();
        sendChangeMessage();
    }

    //==============================================================================
    void RigModel::selectScene (int scene)
    {
        if (! juce::isPositiveAndBelow (scene, numScenes))
            return;

        currentScene = scene;
        applyCurrentScene();
        sendChangeMessage();
    }

    void RigModel::copyScene (int source, int destination)
    {
        if (! juce::isPositiveAndBelow (source, numScenes) || ! juce::isPositiveAndBelow (destination, numScenes)
            || source == destination)
            return;

        remember();
        const auto s = (size_t) source, d = (size_t) destination;
        sceneNames[d] = sceneNames[s];

        forEachSlot ([&] (Slot& slot) { copySceneValues (slot, s, d); });
        forEachParallel ([&] (Parallel& p) { copySceneValues (p, s, d); });

        sceneEdited (destination);
    }

    void RigModel::swapScene (int source, int destination)
    {
        if (! juce::isPositiveAndBelow (source, numScenes) || ! juce::isPositiveAndBelow (destination, numScenes)
            || source == destination)
            return;

        remember();
        const auto s = (size_t) source, d = (size_t) destination;
        std::swap (sceneNames[s], sceneNames[d]);

        forEachSlot ([&] (Slot& slot) { swapSceneValues (slot, s, d); });
        forEachParallel ([&] (Parallel& p) { swapSceneValues (p, s, d); });

        sceneEdited (source == currentScene ? source : destination);
    }

    void RigModel::resetScene (int scene)
    {
        if (! juce::isPositiveAndBelow (scene, numScenes))
            return;

        // 블록 모두 On, 병렬 갈래 모두 On + 기본 레벨, 기본 이름
        remember();
        const auto i = (size_t) scene;
        sceneNames[i] = defaultSceneName (scene);

        forEachSlot ([&] (Slot& slot) { resetSceneValues (slot, i); });
        forEachParallel ([&] (Parallel& p) { resetSceneValues (p, i); });

        sceneEdited (scene);
    }

    void RigModel::resetAllScenes()
    {
        remember();

        for (size_t i = 0; i < (size_t) numScenes; ++i)
        {
            sceneNames[i] = defaultSceneName ((int) i);
            forEachSlot ([&] (Slot& slot) { resetSceneValues (slot, i); });
            forEachParallel ([&] (Parallel& p) { resetSceneValues (p, i); });
        }

        sceneEdited (currentScene);
    }

    void RigModel::renameScene (int scene, const juce::String& name)
    {
        if (! juce::isPositiveAndBelow (scene, numScenes) || sanitizeSceneName (name, scene) == sceneNames[(size_t) scene])
            return;

        remember();
        sceneNames[(size_t) scene] = sanitizeSceneName (name, scene);
        savePatch();
        sendChangeMessage();
    }

    void RigModel::resetSceneValues (Slot& slot, size_t scene)
    {
        slot.on[scene] = true;
    }

    void RigModel::resetSceneValues (Parallel& p, size_t scene)
    {
        for (auto& on : p.branchOn)
            on[scene] = true;

        resetLevelValues (p, scene);
    }

    void RigModel::resetLevelValues (Parallel& p, size_t scene)
    {
        for (auto& level : p.levelDb)
            level[scene] = mergeDefaultDb;
    }

    void RigModel::copySceneValues (Slot& slot, size_t source, size_t destination)
    {
        slot.on[destination] = slot.on[source];
    }

    void RigModel::copySceneValues (Parallel& p, size_t source, size_t destination)
    {
        for (size_t b = 0; b < 2; ++b)
        {
            p.branchOn[b][destination] = p.branchOn[b][source];
            p.levelDb[b][destination] = p.levelDb[b][source];
        }
    }

    void RigModel::swapSceneValues (Slot& slot, size_t a, size_t b)
    {
        std::swap (slot.on[a], slot.on[b]);
    }

    void RigModel::swapSceneValues (Parallel& p, size_t a, size_t b)
    {
        for (size_t k = 0; k < 2; ++k)
        {
            std::swap (p.branchOn[k][a], p.branchOn[k][b]);
            std::swap (p.levelDb[k][a], p.levelDb[k][b]);
        }
    }

    void RigModel::sceneEdited (int scene)
    {
        if (scene == currentScene)
            applyCurrentScene();

        savePatch();
        sendChangeMessage();
    }

    void RigModel::applyCurrentScene()
    {
        forEachSlot ([this] (Slot& slot)
        {
            if (slot.dsp != nullptr)
                slot.dsp->setOn (isOn (slot));
        });

        forEachParallel ([this] (Parallel& p) { applyMerge (p); });
    }

    void RigModel::assignBlock (juce::uint64 slotId, BlockType type, const juce::File& file)
    {
        auto* slot = editSlot (slotId);
        if (slot == nullptr)
            return;

        const auto newFile = usesFile (type) ? file : juce::File();

        if (usesFile (type) && newFile == juce::File())
            return;

        // 같은 지정이면 무시. 단 로딩 실패한 파일은 다시 시도한다
        if (slot->type == type && slot->file == newFile
            && (type == BlockType::None || slot->dsp != nullptr || slot->loading))
            return;

        remember();

        // 교체 = 같은 종류여도 기본값. 같은 파일을 다시 시도할 때만 (로딩 실패 후) 값을 둔다
        // 옛 DSP 는 옛 객체를 들고 있어서 크로스페이드가 끝날 때까지 옛 값으로 소리를 낸다
        if (slot->type != type || slot->file != newFile)
            slot->params = BlockParams::create (type);

        slot->type = type;
        slot->file = newFile;
        startLoading (*slot);
        changed();
    }

    void RigModel::deviceChanged()
    {
        const auto sr = engine.getSampleRate();
        const auto bs = engine.getMaxBlockSize();

        // 엔진이 처리 중인 블록은 엔진이 이미 다시 준비했으므로, 나머지(아직 엔진에 넘기지 않은 블록)만 처리
        forEachSlot ([&] (Slot& slot)
        {
            if (slot.dsp != nullptr && ! engine.isInUse (slot.dsp.get())
                && slot.dsp->needsPrepare (sr, bs))
                slot.dsp->prepare (sr, bs);
        });

        sendChangeMessage();
    }

    //==============================================================================
    void RigModel::startLoading (Slot& slot)
    {
        ++slot.loadGeneration;
        slot.error.clear();
        slot.loading = false;

        if (! usesFile (slot.type) || slot.file == juce::File())
        {
            slot.dsp.reset();
            return;
        }

        // slot.dsp(옛 DSP)는 새 DSP 가 준비될 때까지 그대로 소리를 낸다 (onLoaded 에서 크로스페이드로 교체).
        // 로딩 중에는 체인 전체를 옛 소리로 둔다 (자리가 바뀐 블록의 옛 인스턴스를 새 자리에서 돌릴 수 없으므로)
        slot.loading = true;
        waitingForLoads = true;


        const auto slotId = slot.id;
        const auto generation = slot.loadGeneration;
        const auto type = slot.type;
        const auto file = slot.file;
        const auto sr = engine.getSampleRate();
        const auto bs = engine.getMaxBlockSize();
        juce::WeakReference<RigModel> weakThis (this);

        // 블록의 파라미터 객체를 값으로 넘긴다 (로더 스레드에서 슬롯을 읽지 않는다). 새 DSP 도 같은 객체를 읽는다
        const auto paramsSnapshot = slot.params;

        // 무거운 작업(파일 파싱, 모델 생성, Reset/prewarm)은 로더 스레드에서
        loader.addJob ([weakThis, slotId, generation, type, file, sr, bs, paramsSnapshot]
        {
            juce::String error;
            auto block = loadBlock (type, file, error, paramsSnapshot);

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
        auto* slot = editSlot (slotId);

        if (slot == nullptr || slot->loadGeneration != generation)
            return; // 그 사이 슬롯이 지워졌거나 다른 파일로 바뀜

        slot->loading = false;
        slot->dsp = block;
        slot->error = error;

        // 아직 오디오 스레드가 쓰지 않는 새 블록이므로 On/Off 를 바로 맞춘다
        if (block != nullptr)
            block->setOn (isOn (*slot), true);

        rebuildChain();
        sendChangeMessage();
    }

    void RigModel::rebuildChain()
    {
        if (waitingForLoads)
        {
            bool loading = false;
            forEachSlot ([&] (const Slot& s) { loading = loading || s.loading; });

            if (loading)
                return;

            waitingForLoads = false;
        }

        // 엔진이 지금 소리와 비교해 바뀐 구간만 크로스페이드한다 (장치 설정에 맞춘 준비도 엔진에서).
        // 꺼진 블록도 계속 계산하므로 프로그램에 넣는다 (On/Off 는 블록의 wet/dry). DSP 가 없는 블록은 통과
        const auto addSlot = [] (const Slot& slot, AudioEngine::Program& program)
        {
            if (slot.dsp != nullptr)
                program.push_back ({ slot.dsp, nullptr, {} });
        };

        AudioEngine::Program program;

        for (const auto& item : chain)
        {
            if (const auto* slot = std::get_if<Slot> (&item))
            {
                addSlot (*slot, program);
                continue;
            }

            const auto& parallel = std::get<Parallel> (item);
            AudioEngine::Node node { nullptr, parallel.merge, {} };

            for (size_t b = 0; b < 2; ++b)
                for (const auto& slot : parallel.branches[b])
                    addSlot (slot, node.branches[b]);

            program.push_back (std::move (node));
        }

        engine.setProgram (std::move (program), routing);
    }

    void RigModel::changed()
    {
        savePatch();
        rebuildChain();
        sendChangeMessage();
    }
}
