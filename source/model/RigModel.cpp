#include "RigModel.h"

#include "AppPaths.h"
#include "engine/IrBlock.h"
#include "engine/NamBlock.h"

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
    //  "type": "none" 은 미지정 블록 (통과), "file" 은 "". 기본 패치는 블록 없음
    //  "scenes" : 씬 8개의 이름. 이름이 없거나 문자열이 아니거나 비면 "Scene n", 16자까지
    //  "on"     : 블록의 씬별 On/Off 8개. 없거나 bool 이 아닌 값은 On
    //  현재 씬 번호는 저장하지 않는다 (항상 씬 1 로 열린다)
    //
    //  {
    //    "version": 1,
    //    "name": "config01",
    //    "scenes": [ { "name": "Clean" }, { "name": "Scene 2" }, ... 8개 ],
    //    "blocks": [ { "type": "nam",  "file": "/path/amp.nam", "on": [ true, false, true, true, true, true, true, true ] },
    //                { "type": "none", "file": "",              "on": [ true, true, true, true, true, true, true, true ] },
    //                { "type": "ir",   "file": "/path/cab.wav", "on": [ false, true, true, true, true, true, true, true ] } ]
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

    juce::String RigModel::sanitizeSceneName (const juce::var& value, int scene)
    {
        const auto name = value.isString() ? value.toString().trim().substring (0, maxSceneNameLength).trimEnd()
                                           : juce::String();
        return name.isNotEmpty() ? name : defaultSceneName (scene);
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
        currentScene = 0;

        const auto file = paths::patchFile (patchNumber);
        patchName = file.getFileName();

        for (int i = 0; i < numScenes; ++i)
            sceneNames[(size_t) i] = defaultSceneName (i);

        if (file.existsAsFile())
        {
            const auto json = juce::JSON::parse (file);

            patchName = sanitizePatchName (json.getProperty ("name", {}), file.getFileName());

            const auto* scenes = json.getProperty ("scenes", {}).getArray();

            for (int i = 0; i < numScenes; ++i)
                sceneNames[(size_t) i] = sanitizeSceneName (scenes != nullptr ? (*scenes)[i].getProperty ("name", {}) : juce::var(), i);

            if (auto* blocks = json.getProperty ("blocks", {}).getArray())
            {
                for (const auto& b : *blocks)
                {
                    Slot slot;
                    slot.id = nextSlotId++;
                    slot.type = blockTypeFromString (b.getProperty ("type", {}).toString());
                    const auto* on = b.getProperty ("on", {}).getArray();

                    for (int i = 0; i < numScenes; ++i)
                        slot.on[(size_t) i] = on == nullptr || ! (*on)[i].isBool() || (bool) (*on)[i];

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

        // 새 패치의 블록이 모두 준비(또는 실패)될 때까지 옛 패치 소리를 유지했다가 한 번에 크로스페이드
        waitingForPatch = true;
        rebuildChain();
        sendChangeMessage();
    }

    void RigModel::resetPatch()
    {
        slots.clear();
        selectedIndex = -1;
        currentScene = 0;
        patchName = paths::patchFile (patchNumber).getFileName();

        for (int i = 0; i < numScenes; ++i)
            sceneNames[(size_t) i] = defaultSceneName (i);

        waitingForPatch = false;
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

    void RigModel::savePatch() const
    {
        juce::Array<juce::var> blocks;

        for (const auto& slot : slots)
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty ("type", toString (slot.type));
            obj->setProperty ("file", slot.file.getFullPathName());

            juce::Array<juce::var> on;
            for (const bool b : slot.on)
                on.add (b);

            obj->setProperty ("on", on);
            blocks.add (juce::var (obj.release()));
        }

        juce::Array<juce::var> scenes;

        for (const auto& name : sceneNames)
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty ("name", name);
            scenes.add (juce::var (obj.release()));
        }

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty ("version", 1);
        root->setProperty ("name", patchName);
        root->setProperty ("scenes", scenes);
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

        // 같은 인스턴스를 바뀐 순서로 동시에 처리할 수 없으므로, 두 블록을 새로 로딩해 크로스페이드한다.
        // 로딩되는 동안은 옛 순서 그대로 소리가 난다
        startLoading (slots[(size_t) index]);
        startLoading (slots[(size_t) target]);
        changed();
    }

    void RigModel::setOn (int index, bool shouldBeOn)
    {
        if (! juce::isPositiveAndBelow (index, (int) slots.size()))
            return;

        // 프로그램은 그대로 두고 블록의 wet/dry 목표만 바꾼다 (오디오 스레드가 크로스페이드)
        auto& slot = slots[(size_t) index];
        slot.on[(size_t) currentScene] = shouldBeOn;

        if (slot.dsp != nullptr)
            slot.dsp->setOn (shouldBeOn);

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

        sceneNames[(size_t) destination] = sceneNames[(size_t) source];

        for (auto& slot : slots)
            slot.on[(size_t) destination] = slot.on[(size_t) source];

        sceneEdited (destination);
    }

    void RigModel::swapScene (int source, int destination)
    {
        if (! juce::isPositiveAndBelow (source, numScenes) || ! juce::isPositiveAndBelow (destination, numScenes)
            || source == destination)
            return;

        std::swap (sceneNames[(size_t) source], sceneNames[(size_t) destination]);

        for (auto& slot : slots)
            std::swap (slot.on[(size_t) source], slot.on[(size_t) destination]);

        sceneEdited (source == currentScene ? source : destination);
    }

    void RigModel::resetScene (int scene)
    {
        if (! juce::isPositiveAndBelow (scene, numScenes))
            return;

        sceneNames[(size_t) scene] = defaultSceneName (scene);

        for (auto& slot : slots)
            slot.on[(size_t) scene] = true;

        sceneEdited (scene);
    }

    void RigModel::resetAllScenes()
    {
        for (int i = 0; i < numScenes; ++i)
            sceneNames[(size_t) i] = defaultSceneName (i);

        for (auto& slot : slots)
            slot.on = allScenesOn();

        sceneEdited (currentScene);
    }

        void RigModel::renameScene (int scene, const juce::String& name)
    {
        if (! juce::isPositiveAndBelow (scene, numScenes))
            return;

        sceneNames[(size_t) scene] = sanitizeSceneName (name, scene);
        savePatch();
        sendChangeMessage();
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
        for (auto& slot : slots)
            if (slot.dsp != nullptr)
                slot.dsp->setOn (isOn (slot));
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

        // 엔진이 처리 중인 블록은 엔진이 이미 다시 준비했으므로, 나머지(아직 엔진에 넘기지 않은 블록)만 처리
        for (auto& slot : slots)
            if (slot.dsp != nullptr && ! engine.isInUse (slot.dsp.get())
                && slot.dsp->needsPrepare (sr, bs))
                slot.dsp->prepare (sr, bs);

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

        // slot.dsp(옛 DSP)는 새 DSP 가 준비될 때까지 그대로 소리를 낸다 (onLoaded 에서 크로스페이드로 교체)

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

        // 아직 오디오 스레드가 쓰지 않는 새 블록이므로 On/Off 를 바로 맞춘다
        if (block != nullptr)
            block->setOn (isOn (*it), true);

        rebuildChain();
        sendChangeMessage();
    }

    void RigModel::rebuildChain()
    {
        if (waitingForPatch)
        {
            if (std::any_of (slots.begin(), slots.end(), [] (const Slot& s) { return s.loading; }))
                return;

            waitingForPatch = false;
        }

        // 엔진이 지금 소리와 비교해 바뀐 구간만 크로스페이드한다 (장치 설정에 맞춘 준비도 엔진에서).
        // 꺼진 블록도 계속 계산하므로 프로그램에 넣는다 (On/Off 는 블록의 wet/dry)
        AudioEngine::Program program;

        for (auto& slot : slots)
            if (slot.dsp != nullptr)
                program.push_back (slot.dsp);

        engine.setProgram (std::move (program));
    }

    void RigModel::changed()
    {
        savePatch();
        rebuildChain();
        sendChangeMessage();
    }
}
