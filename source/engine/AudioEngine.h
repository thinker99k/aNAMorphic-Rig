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

        AudioEngine() = default;
        ~AudioEngine() override;

        // --- 메시지 스레드 전용 ---
        void setChain (std::unique_ptr<Chain> newChain);
        void collectGarbage();

        double getSampleRate() const noexcept   { return sampleRate.load(); }
        int getMaxBlockSize() const noexcept    { return maxBlockSize.load(); }

        // UI 미터용: 마지막으로 읽은 뒤의 피크 값을 가져오고 0 으로 초기화
        float takeInputPeak() noexcept  { return inputPeak.exchange (0.0f); }
        float takeOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

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

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
    };
}
