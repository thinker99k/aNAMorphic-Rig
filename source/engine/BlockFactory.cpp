#include "BlockFactory.h"

#include "IrBlock.h"
#include "NamBlock.h"

namespace rig
{
    std::shared_ptr<Block> loadBlock (BlockType type, const juce::File& file, juce::String& error,
                                      std::shared_ptr<BlockParams> params)
    {
        switch (type)
        {
            case BlockType::Nam:  return NamBlock::load (file, error, std::move (params));
            case BlockType::Ir:   return IrBlock::load (file, error, std::move (params));
            case BlockType::None: break;
        }

        error = "Unsupported block type";
        return nullptr;
    }
}
