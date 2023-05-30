#include "ReplacementAnimation.h"

SubMod* ReplacementAnimation::GetParentSubMod() const
{
    return _parentSubMod;
}

bool ReplacementAnimation::EvaluateConditions(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const
{
    if (_bDisabled) {
        return false;
    }

    if (_conditionSet->IsEmpty()) {
        return true;
    }

    return _conditionSet->EvaluateAll(a_refr, a_clipGenerator);
}
