#include "IrParams.h"

namespace rig
{
    juce::var IrParams::toVar() const
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("lowCutOn",  lowCutOn.load());
        obj->setProperty ("lowCutHz",  (double) lowCutHz.load());
        obj->setProperty ("highCutOn", highCutOn.load());
        obj->setProperty ("highCutHz", (double) highCutHz.load());
        return juce::var (obj.release());
    }

    void IrParams::fromVar (const juce::var& v)
    {
        // 항목이 없거나 객체가 아니면 getProperty 가 빈 값을 돌려주므로 필드마다 기본값이 된다
        lowCutOn.store  (readBool (v, "lowCutOn", false));
        lowCutHz.store  ((float) readNumber (v, "lowCutHz", lowCutDefaultHz, lowCutMinHz, lowCutMaxHz));
        highCutOn.store (readBool (v, "highCutOn", false));
        highCutHz.store ((float) readNumber (v, "highCutHz", highCutDefaultHz, highCutMinHz, highCutMaxHz));
    }
}
