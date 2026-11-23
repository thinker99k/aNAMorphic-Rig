#include "PitchDetector.h"

#include <algorithm>
#include <cmath>

namespace rig
{
    void PitchDetector::prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        tauMax = (int) std::ceil (sampleRate / minHz);
        tauMin = (int) std::floor (sampleRate / maxHz);
        d.assign ((size_t) tauMax + 2, 0.0f);
        cmnd.assign ((size_t) tauMax + 2, 1.0f);
    }

    int PitchDetector::samplesNeeded() const noexcept
    {
        return window + tauMax + 2;
    }

    PitchResult PitchDetector::detect (const float* x, int n) noexcept
    {
        if (x == nullptr || tauMax <= 0 || n < samplesNeeded())
            return {};

        // 1. 소리가 작으면 검출하지 않는다
        double energy = 0.0;
        for (int j = 0; j < window; ++j)
            energy += (double) x[j] * x[j];

        if (std::sqrt (energy / window) < silenceRms)
            return {};

        // 2. 차이 함수 (x[j + t] 는 최대 window + tauMax, n 보다 작다)
        for (int t = 1; t <= tauMax + 1; ++t)
        {
            double sum = 0.0;
            for (int j = 0; j < window; ++j)
            {
                const double diff = (double) x[j] - x[j + t];
                sum += diff * diff;
            }
            d[(size_t) t] = (float) sum;
        }

        // 3. 누적 평균 정규화
        cmnd[0] = 1.0f;
        double running = 0.0;
        for (int t = 1; t <= tauMax + 1; ++t)
        {
            running += d[(size_t) t];
            cmnd[(size_t) t] = running > 0.0 ? (float) ((double) d[(size_t) t] * t / running) : 1.0f;
        }

        // 4. 임계값보다 처음 낮아지는 곳에서 지역 최솟값까지
        int tau = -1;
        for (int t = std::max (tauMin, 2); t <= tauMax; ++t)
        {
            if (cmnd[(size_t) t] < threshold)
            {
                while (t + 1 <= tauMax && cmnd[(size_t) t + 1] < cmnd[(size_t) t])
                    ++t;
                tau = t;
                break;
            }
        }

        if (tau < 0)
            return {};

        // 5. 포물선 보간 (tau <= tauMax 이므로 cmnd[tau + 1] 은 크기 안)
        const double a = cmnd[(size_t) tau - 1], b = cmnd[(size_t) tau], c = cmnd[(size_t) tau + 1];
        const double den = a - 2.0 * b + c;
        const double offset = den != 0.0 ? 0.5 * (a - c) / den : 0.0;

        // 6.
        return { true, sampleRate / (tau + offset) };
    }

    NoteInfo PitchDetector::toNote (double hz, double referenceHz) noexcept
    {
        static constexpr const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        NoteInfo note;
        if (hz <= 0.0 || referenceHz <= 0.0)
            return note;

        const double midiFloat = 69.0 + 12.0 * std::log2 (hz / referenceHz);
        note.midi  = (int) std::lround (midiFloat);
        note.cents = (midiFloat - note.midi) * 100.0;
        note.name  = names[((note.midi % 12) + 12) % 12];
        return note;
    }
}
