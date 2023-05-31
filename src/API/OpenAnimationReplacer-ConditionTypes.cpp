#include "OpenAnimationReplacer-ConditionTypes.h"

#include "OpenAnimationReplacerAPI-Conditions.h"

namespace Conditions
{
    CustomCondition::CustomCondition()
    {
        const auto factory = g_oarConditionsInterface->GetWrappedConditionFactory();
        _wrappedCondition = std::unique_ptr<ICondition>(factory());
        _wrappedCondition->PreInitialize();
    }

    bool CustomCondition::Evaluate(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (IsDisabled()) {
            return true;
        }

        // do not call wrapped function's EvaluateImpl, but our own instead
        return IsNegated() ? !EvaluateImpl(a_refr, a_clipGenerator) : EvaluateImpl(a_refr, a_clipGenerator);
    }

    IConditionComponent* CustomCondition::AddBaseComponent(ConditionComponentType a_componentType, const char* a_name, const char* a_description/* = ""*/)
    {
        return AddComponent(g_oarConditionsInterface->GetConditionComponentFactory(a_componentType), a_name, a_description);
    }
}
