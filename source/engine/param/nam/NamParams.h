#pragma once

#include "engine/param/common/BlockParams.h"

#include <atomic>

namespace rig
{
    // NAM 블록용 3밴드 EQ 값 (패치당 1벌, 체인의 모든 NAM 블록 뒤에 각각 적용)
    class NamParams final : public BlockParams
    {
    public:
        // 범위·경계선·폭 상수는 여기에만 둔다 (UI 도 이 값을 쓴다)
        static constexpr double gainMinDb = -18.0, gainMaxDb = 18.0, gainDefaultDb = 0.0;

        // 세 밴드 모두 peak 필터, Freq = 중심 주파수
        static constexpr double bassMinHz   = 40.0,   bassMaxHz   = 200.0,   bassDefaultHz   = 100.0;
        static constexpr double middleMinHz = 315.0,  middleMaxHz = 2000.0,  middleDefaultHz = 800.0;
        static constexpr double trebleMinHz = 2500.0, trebleMaxHz = 10000.0, trebleDefaultHz = 4000.0;

        static constexpr double h1Hz = 250.0;    // Bass / Middle 경계
        static constexpr double h2Hz = 2500.0;   // Middle / Treble 경계

        static constexpr double eqQ = 0.8;       // Bass / Middle / Treble 공통 폭 (약 1.5~2 옥타브)

        std::atomic<bool> eqOn { false };
        std::atomic<float> bassDb   { (float) gainDefaultDb }, bassHz   { (float) bassDefaultHz };
        std::atomic<float> middleDb { (float) gainDefaultDb }, middleHz { (float) middleDefaultHz };
        std::atomic<float> trebleDb { (float) gainDefaultDb }, trebleHz { (float) trebleDefaultHz };

        juce::var toVar() const override;
        void fromVar (const juce::var&) override;
        void setDefaults() override;
    };

    // 침범 금지: 어떤 조합에서도 Bass Freq < Middle Freq < Treble Freq
    static_assert (NamParams::bassMaxHz   <  NamParams::h1Hz && NamParams::h1Hz < NamParams::middleMinHz);   // 200 < 250 < 315
    static_assert (NamParams::middleMinHz >= NamParams::bassMaxHz   * 1.5);                                  // 최소 간격
    // Middle / Treble 은 순서만 보장한다 (Treble Freq 를 2.5 kHz 부터 쓰기 위해 H2 여유와 1.5 배 간격 규칙은 뺐다)
    static_assert (NamParams::middleMaxHz < NamParams::trebleMinHz);                                         // 2000 < 2500
}
