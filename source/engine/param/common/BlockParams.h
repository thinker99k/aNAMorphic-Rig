#pragma once

#include "engine/Block.h"

#include <atomic>
#include <memory>
#include <vector>

namespace rig
{
    // 블록 파라미터 하나의 정의. 스위치는 0 = 끔, 1 = 켬
    struct ParamSpec
    {
        const char* key;   // 패치 파일의 이름
        float minValue, maxValue, defaultValue;
        bool isSwitch = false;
    };

    // 블록 하나의 지금 씬 파라미터 값 (DSP 의 처리 단계가 읽는다).
    // 씬별 값은 RigModel 의 Slot 이 갖고, 씬을 바꾸거나 값을 고치면 RigModel 이 여기에 넣는다
    //  - set 은 메시지 스레드, get 은 오디오 스레드 (원자 변수)
    class BlockParams
    {
    public:
        explicit BlockParams (BlockType);

        static std::shared_ptr<BlockParams> create (BlockType);    // 기본값으로. 파라미터가 없는 종류는 nullptr
        static const std::vector<ParamSpec>& specs (BlockType);   // 순서 = 번호. 파라미터가 없는 종류는 빈 목록
        static std::vector<float> defaults (BlockType);

        float get (int index) const noexcept { return values[(size_t) index].load (std::memory_order_relaxed); }
        bool isOn (int index) const noexcept { return get (index) >= 0.5f; }
        void set (int index, float value) noexcept { values[(size_t) index].store (value, std::memory_order_relaxed); }

    private:
        std::vector<std::atomic<float>> values;   // 만든 뒤 크기 불변
    };
}
