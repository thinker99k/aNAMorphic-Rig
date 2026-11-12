#pragma once

#include "engine/AudioEngine.h"
#include "engine/Block.h"

#include <juce_events/juce_events.h>

#include <memory>
#include <vector>

namespace rig
{
    // 체인 편집 상태 (메시지 스레드 전용).
    //
    // UI 는 RigModel 만 보고 편집하고, RigModel 은 변경될 때마다
    //  1) 현재 패치를 ~/Documents/aNAMorphic-Rig/patch/config<NN> 에 자동 저장하고
    //  2) 오디오 엔진에 새 Chain 을 넘기고
    //  3) ChangeBroadcaster 로 UI 에 알린다.
    class RigModel final : public juce::ChangeBroadcaster
    {
    public:
        struct Slot
        {
            juce::uint64 id = 0;                 // 슬롯 고유 번호 (비동기 로딩 결과를 찾을 때 사용)
            BlockType type = BlockType::None;
            juce::File file;                     // None 이면 비어 있고, NAM / IR 이면 항상 지정되어 있다
            bool on = true;                      // 블록별 값: 종류가 바뀌어도 블록에 남는다

            std::shared_ptr<Block> dsp;          // 소리를 내는 DSP (없으면 통과). 새 파일을 로딩하는 동안은 옛 DSP 를 그대로 둔다
            bool loading = false;
            juce::String error;
            int loadGeneration = 0;              // 늦게 도착한 옛 로딩 결과를 버리기 위함
        };

        static constexpr int numPatches = 32;
        static constexpr int maxPatchNameLength = 32;   // 글자(코드 포인트) 수

        // 패치 파일의 "name" 값 검증: 문자열이 아니거나 공백뿐이면 fallback, 길면 앞 32자만
        static juce::String sanitizePatchName (const juce::var& value, const juce::String& fallback);

        explicit RigModel (AudioEngine& engine);
        ~RigModel() override;

        // 패치
        int getPatchNumber() const noexcept { return patchNumber; }
        const juce::String& getPatchName() const noexcept { return patchName; }
        void setPatchName (const juce::String& name);
        void loadPatch (int number);

        // 입력/출력 볼륨 (dB, 앱 전역, settings.json 에 저장). UI 재구성을 막기 위해 변경 알림은 보내지 않는다
        float getInputGainDb() const noexcept  { return inputGainDb; }
        float getOutputGainDb() const noexcept { return outputGainDb; }
        void setInputGainDb (float db);
        void setOutputGainDb (float db);   // 뮤트도 함께 해제한다
        void saveGains();                   // 노브 드래그가 끝났을 때 settings.json 에 저장

        // 출력 뮤트: settings.json 에 저장하지 않고 패치를 바꿔도 유지된다. 뮤트 중에도 outputGainDb 는 원래 값
        bool isOutputMuted() const noexcept { return outputMuted; }
        void setOutputMuted (bool shouldBeMuted);

        // 체인 편집 (블록 개수 제한 없음: CPU 가 버티는 만큼)
        const std::vector<Slot>& getSlots() const noexcept { return slots; }
        int getSelectedIndex() const noexcept { return selectedIndex; }
        const Slot* getSelectedSlot() const noexcept;

        void select (int index);
        void insertBlock (int index);   // 미지정(None) 블록을 index 자리에 넣고 선택한다
        void removeBlock (int index);
        void moveBlock (int index, int delta);
        void setOn (int index, bool shouldBeOn);

        // 블록에 장치를 지정한다. None 이면 file 은 무시되고 비워진다. 블록별 값(on)은 그대로
        void assignBlock (int index, BlockType type, const juce::File& file);

        // 오디오 장치(샘플레이트 등)가 바뀐 뒤 호출: 꺼진(Off) 블록까지 다시 준비
        void deviceChanged();

    private:
        void startLoading (Slot& slot);
        void onLoaded (juce::uint64 slotId, int generation, std::shared_ptr<Block> block, juce::String error);
        void rebuildChain();
        void savePatch() const;
        void loadSettings();
        void saveSettings() const;
        void changed();   // 저장 + 체인 재구성 + UI 알림

        AudioEngine& engine;
        juce::ThreadPool loader { 1 };          // 모델/IR 로딩 전용 스레드

        int patchNumber = 1;
        juce::String patchName { "config01" };
        float inputGainDb = 0.0f, outputGainDb = 0.0f;
        bool outputMuted = false;               // loadPatch() 에서 초기화하지 않는다
        std::vector<Slot> slots;
        int selectedIndex = -1;
        bool waitingForPatch = false;           // 패치 전환 후 새 패치 블록이 다 로딩될 때까지 옛 소리 유지
        juce::uint64 nextSlotId = 1;

        JUCE_DECLARE_WEAK_REFERENCEABLE (RigModel)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RigModel)
    };
}
