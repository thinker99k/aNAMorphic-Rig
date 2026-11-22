#include "BlockParams.h"

#include "ValueSteps.h"

#include "engine/param/ir/IrParams.h"
#include "engine/param/nam/NamParams.h"

namespace rig
{
    double ParamSpec::constrain (double value) const
    {
        if (isSwitch)
            return value >= 0.5 ? 1.0 : 0.0;

        return steps::roundTo (juce::jlimit ((double) minValue, (double) maxValue, value), decimals);
    }

    BlockParams::BlockParams (BlockType type) : values (specs (type).size())
    {
        const auto& specList = specs (type);

        for (size_t i = 0; i < specList.size(); ++i)
            values[i].store (specList[i].defaultValue, std::memory_order_relaxed);
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
        std::vector<float> values;

        for (const auto& spec : specs (type))
            values.push_back (spec.defaultValue);

        return values;
    }
}
