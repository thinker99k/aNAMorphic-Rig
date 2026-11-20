#include "BlockFactory.h"

#include "IrBlock.h"
#include "NamBlock.h"
#include "engine/param/ir/IrParams.h"
#include "engine/param/nam/NamParams.h"

namespace rig
{
    std::shared_ptr<Block> loadBlock (BlockType type, const juce::File& file, juce::String& error,
                                      std::shared_ptr<BlockParams> params)
    {
        switch (type)
        {
            case BlockType::Nam:  return NamBlock::load (file, error, std::dynamic_pointer_cast<NamParams> (params));
            case BlockType::Ir:   return IrBlock::load (file, error, std::dynamic_pointer_cast<IrParams> (params));
            case BlockType::None: break;
        }

        error = "Unsupported block type";
        return nullptr;
    }
}
