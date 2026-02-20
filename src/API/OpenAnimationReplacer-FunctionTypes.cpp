#include "OpenAnimationReplacer-FunctionTypes.h"

#include "OpenAnimationReplacerAPI-Functions.h"

namespace Functions
{
	CustomFunction::CustomFunction()
	{
		const auto factory = g_oarFunctionsInterface->GetWrappedFunctionFactory();
		_wrappedFunction = std::unique_ptr<IFunction>(factory());
		_wrappedFunction->PreInitialize();
	}

	bool CustomFunction::Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const
	{
		if (!IsDisabled()) {
			// do not call wrapped function's EvaluateImpl, but our own instead
			return RunImpl(a_refr, a_clipGenerator, a_parentSubMod, a_trigger);
		}

		return false;
	}

	IFunctionComponent* CustomFunction::AddBaseComponent(FunctionComponentType a_componentType, const char* a_name, const char* a_description)
	{
		return AddComponent(g_oarFunctionsInterface->GetFunctionComponentFactory(a_componentType), a_name, a_description);
	}
}
