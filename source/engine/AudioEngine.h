#pragma once

#include "Block.h"
#include "Merge.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace rig
{
    // 오디오 장치 콜백 + 직렬/병렬 체인 처리기.
    //
    // 스레드 모델
    //  - 메시지(UI) 스레드가 처리할 블록 트리(프로그램)를 setProgram() 으로 넘긴다.
    //  - 엔진은 지금 소리와 새 프로그램을 비교해 Transition 을 만들어 원자 포인터(live)로 오디오 스레드에 넘긴다. 락 없음.
    //  - 교체된 옛 Transition 은 바로 지우지 않고 retired 목록에 두었다가,
    //    오디오 콜백이 충분히 지나간 뒤 메시지 스레드에서 collectGarbage() 로 해제한다.
    //    (오디오 스레드에서 delete 가 일어나지 않게 하기 위함. Transition 이 블록도 소유하므로 블록 수명도 같이 해결됨)
    //
    // 크로스페이드 (소리가 바뀌는 모든 편집: 추가, 파일 교체, 삭제, 이동, 병렬 생성/해제, 패치 전환. On/Off 는 Block 의 wet/dry,
    // 병렬 갈래 On/Off·레벨은 Merge 의 게인으로)
    //  - 옛/새 프로그램의 각 목록(본 줄, 병렬 안의 각 갈래)에서 양쪽에 같은 순서로 남은 노드(같은 블록 인스턴스, 같은 Merge)는
    //    한 번만 처리하고, 그 사이사이의 달라진 구간(gap)마다 옛 구간과 새 구간을 같은 입력으로 함께 돌려 crossfadeSeconds 동안
    //    등전력으로 섞는다. 빈 구간은 통과이므로 추가 = dry → wet, 삭제 = wet → dry, 교체 = 옛 → 새.
    //  - 같은 인스턴스를 두 번 처리할 수 없으므로, 새 구간에 지금 소리의 블록/Merge 가 들어 있으면(이동, 병렬 생성/해제)
    //    보내지 않고 기다린다. (RigModel 이 자리가 바뀐 블록을 새 인스턴스로 로딩해서 해결한다)
    //  - 페이드 중에 온 새 프로그램은 페이드가 끝난 뒤 최신 것 하나만 적용한다.
    //
    // 입출력 채널 (Routing, 패치별)
    //  - 장치는 항상 모든 채널을 열어 두고(openAllChannels), 콜백 배열 번호 = 장치 채널 번호로 읽고 쓴다. 채널을 바꿔도 장치를 다시 열지 않는다
    //  - 채널이 바뀌면 체인과 같은 페이드 구간에서 입력은 옛/새 채널을 섞고, 출력은 옛 채널을 줄이며 새 채널을 키운다
    //  - 장치에 없는 채널이면 기본 채널(입력 1, 출력 1+2 / 채널이 하나뿐이면 1)로 대신한다. 패치 값은 그대로 둔다
    class AudioEngine final : public juce::AudioIODeviceCallback
    {
    public:
        // 프로그램의 노드 하나: 블록 하나, 또는 병렬(두 갈래 + 합류 Merge). 병렬은 한 단계만 (갈래 안에 병렬 없음)
        struct Node
        {
            std::shared_ptr<Block> block;                    // 블록이면
            std::shared_ptr<Merge> merge;                    // 병렬이면 (block 은 nullptr)
            std::array<std::vector<Node>, 2> branches;       // 병렬의 위 / 아래 갈래 (빈 갈래 = 통과)

            bool isParallel() const noexcept { return merge != nullptr; }
            bool sameNode (const Node& o) const noexcept { return block == o.block && merge == o.merge; }   // 갈래 내용은 비교 안 함
        };

        using Program = std::vector<Node>;   // 실제로 처리할 노드만 순서대로

        // 채널 번호는 0부터. 출력은 stereoOutput 이면 output, output + 1 두 채널
        struct Routing
        {
            int input = 0;
            int output = 0;
            bool stereoOutput = true;

            bool operator== (const Routing&) const = default;
        };

        // 장치를 열 때 요청하는 채널 수. 모든 백엔드가 실제 채널 수로 잘라서 열므로 "전부"라는 뜻
        static constexpr int openAllChannels = 256;

        static constexpr double crossfadeSeconds = transitionSeconds;   // 20 ms (Block.h)

        // 입력/출력 볼륨 범위 (dB). 입력의 최솟값은 실제 -15 dB, 출력은 outputMinDb 이하가 -inf(무음)
        static constexpr float inputMinDb  = -15.0f;
        static constexpr float inputMaxDb  =  15.0f;
        static constexpr float outputMinDb = -60.0f;
        static constexpr float outputMaxDb =  10.0f;

        AudioEngine() = default;
        ~AudioEngine() override;

        // --- 메시지 스레드 전용 ---
        void setProgram (Program program, Routing routing);
        void collectGarbage();   // 주기적으로 호출: 옛 Transition 해제 + 기다리던 프로그램 적용

        // 오디오 스레드가 지금(또는 방금 전까지) 처리할 수 있는 블록인지
        bool isInUse (const Block* block) const;

        // 입력 직후(체인 앞) / 출력 직전(체인 뒤) 볼륨
        void setInputGainDb (float db);
        void setOutputGainDb (float db);

        double getSampleRate() const noexcept   { return sampleRate.load(); }
        int getMaxBlockSize() const noexcept    { return maxBlockSize.load(); }

        // UI 미터용: 마지막으로 읽은 뒤의 피크 값을 가져오고 0 으로 초기화
        float takeInputPeak() noexcept  { return inputPeak.exchange (0.0f); }
        float takeOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

        // 튜너: 켜져 있는 동안 출력은 무음 (체인은 계속 처리. 켜고 끌 때 transitionSeconds 동안 등전력으로 줄이고 키운다)
        void setTunerActive (bool shouldBeActive) noexcept { tunerActive.store (shouldBeActive, std::memory_order_relaxed); }

        // 튜너 피치 분석용 (UI 스레드): 고른 입력 채널의 입력 볼륨 전 신호의 최근 numSamples 개.
        // 데이터가 부족하거나 복사 중에 덮어쓰였으면 false
        bool copyLatestInput (float* dest, int numSamples) const noexcept { return inputRing.copyLatest (dest, numSamples); }

        // [SPECTRUM] begin (테스트용)
        // 스펙트럼 창용 (UI 스레드): 출력 게인 뒤, 출력 채널 복사 직전 신호의 최근 numSamples 개.
        // 데이터가 부족하거나 복사 중에 덮어쓰였으면 false
        bool copyLatestOutput (float* dest, int numSamples) const noexcept { return outputScope.copyLatest (dest, numSamples); }

        // 스펙트럼 창이 보일 때만 true: 꺼져 있으면 오디오 스레드는 기록하지 않는다
        void setScopeEnabled (bool shouldBeEnabled) noexcept { scopeEnabled.store (shouldBeEnabled, std::memory_order_relaxed); }
        // [SPECTRUM] end

        // --- AudioIODeviceCallback ---
        void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                               int numInputChannels,
                                               float* const* outputChannelData,
                                               int numOutputChannels,
                                               int numSamples,
                                               const juce::AudioIODeviceCallbackContext& context) override;
        void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;

    private:
        // 페이드 중 처리 순서의 한 칸: 양쪽에 남은 노드 하나, 또는 달라진 구간(gap) 하나
        struct Step
        {
            bool gap = false;
            Node node;                                   // 남은 노드 (gap 이 아닐 때). 병렬이면 갈래는 아래 branches 로 처리
            std::array<std::vector<Step>, 2> branches;   // 남은 병렬의 위 / 아래 갈래
            Program oldPart, newPart;                    // gap: 같은 입력으로 함께 돌려 섞는다 (빈 쪽 = 통과)
        };

        // 메시지 스레드가 만들고, 공개된 뒤에는 바뀌지 않는다
        struct Transition
        {
            Program program;               // 새 프로그램. 페이드가 없거나 끝나면 이것만 처리한다
            std::vector<Step> steps;       // 페이드 중 처리 순서 (crossfade 일 때만)
            Routing oldRouting, newRouting;
            bool crossfade = false;   // false 면 steps, oldRouting 은 쓰지 않는다
            juce::uint64 id = 0;      // settled 비교용 (주소 재사용과 헷갈리지 않도록)

            template <typename Fn> void forEachBlock (Fn&& fn) const;
        };

        // 핑퐁 버퍼 쌍의 수: 본 줄 1 + gap 의 옛 구간 1 + 병렬 아래 갈래 1 (병렬은 한 단계라 이 이상 겹치지 않는다)
        static constexpr int numScratchPairs = 3;

        static void storeMax (std::atomic<float>& target, float value) noexcept;

        // 장치에 없는 채널이면 기본 채널로. 입력이 하나도 없으면 input = -1, 출력이 없으면 출력 채널 수 0
        struct Channels
        {
            int input = -1, firstOutput = 0, numOutputs = 0;
            bool hasOutput (int ch) const noexcept { return ch >= firstOutput && ch < firstOutput + numOutputs; }
        };

        static Channels resolve (const Routing&, int numInputs, int numOutputs) noexcept;
        // 결과는 항상 a. free = 아직 쓰지 않은 첫 버퍼 쌍 (병렬 아래 갈래, gap 의 옛 구간이 차례로 가져간다)
        void processProgram (const Program&, float*& a, float*& b, int free, int n) noexcept;
        void processSteps (const std::vector<Step>&, float*& a, float*& b, int free, int n) noexcept;
        float fadeAngle (int i) const noexcept;   // 페이드 진행 각도 (0 = 옛 소리, halfPi = 새 소리)
        void dispatch();   // 기다리던 프로그램을 보낼 수 있으면 보낸다
        static void diff (const Program& oldList, const Program& newList, std::vector<Step>& out);   // 페이드 중 처리 순서 만들기
        void publish (std::unique_ptr<Transition> next);

        std::atomic<Transition*> live { nullptr };
        std::unique_ptr<Transition> owned;       // live 가 가리키는 객체의 소유자
        std::atomic<juce::uint64> settled { 0 };   // 오디오 스레드가 페이드를 끝낸 Transition 의 id
        juce::uint64 nextTransitionId = 1;

        Program wanted;                           // 가장 최근 setProgram() 값
        Routing wantedRouting;
        bool hasWanted = false;

        struct Retired
        {
            std::unique_ptr<Transition> transition;
            juce::uint64 callbackStamp;
        };

        std::vector<Retired> retired;
        std::atomic<juce::uint64> callbackCount { 0 };
        std::atomic<bool> deviceRunning { false };

        std::atomic<double> sampleRate { 48000.0 };
        std::atomic<int> maxBlockSize { 512 };

        juce::AudioBuffer<float> scratch;        // numScratchPairs * 2 채널 (핑퐁 버퍼 쌍)

        // 오디오 스레드 전용 크로스페이드 상태
        Transition* current = nullptr;
        int fadePosition = 0, fadeLength = 1;

        std::atomic<float> inputPeak { 0.0f };
        std::atomic<float> outputPeak { 0.0f };

        // 목표 게인(선형). 메시지 스레드가 쓰고 오디오 스레드가 읽는다
        std::atomic<float> inputGain { 1.0f };
        std::atomic<float> outputGain { 1.0f };

        // 오디오 스레드 전용: 게인 변화를 transitionSeconds(20 ms) 동안 선형으로 이어 "틱" 잡음을 막는다
        juce::SmoothedValue<float> inputSmoothed { 1.0f }, outputSmoothed { 1.0f };

        // 튜너 무음 진행 (오디오 스레드 전용): 0 = 소리, 1 = 무음. transitionSeconds 동안 선형으로 가고, 게인은 cos (등전력)
        std::atomic<bool> tunerActive { false };
        juce::SmoothedValue<float> tunerFade { 0.0f };

        // 가장 최근 샘플을 덮어쓰는 링 버퍼. 쓰는 쪽은 오디오 스레드 하나, 읽는 쪽은 UI 스레드 하나
        struct SampleRing
        {
            // 2의 거듭제곱. 한 번에 읽을 수 있는 길이는 절반까지 (스펙트럼 8192점, 튜너 약 5300점 = 192 kHz 에서 2048 + 3200)
            static constexpr int size = 32768;

            // atomic 배열: 쓰는 도중에 읽어도 data race 가 아니다 (relaxed 라 일반 float 와 같은 속도). 생성 후 크기 불변
            std::unique_ptr<std::atomic<float>[]> data { new std::atomic<float>[size] {} };
            std::atomic<juce::uint64> writePos { 0 };   // 지금까지 쓴 샘플 총 개수

            void write (const float* samples, int n) noexcept;              // 오디오 스레드
            bool copyLatest (float* dest, int numSamples) const noexcept;   // UI 스레드
        };

        SampleRing inputRing;   // 튜너용 입력. 튜너가 닫혀 있어도 늘 기록 (다시 열었을 때 옛 소리를 읽지 않게)

        // [SPECTRUM] begin (테스트용)
        SampleRing outputScope;
        std::atomic<bool> scopeEnabled { false };   // 스펙트럼 창이 보일 때만 true
        // [SPECTRUM] end

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
    };
}
