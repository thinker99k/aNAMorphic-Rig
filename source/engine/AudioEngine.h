#pragma once

#include "Block.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <memory>
#include <vector>

namespace rig
{
    // 오디오 장치 콜백 + 직렬 체인 처리기.
    //
    // 스레드 모델
    //  - 메시지(UI) 스레드가 새 Chain 을 통째로 만들어 setChain() 으로 넘긴다.
    //  - 오디오 스레드는 원자 포인터(live)로 현재 Chain 을 읽기만 한다. 락 없음.
    //  - 교체된 옛 Chain 은 바로 지우지 않고 retired 목록에 두었다가,
    //    오디오 콜백이 충분히 지나간 뒤 메시지 스레드에서 collectGarbage() 로 해제한다.
    //    (오디오 스레드에서 delete 가 일어나지 않게 하기 위함)
    class AudioEngine final : public juce::AudioIODeviceCallback
    {
    public:
        struct Chain
        {
            // 실제로 처리할 블록만 순서대로 (바이패스/빈 슬롯은 제외된 상태)
            std::vector<std::shared_ptr<Block>> blocks;
        };

        // 입력/출력 볼륨 범위 (dB). 입력의 최솟값은 실제 -15 dB, 출력은 outputMinDb 이하가 -inf(무음)
        static constexpr float inputMinDb  = -15.0f;
        static constexpr float inputMaxDb  =  15.0f;
        static constexpr float outputMinDb = -60.0f;
        static constexpr float outputMaxDb =  10.0f;

        AudioEngine() = default;
        ~AudioEngine() override;

        // --- 메시지 스레드 전용 ---
        void setChain (std::unique_ptr<Chain> newChain);
        void collectGarbage();

        // 입력 직후(체인 앞) / 출력 직전(체인 뒤) 볼륨
        void setInputGainDb (float db);
        void setOutputGainDb (float db);

        double getSampleRate() const noexcept   { return sampleRate.load(); }
        int getMaxBlockSize() const noexcept    { return maxBlockSize.load(); }

        // UI 미터용: 마지막으로 읽은 뒤의 피크 값을 가져오고 0 으로 초기화
        float takeInputPeak() noexcept  { return inputPeak.exchange (0.0f); }
        float takeOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

        // 파형 창용 (UI 스레드): 가장 최근 numSamples 개를 복사. 데이터가 부족하거나 도중에 덮어쓰였으면 false
        //  - 입력: 입력 게인 앞 (IN 미터와 같은 지점)
        //  - 출력: 출력 게인 뒤, 출력 채널로 복사하기 직전 (OUT 미터와 같은 지점)
        bool copyLatestInput (float* dest, int numSamples) const noexcept  { return inputScope.copyLatest (dest, numSamples); }
        bool copyLatestOutput (float* dest, int numSamples) const noexcept { return outputScope.copyLatest (dest, numSamples); }

        // 파형 창이 보일 때만 true: 꺼져 있으면 오디오 스레드는 기록하지 않는다
        void setScopeEnabled (bool shouldBeEnabled) noexcept { scopeEnabled.store (shouldBeEnabled, std::memory_order_relaxed); }

        // 가장 최근 샘플을 덮어쓰는 링 버퍼. 쓰는 쪽은 오디오 스레드 하나, 읽는 쪽은 UI 스레드 하나
        struct ScopeRing
        {
            static constexpr int size = 32768;   // 2의 거듭제곱, 48 kHz 에서 약 0.68 초

            void write (const float* samples, int n) noexcept;                  // 오디오 스레드
            bool copyLatest (float* dest, int numSamples) const noexcept;       // UI 스레드

        private:
            // atomic 배열: 쓰는 도중에 읽어도 data race 가 아니다 (relaxed 라 일반 float 와 같은 속도). 생성 후 크기 불변
            std::unique_ptr<std::atomic<float>[]> data { new std::atomic<float>[size] {} };
            std::atomic<juce::uint64> writePos { 0 };                           // 지금까지 쓴 샘플 총 개수
        };

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
        static void storeMax (std::atomic<float>& target, float value) noexcept;

        std::atomic<Chain*> live { nullptr };
        std::unique_ptr<Chain> owned;            // live 가 가리키는 객체의 소유자

        struct Retired
        {
            std::unique_ptr<Chain> chain;
            juce::uint64 callbackStamp;
        };
        std::vector<Retired> retired;

        std::atomic<juce::uint64> callbackCount { 0 };
        std::atomic<bool> deviceRunning { false };

        std::atomic<double> sampleRate { 48000.0 };
        std::atomic<int> maxBlockSize { 512 };

        juce::AudioBuffer<float> scratch;        // 2채널: 핑퐁 버퍼

        std::atomic<float> inputPeak { 0.0f };
        std::atomic<float> outputPeak { 0.0f };

        // 목표 게인(선형). 메시지 스레드가 쓰고 오디오 스레드가 읽는다
        std::atomic<float> inputGain { 1.0f };
        std::atomic<float> outputGain { 1.0f };

        // 오디오 스레드 전용: 게인 변화를 20 ms 동안 선형으로 이어 "틱" 잡음을 막는다
        juce::SmoothedValue<float> inputSmoothed { 1.0f }, outputSmoothed { 1.0f };

        ScopeRing inputScope, outputScope;
        std::atomic<bool> scopeEnabled { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
    };
}
