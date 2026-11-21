#pragma once

#include "engine/AudioEngine.h"
#include "engine/Block.h"

#include <juce_events/juce_events.h>

#include <array>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace rig
{
    // 체인 편집 상태 (메시지 스레드 전용).
    //
    // 체인 = IN → 직렬/병렬 트리 → OUT. 본 줄은 블록과 병렬이 섞인 목록, 병렬은 위/아래 두 갈래(각각 블록 1개 이상)와
    // 합류(Merge). 병렬은 한 단계만 (갈래 안에 병렬 없음), 갈래는 반드시 다시 합쳐진다.
    //
    // 씬: 패치마다 8개. 씬은 블록별 On/Off, 병렬 갈래별 On/Off·레벨, 이름을 가진다.
    //  - 현재 씬 번호는 저장하지 않는다 (패치는 항상 씬 1 로 열린다). 씬 전환은 디스크에 쓰지 않는다
    //  - 씬 편집(On/Off, 복사/교환/초기화/이름)은 패치에 저장한다
    //
    // UI 는 RigModel 만 보고 편집하고, RigModel 은 변경될 때마다
    //  1) 현재 패치를 ~/Documents/aNAMorphic-Rig/patch/config<NN> 에 자동 저장하고
    //  2) 오디오 엔진에 새 Chain 을 넘기고
    //  3) ChangeBroadcaster 로 UI 에 알린다.
    class RigModel final : public juce::ChangeBroadcaster
    {
    public:
        static constexpr int numScenes = 8;
        static constexpr int maxSceneNameLength = 16;   // 글자(코드 포인트) 수

        static constexpr std::array<bool, numScenes> allScenesOn()
        {
            std::array<bool, numScenes> on {};
            on.fill (true);
            return on;
        }

        struct Slot
        {
            juce::uint64 id = 0;                 // 슬롯 고유 번호 (비동기 로딩 결과를 찾을 때 사용)
            BlockType type = BlockType::None;
            juce::File file;                     // None 이면 비어 있고, NAM / IR 이면 항상 지정되어 있다
            std::array<bool, numScenes> on = allScenesOn();   // 씬별 On/Off: 블록을 옮기거나 종류가 바뀌어도 블록에 남는다

            std::shared_ptr<Block> dsp;          // 소리를 내는 DSP (없으면 통과). 새 파일을 로딩하는 동안은 옛 DSP 를 그대로 둔다
            bool loading = false;
            juce::String error;
            int loadGeneration = 0;              // 늦게 도착한 옛 로딩 결과를 버리기 위함
        };

        // 병렬 갈래 레벨 (dB, 씬별). mergeMinDb 이하 = -inf
        static constexpr float mergeMinDb = -60.0f;
        static constexpr float mergeMaxDb = 0.0f;
        static constexpr float mergeDefaultDb = -3.0f;   // 병렬을 만들 때, Level Reset, 씬 Reset

        // 병렬 하나: 위(0) / 아래(1) 두 갈래와 합류. 갈래 Off = 그 갈래 무음 (블록 Off 의 바이패스와 다르다)
        struct Parallel
        {
            juce::uint64 id = 0;                                    // 슬롯과 같은 번호 공간
            std::array<std::vector<Slot>, 2> branches;              // 둘 다 비지 않는다
            std::array<std::array<bool, numScenes>, 2> branchOn;    // 씬별 갈래 On/Off
            std::array<std::array<float, numScenes>, 2> levelDb;    // 씬별 갈래 레벨
            std::shared_ptr<Merge> merge;                           // 두 갈래를 합치는 DSP
        };

        using Item = std::variant<Slot, Parallel>;   // 본 줄의 한 칸

        // 트리 안의 자리. branch < 0 이면 본 줄의 item 번째, 아니면 item 번째 병렬의 branch 갈래 안 index 번째.
        // 넣을 자리로 쓸 때는 그 번째 앞 (item / index 가 목록 크기면 맨 뒤)
        struct Place
        {
            int item = 0;
            int branch = -1;
            int index = 0;
        };

        static constexpr int numPatches = 32;
        static constexpr int maxPatchNameLength = 32;   // 글자(코드 포인트) 수

        // 패치 파일의 "name" 값 검증: 문자열이 아니거나 공백뿐이면 fallback, 길면 앞 32자만
        static juce::String sanitizePatchName (const juce::var& value, const juce::String& fallback);

        // 패치를 불러오지 않고 파일의 이름만 읽는다 (loadPatch 와 같은 규칙: 파일이 없거나 이름이 이상하면 파일명)
        static juce::String readPatchName (int number);

        explicit RigModel (AudioEngine& engine);
        ~RigModel() override;

        // 패치
        int getPatchNumber() const noexcept { return patchNumber; }
        const juce::String& getPatchName() const noexcept { return patchName; }
        void setPatchName (const juce::String& name);
        void loadPatch (int number);
        void resetPatch();   // 지금 패치를 기본 패치로 (블록 없음, 기본 씬, 이름 = 파일명). 번호 유지, 저장, 크로스페이드

        // 입력/출력 볼륨 (dB, 앱 전역, settings.json 에 저장). UI 재구성을 막기 위해 변경 알림은 보내지 않는다
        float getInputGainDb() const noexcept  { return inputGainDb; }
        float getOutputGainDb() const noexcept { return outputGainDb; }
        void setInputGainDb (float db);
        void setOutputGainDb (float db);   // 뮤트도 함께 해제한다
        void saveGains();                   // 노브 드래그가 끝났을 때 settings.json 에 저장

        // 출력 뮤트: settings.json 에 저장하지 않고 패치를 바꿔도 유지된다. 뮤트 중에도 outputGainDb 는 원래 값
        bool isOutputMuted() const noexcept { return outputMuted; }
        void setOutputMuted (bool shouldBeMuted);

        // 체인 편집 (블록 개수 제한 없음: CPU 가 버티는 만큼). 블록과 병렬은 id 로 가리킨다
        const std::vector<Item>& getChain() const noexcept { return chain; }
        const Slot* findSlot (juce::uint64 id) const;
        const Parallel* findParallel (juce::uint64 id) const;
        std::optional<Place> findPlace (juce::uint64 slotId) const;   // 블록의 자리

        // 선택은 블록 하나, 병렬(합류) 하나, IN / OUT 노드 하나 중 하나. 하나를 고르면 나머지는 풀린다
        enum class Endpoint { none, input, output };
        juce::uint64 getSelectedSlotId() const noexcept { return selectedSlotId; }
        const Slot* getSelectedSlot() const { return findSlot (selectedSlotId); }
        juce::uint64 getSelectedParallelId() const noexcept { return selectedParallelId; }
        Endpoint getSelectedEndpoint() const noexcept { return selectedEndpoint; }
        void selectSlot (juce::uint64 id);
        void selectParallel (juce::uint64 id);
        void selectEndpoint (Endpoint);

        // 편집 (모두 저장 + 크로스페이드 + 알림). 자리가 바뀌는 블록은 새 인스턴스로 다시 로딩하고, 준비될 때까지 옛 소리 유지
        void insertBlock (Place where, BlockType type, const juce::File& file);   // where 에 새 블록을 넣고 선택한다
        void addParallel (juce::uint64 belowSlotId, BlockType type, const juce::File& file);   // 본 줄 블록 아래에 새 블록 → 병렬
        void moveBlock (juce::uint64 slotId, Place where);   // where 는 옮기기 전 트리 기준 (먼저 넣고 나중에 지운다)
        void removeBlock (juce::uint64 slotId);   // 갈래가 비면 병렬 해제. 선택은 신호 흐름상 바로 앞 블록, 없으면 IN
        void setOn (juce::uint64 slotId, bool shouldBeOn);   // 현재 씬의 값만 바꾼다
        bool isOn (const Slot& slot) const noexcept { return slot.on[(size_t) currentScene]; }

        // 씬 (index 0~7). 범위를 벗어나면 무시
        int getCurrentScene() const noexcept { return currentScene; }
        const juce::String& getSceneName (int scene) const { return sceneNames[(size_t) scene]; }
        void selectScene (int scene);                  // 저장하지 않는다
        void copyScene (int source, int destination);  // 이름까지 모두 복사
        void swapScene (int source, int destination);  // 이름과 On/Off 를 맞바꾼다
        void resetScene (int scene);                   // 모두 On, 기본 이름
        void resetAllScenes();                         // 8개 씬 모두 resetScene
        void renameScene (int scene, const juce::String& name);

        // 입출력 채널 (패치별, 0부터). 장치에 없는 채널이어도 그대로 저장하고, 소리는 엔진이 기본 채널로 낸다
        const AudioEngine::Routing& getRouting() const noexcept { return routing; }
        void setInputChannel (int channel);
        void setOutputChannel (int firstChannel, bool stereo);

        // 이 패치에서 except 말고 다른 곳(지금은 IN / OUT, 나중에 FX Loop 블록)이 쓰는 장치 채널. 선택 목록에서 막는 데 쓴다
        juce::BigInteger getUsedInputChannels (Endpoint except) const;
        juce::BigInteger getUsedOutputChannels (Endpoint except) const;

        // 블록에 장치를 지정한다. None 이면 file 은 무시되고 비워진다. 블록별 값(on)은 그대로
        void assignBlock (juce::uint64 slotId, BlockType type, const juce::File& file);

        // 병렬 합류 (현재 씬의 값만 바꾼다). branch 0 = 위, 1 = 아래
        void setBranchOn (juce::uint64 parallelId, int branch, bool shouldBeOn);
        void setBranchLevel (juce::uint64 parallelId, int branch, float db);
        void resetLevels (juce::uint64 parallelId);   // Level Reset: 현재 씬의 두 갈래를 mergeDefaultDb 로
        bool isBranchOn (const Parallel& p, int branch) const noexcept { return p.branchOn[(size_t) branch][(size_t) currentScene]; }
        float getBranchLevel (const Parallel& p, int branch) const noexcept { return p.levelDb[(size_t) branch][(size_t) currentScene]; }

        // 오디오 장치(샘플레이트 등)가 바뀐 뒤 호출: 꺼진(Off) 블록까지 다시 준비
        void deviceChanged();

    private:
        Slot* editSlot (juce::uint64 id);              // findSlot 의 수정용 (0 이나 없는 id 면 nullptr)
        Parallel* editParallel (juce::uint64 id);
        template <typename Fn> void forEachSlot (Fn&& fn);
        template <typename Fn> void forEachParallel (Fn&& fn);

        Slot makeSlot (BlockType type, const juce::File& file);
        Parallel makeParallel();
        void setSelectedSlot (juce::uint64 id);   // 알림 없이 (편집 함수가 마지막에 한 번 알린다)
        bool isValidInsertPlace (const Place&) const;
        void insertSlot (Place where, Slot slot);
        void eraseSlot (juce::uint64 slotId);   // 갈래가 비면 병렬 해제 (남은 갈래 블록은 본 줄로 옮겨 다시 로딩)
        void applyMerge (Parallel&, bool immediately = false);   // 현재 씬의 갈래 On/Off·레벨을 Merge 게인으로
        void mergeEdited (Parallel&);   // 합류 값 편집 후: DSP 반영, 저장, UI 알림

        static Slot readSlot (const juce::var& json, juce::uint64 id);
        static juce::var writeSlot (const Slot&);
        static juce::var writeParallel (const Parallel&);

        void startLoading (Slot& slot);
        void onLoaded (juce::uint64 slotId, int generation, std::shared_ptr<Block> block, juce::String error);
        void rebuildChain();
        void savePatch() const;
        void loadSettings();
        void saveSettings() const;
        void changed();   // 저장 + 체인 재구성 + UI 알림
        void applyCurrentScene();   // 모든 DSP 의 On/Off 와 Merge 게인을 현재 씬 값으로
        void sceneEdited (int scene);   // 씬 편집 후: 현재 씬이면 DSP 반영, 저장, UI 알림

        static juce::String defaultSceneName (int scene) { return "Scene " + juce::String (scene + 1); }
        static juce::String sanitizeSceneName (const juce::var& value, int scene);
        static AudioEngine::Routing readRouting (const juce::var& io);   // 패치 파일의 "io" 값 검증

        AudioEngine& engine;
        juce::ThreadPool loader { 1 };          // 모델/IR 로딩 전용 스레드

        int patchNumber = 1;
        juce::String patchName { "config01" };
        float inputGainDb = 0.0f, outputGainDb = 0.0f;
        bool outputMuted = false;               // loadPatch() 에서 초기화하지 않는다
        std::vector<Item> chain;
        juce::uint64 selectedSlotId = 0, selectedParallelId = 0;   // 0 = 선택 없음
        Endpoint selectedEndpoint = Endpoint::none;
        AudioEngine::Routing routing;
        int currentScene = 0;                   // 저장하지 않는다
        std::array<juce::String, numScenes> sceneNames;
        bool waitingForLoads = false;           // 로딩을 시작한 블록(패치 전환, 추가, 이동 등)이 다 준비될 때까지 옛 소리 유지
        juce::uint64 nextId = 1;                // 슬롯과 병렬의 id (0 = 없음)

        JUCE_DECLARE_WEAK_REFERENCEABLE (RigModel)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RigModel)
    };
}
