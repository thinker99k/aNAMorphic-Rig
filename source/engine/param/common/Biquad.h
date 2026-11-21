#pragma once

#include <algorithm>
#include <cmath>

namespace rig
{
    // double 정밀도 2차 필터 (Direct Form II Transposed) + RBJ Audio EQ Cookbook 계수 계산.
    // 계수 계산은 할당이 없어 오디오 스레드에서 불러도 된다 (juce::dsp::IIR::Coefficients 는 할당하므로 쓰지 않는다)
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void reset() noexcept { z1 = z2 = 0; }

        void setPeak (double sr, double hz, double q, double gainDb) noexcept
        {
            const Terms t (sr, hz, q);
            const double A = std::pow (10.0, gainDb / 40.0);
            set (1 + t.alpha * A, -2 * t.cosw, 1 - t.alpha * A,
                 1 + t.alpha / A, -2 * t.cosw, 1 - t.alpha / A);
        }

        void setHighPass (double sr, double hz, double q) noexcept
        {
            const Terms t (sr, hz, q);
            set ((1 + t.cosw) / 2, -(1 + t.cosw), (1 + t.cosw) / 2, 1 + t.alpha, -2 * t.cosw, 1 - t.alpha);
        }

        void setLowPass (double sr, double hz, double q) noexcept
        {
            const Terms t (sr, hz, q);
            set ((1 - t.cosw) / 2, 1 - t.cosw, (1 - t.cosw) / 2, 1 + t.alpha, -2 * t.cosw, 1 - t.alpha);
        }

    private:
        struct Terms
        {
            double cosw, alpha;

            Terms (double sr, double hz, double q) noexcept
            {
                // 샘플레이트의 0.45 배(= 나이퀴스트의 0.9 배) 위는 막는다: 낮은 샘플레이트에서 20 kHz 설정이 불안정해지지 않게.
                // (나이퀴스트의 0.45 배로 하면 48 kHz 에서 10.8 kHz 로 막혀 RC 20 kHz 범위와 맞지 않는다)
                const double f = std::clamp (hz, 1.0, sr * 0.45);
                const double w0 = 2.0 * 3.14159265358979323846 * f / sr;
                cosw = std::cos (w0);
                alpha = std::sin (w0) / (2.0 * q);
            }
        };

        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
        {
            b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0;
            a1 = na1 / na0; a2 = na2 / na0;
        }
    };
}
