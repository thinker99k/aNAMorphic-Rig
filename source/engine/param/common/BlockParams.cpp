#include "BlockParams.h"

#include "engine/param/ir/IrParams.h"
#include "engine/param/nam/NamParams.h"

namespace rig
{
    std::shared_ptr<BlockParams> BlockParams::create (BlockType type)
    {
        switch (type)
        {
            case BlockType::Nam:  return std::make_shared<NamParams>();
            case BlockType::Ir:   return std::make_shared<IrParams>();
            case BlockType::None: break;
        }

        return nullptr;
    }

    const std::vector<BlockType>& BlockParams::paramTypes()
    {
        static const std::vector<BlockType> types { BlockType::Nam, BlockType::Ir };
        return types;
    }

    double BlockParams::readNumber (const juce::var& obj, const juce::Identifier& key, double def, double lo, double hi)
    {
        const auto v = obj.getProperty (key, {});

        if (! (v.isDouble() || v.isInt() || v.isInt64()))
            return def;

        const auto d = (double) v;
        return std::isfinite (d) ? juce::jlimit (lo, hi, d) : def;
    }

    bool BlockParams::readBool (const juce::var& obj, const juce::Identifier& key, bool def)
    {
        const auto v = obj.getProperty (key, {});
        return v.isBool() ? (bool) v : def;
    }
}
