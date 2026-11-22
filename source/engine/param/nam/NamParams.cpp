#include "NamParams.h"

namespace rig
{
    juce::var NamParams::toVar() const
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("eqOn",     eqOn.load());
        obj->setProperty ("bassDb",   (double) bassDb.load());
        obj->setProperty ("bassHz",   (double) bassHz.load());
        obj->setProperty ("middleDb", (double) middleDb.load());
        obj->setProperty ("middleHz", (double) middleHz.load());
        obj->setProperty ("trebleDb", (double) trebleDb.load());
        obj->setProperty ("trebleHz", (double) trebleHz.load());
        return juce::var (obj.release());
    }

    void NamParams::fromVar (const juce::var& v)
    {
        // 항목이 없거나 객체가 아니면 getProperty 가 빈 값을 돌려주므로 필드마다 기본값이 된다
        eqOn.store     (readBool   (v, "eqOn", false));
        bassDb.store   ((float) readNumber (v, "bassDb",   gainDefaultDb,   gainMinDb,   gainMaxDb));
        bassHz.store   ((float) readNumber (v, "bassHz",   bassDefaultHz,   bassMinHz,   bassMaxHz));
        middleDb.store ((float) readNumber (v, "middleDb", gainDefaultDb,   gainMinDb,   gainMaxDb));
        middleHz.store ((float) readNumber (v, "middleHz", middleDefaultHz, middleMinHz, middleMaxHz));
        trebleDb.store ((float) readNumber (v, "trebleDb", gainDefaultDb,   gainMinDb,   gainMaxDb));
        trebleHz.store ((float) readNumber (v, "trebleHz", trebleDefaultHz, trebleMinHz, trebleMaxHz));
    }
}
