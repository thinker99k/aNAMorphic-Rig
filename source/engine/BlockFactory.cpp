#include "BlockFactory.h"

#include "IrBlock.h"
#include "NamBlock.h"
#include "engine/param/common/GainStage.h"

namespace rig
{
    std::shared_ptr<Block> loadBlock (BlockType type, const juce::File& file, juce::String& error,
                                      std::shared_ptr<BlockParams> params)
    {
        std::shared_ptr<Block> block;

        switch (type)
        {
            case BlockType::Nam:  block = NamBlock::load (file, error, params); break;
            case BlockType::Ir:   block = IrBlock::load (file, error, params); break;
            case BlockType::None: error = "Unsupported block type"; break;
        }

        // 공용 Output: 종류와 상관없이 블록 소리 맨 끝
        if (block != nullptr && params != nullptr)
            block->setOutputStage (std::make_unique<GainStage> (params, BlockParams::outputIndex (type)));

        return block;
    }
}
