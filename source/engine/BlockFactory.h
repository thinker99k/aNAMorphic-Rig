#pragma once

#include "Block.h"
#include "engine/param/common/BlockParams.h"

#include <memory>

namespace rig
{
    // 블록 종류와 파일로 블록을 만든다. params 가 있으면 그 종류의 파라미터 단계(EQ, 컷 등)와 공용 Output 을 블록 뒤에 건다.
    // 무거운 작업이므로 백그라운드 스레드에서 호출할 것. 실패하면 nullptr 과 error.
    // 블록 종류를 추가할 때는 여기에 한 경우를 더한다 (NAM / IR 블록 클래스를 아는 곳은 이 파일뿐)
    std::shared_ptr<Block> loadBlock (BlockType type, const juce::File& file, juce::String& error,
                                      std::shared_ptr<BlockParams> params);
}
