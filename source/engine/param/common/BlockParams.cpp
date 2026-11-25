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

    const ParamSpec& CommonParams::outputSpec()
    {
        static const ParamSpec s { "outputDb", (float) outputMinDb, (float) outputMaxDb, (float) outputDefaultDb, steps::dbDecimals };
        return s;
    }

    namespace
    {
        std::vector<ParamSpec> withCommon (const std::vector<ParamSpec>& own)
        {
            auto all = own;
            all.push_back (CommonParams::outputSpec());
            return all;
        }
    }

    const std::vector<ParamSpec>& BlockParams::specs (BlockType type)
    {
        static const std::vector<ParamSpec> none;
        static const auto nam = withCommon (NamParams::specs());
        static const auto ir = withCommon (IrParams::specs());

        switch (type)
        {
            case BlockType::Nam:  return nam;
            case BlockType::Ir:   return ir;
            case BlockType::None: break;
        }

        return none;
    }

    int BlockParams::outputIndex (BlockType type)
    {
        return (int) specs (type).size() - 1;   // 공용은 Output 하나, 맨 끝
    }

    int BlockParams::headlineIndex (BlockType type)
    {
        // NAM = Gain (드라이브), IR = Output (부스트). 블록 종류를 추가할 때는 여기에 한 경우를 더한다
        switch (type)
        {
            case BlockType::Nam:  return NamParams::gainDb;
            case BlockType::Ir:   return outputIndex (type);
            case BlockType::None: break;
        }

        return -1;
    }

    std::vector<float> BlockParams::defaults (BlockType type)
    {
        std::vector<float> values;

        for (const auto& spec : specs (type))
            values.push_back (spec.defaultValue);

        return values;
    }
}
