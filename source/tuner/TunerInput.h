#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <memory>

namespace rig
{
    // 튜너용 입력 복사 콜백. AudioDeviceManager 에 엔진과 따로 등록한다 (튜너 대화상자가 열려 있는 동안만)
    //  - 지금 패치의 입력 채널(setInputChannel)의 원래 신호(입력 볼륨 전)를 락 없는 링 버퍼에 쓴다
    //    엔진처럼 콜백 배열 번호 = 장치 채널 번호이고, 장치에 없는 채널이면 입력 1 로 대신한다 (AudioEngine::resolve 와 같은 규칙)
    //  - 출력: 통과(setPassThrough)가 꺼져 있으면 0 (AudioDeviceManager 가 여러 콜백의 출력을 더하므로 소리를 보태지 않는다).
    //          켜져 있으면 그 입력을 그대로(게인 1) 패치의 출력 채널(setOutput)로 내보낸다. 켜고 끌 때 20 ms 페이드
    //          출력 채널 규칙도 AudioEngine::resolve 와 같다 (없으면 1+2, 채널이 하나뿐이면 1)
    //  - 쓰기는 오디오 스레드 하나, 읽기(copyLatest)는 UI 스레드 하나
    class TunerInput final : public juce::AudioIODeviceCallback
    {
    public:
        // --- UI(메시지) 스레드 ---
        void   setInputChannel (int deviceChannel) noexcept { wantedChannel.store (deviceChannel, std::memory_order_relaxed); }   // 0부터 (Routing::input)
        bool   copyLatest (float* dest, int numSamples) const noexcept;   // 최근 numSamples 개. 부족하거나 중간에 덮어쓰이면 false
        double getSampleRate() const noexcept       { return sampleRate.load (std::memory_order_relaxed); }
        int    getInputChannel() const noexcept     { return inputChannel.load (std::memory_order_relaxed); }   // 실제로 읽는 채널. 1부터, 없으면 0

        // 입력 통과 출력 (Routing::output / stereoOutput 과 같은 뜻)
        void   setOutput (int firstChannel, bool stereo) noexcept
        {
            outputChannel.store (firstChannel, std::memory_order_relaxed);
            stereoOutput.store (stereo, std::memory_order_relaxed);
        }
        void   setPassThrough (bool shouldPass) noexcept { passThrough.store (shouldPass, std::memory_order_relaxed); }
        bool   isPassingThrough() const noexcept       { return passThrough.load (std::memory_order_relaxed); }

        static constexpr double fadeSeconds = 0.02;

        // --- AudioIODeviceCallback (오디오 스레드) ---
        void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
        void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                               float* const* outputChannelData, int numOutputChannels,
                                               int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    private:
        // 장치에 없는 채널이면 0. 입력이 하나도 없으면 -1
        int resolveChannel (int numInputs) const noexcept
        {
            if (numInputs <= 0)
                return -1;

            const int wanted = wantedChannel.load (std::memory_order_relaxed);
            return juce::isPositiveAndBelow (wanted, numInputs) ? wanted : 0;
        }

        static constexpr int ringSize = 16384;   // 2의 거듭제곱. 요청 길이의 2배 이상이어야 한다
        static_assert ((ringSize & (ringSize - 1)) == 0, "ringSize must be a power of two");

        std::unique_ptr<std::atomic<float>[]> data { new std::atomic<float>[ringSize] {} };
        std::atomic<juce::uint64> writePos { 0 };
        std::atomic<double> sampleRate { 48000.0 };
        std::atomic<int> inputChannel { 0 };
        std::atomic<int> wantedChannel { 0 };

        std::atomic<int> outputChannel { 0 };
        std::atomic<bool> stereoOutput { true };
        std::atomic<bool> passThrough { false };
        juce::SmoothedValue<float> passGain { 0.0f };   // 오디오 스레드 전용
    };
}
