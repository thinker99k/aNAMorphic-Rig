#include "ParamPanel.h"

#include "IrParamPanel.h"
#include "NamParamPanel.h"

namespace rig
{
    std::unique_ptr<ParamPanel> createParamPanel (BlockType type, RigModel& model)
    {
        switch (type)
        {
            case BlockType::Nam:  return std::make_unique<NamParamPanel> (model);
            case BlockType::Ir:   return std::make_unique<IrParamPanel> (model);
            case BlockType::None: break;
        }

        return nullptr;
    }
}
