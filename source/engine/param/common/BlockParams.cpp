#include "BlockParams.h"

#include "engine/param/ir/IrParams.h"
#include "engine/param/nam/NamParams.h"

namespace rig
{
    BlockParams::BlockParams (BlockType type) : values (specs (type).size())
    {
        const auto& s = specs (type);

        for (size_t i = 0; i < s.size(); ++i)
            values[i].store (s[i].defaultValue, std::memory_order_relaxed);
    }

    std::shared_ptr<BlockParams> BlockParams::create (BlockType type)
    {
        return specs (type).empty() ? nullptr : std::make_shared<BlockParams> (type);
    }

    const std::vector<ParamSpec>& BlockParams::specs (BlockType type)
    {
        static const std::vector<ParamSpec> none;

        switch (type)
        {
            case BlockType::Nam:  return NamParams::specs();
            case BlockType::Ir:   return IrParams::specs();
            case BlockType::None: break;
        }

        return none;
    }

    std::vector<float> BlockParams::defaults (BlockType type)
    {
        std::vector<float> v;

        for (const auto& s : specs (type))
            v.push_back (s.defaultValue);

        return v;
    }
}
