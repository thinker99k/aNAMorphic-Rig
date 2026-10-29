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
    //  1) 현재 패치를 ~/aNAMorphic-Rig/config<N> 에 자동 저장하고
    //  2) 오디오 엔진에 새 Chain 을 넘기고
    //  3) ChangeBroadcaster 로 UI 에 알린다.
    class RigModel final : public juce::ChangeBroadcaster
    {
    public:
        struct Slot
        {
            juce::uint64 id = 0;                 // 슬롯 고유 번호 (비동기 로딩 결과를 찾을 때 사용)
            BlockType type = BlockType::Nam;
            juce::File file;                     // 비어 있으면 아직 파일 미지정
            bool bypassed = false;

            std::shared_ptr<Block> dsp;          // 로딩 완료된 DSP (없으면 통과)
            bool loading = false;
            juce::String error;
            int loadGeneration = 0;              // 늦게 도착한 옛 로딩 결과를 버리기 위함
        };

        static constexpr int numPatches = 32;

        explicit RigModel (AudioEngine& engine);
        ~RigModel() override;

        // 패치
        int getPatchNumber() const noexcept { return patchNumber; }
        void loadPatch (int number);

        // 체인 편집 (블록 개수 제한 없음: CPU 가 버티는 만큼)
        const std::vector<Slot>& getSlots() const noexcept { return slots; }
        int getSelectedIndex() const noexcept { return selectedIndex; }
        const Slot* getSelectedSlot() const noexcept;

        void select (int index);
        void addBlock (BlockType type);
        void removeBlock (int index);
        void moveBlock (int index, int delta);
        void setBypassed (int index, bool shouldBeBypassed);
        void setFile (int index, const juce::File& file);

        // 오디오 장치(샘플레이트 등)가 바뀐 뒤 호출: 바이패스된 블록까지 다시 준비
        void deviceChanged();

    private:
        void startLoading (Slot& slot);
        void onLoaded (juce::uint64 slotId, int generation, std::shared_ptr<Block> block, juce::String error);
        void rebuildChain();
        void savePatch() const;
        void changed();   // 저장 + 체인 재구성 + UI 알림

        AudioEngine& engine;
        juce::ThreadPool loader { 1 };          // 모델/IR 로딩 전용 스레드

        int patchNumber = 1;
        std::vector<Slot> slots;
        int selectedIndex = -1;
        juce::uint64 nextSlotId = 1;

        JUCE_DECLARE_WEAK_REFERENCEABLE (RigModel)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RigModel)
    };
}
