#include "ModAPI.h"
#include "OpenAnimationReplacer.h"

namespace OAR_API
{
	namespace Animations
	{
		ReplacementAnimationInfo AnimationsInterface::GetCurrentReplacementAnimationInfo(RE::hkbClipGenerator* a_clipGenerator) noexcept
		{
			ReplacementAnimationInfo info;

			if (const auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_clipGenerator)) {
				if (const auto replacementAnimation = activeClip->GetReplacementAnimation()) {
					info.animationPath = replacementAnimation->GetAnimPath();
					info.projectName = replacementAnimation->GetProjectName();

					if (replacementAnimation->HasVariants()) {
						replacementAnimation->ForEachVariant([&](const Variant& a_variant) {
							if (a_variant.GetIndex() == activeClip->GetCurrentIndex()) {
								info.variantFilename = a_variant.GetFilename();
								return RE::BSVisit::BSVisitControl::kStop;
							}
							return RE::BSVisit::BSVisitControl::kContinue;
						});
					}

					if (const auto parentSubMod = replacementAnimation->GetParentSubMod()) {
						info.subModName = parentSubMod->GetName();
						if (const auto parentMod = parentSubMod->GetParentMod()) {
							info.modName = parentMod->GetName();
						}
					}
				}
			}

			return info;
		}

		void AnimationsInterface::ClearConditionStateData(RE::hkbClipGenerator* a_clipGenerator) noexcept
		{
			if (const auto clip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_clipGenerator)) {
				OpenAnimationReplacer::GetSingleton().ClearConditionStateDataForRefr(clip->GetRefr());
			}
		}

		void AnimationsInterface::ClearConditionStateData(RE::TESObjectREFR* a_refr) noexcept
		{
			OpenAnimationReplacer::GetSingleton().ClearConditionStateDataForRefr(a_refr);
		}
	}

	namespace Conditions
	{
		APIResult ConditionsInterface::AddCustomCondition([[maybe_unused]] SKSE::PluginHandle a_pluginHandle, const char* a_pluginName, REL::Version a_pluginVersion, const char* a_conditionName, ::Conditions::ConditionFactory a_conditionFactory) noexcept
		{
			return OpenAnimationReplacer::GetSingleton().AddCustomCondition(a_pluginName, a_pluginVersion, a_conditionName, a_conditionFactory);
		}

		::Conditions::ICondition* WrappedConditionFactory()
		{
			return new ::Conditions::WrappedCondition();
		}

		::Conditions::ConditionFactory ConditionsInterface::GetWrappedConditionFactory() noexcept
		{
			return &WrappedConditionFactory;
		}

		::Conditions::IConditionComponent* MultiConditionComponentFactory(const ::Conditions::ICondition* a_parentCondition, const char* a_name, const char* a_description)
		{
			return new ::Conditions::MultiConditionComponent(a_parentCondition, a_name, a_description);
		}

		::Conditions::IConditionComponent* FormConditionComponentFactory(const ::Conditions::ICondition* a_parentCondition, const char* a_name, const char* a_description)
		{
			return new ::Conditions::FormConditionComponent(a_parentCondition, a_name, a_description);
		}

		::Conditions::IConditionComponent* NumericConditionComponentFactory(const ::Conditions::ICondition* a_parentCondition, const char* a_name, const char* a_description)
		{
			return new ::Conditions::NumericConditionComponent(a_parentCondition, a_name, a_description);
		}

		::Conditions::IConditionComponent* NiPoint3ConditionComponentFactory(const ::Conditions::ICondition* a_parentCondition, const char* a_name, const char* a_description)
		{
			return new ::Conditions::NiPoint3ConditionComponent(a_parentCondition, a_name, a_description);
		}

		::Conditions::IConditionComponent* KeywordConditionComponentFactory(const ::Conditions::ICondition* a_parentCondition, const char* a_name, const char* a_description)
		{
			return new ::Conditions::KeywordConditionComponent(a_parentCondition, a_name, a_description);
		}

		::Conditions::IConditionComponent* TextConditionComponentFactory(const ::Conditions::ICondition* a_parentCondition, const char* a_name, const char* a_description)
		{
			return new ::Conditions::TextConditionComponent(a_parentCondition, a_name, a_description);
		}

		::Conditions::IConditionComponent* BoolConditionComponentFactory(const ::Conditions::ICondition* a_parentCondition, const char* a_name, const char* a_description)
		{
			return new ::Conditions::BoolConditionComponent(a_parentCondition, a_name, a_description);
		}

		::Conditions::IConditionComponent* ComparisonConditionComponentFactory(const ::Conditions::ICondition* a_parentCondition, const char* a_name, const char* a_description)
		{
			return new ::Conditions::ComparisonConditionComponent(a_parentCondition, a_name, a_description);
		}

		::Conditions::IConditionComponent* ConditionStateComponentFactory(const ::Conditions::ICondition* a_parentCondition, const char* a_name, const char* a_description)
		{
			return new ::Conditions::ConditionStateComponent(a_parentCondition, a_name, a_description);
		}

		::Conditions::ConditionComponentFactory ConditionsInterface::GetConditionComponentFactory(::Conditions::ConditionComponentType a_componentType) noexcept
		{
			switch (a_componentType) {
			case ::Conditions::ConditionComponentType::kMulti:
				return &MultiConditionComponentFactory;
			case ::Conditions::ConditionComponentType::kForm:
				return &FormConditionComponentFactory;
			case ::Conditions::ConditionComponentType::kNumeric:
				return &NumericConditionComponentFactory;
			case ::Conditions::ConditionComponentType::kNiPoint3:
				return &NiPoint3ConditionComponentFactory;
			case ::Conditions::ConditionComponentType::kKeyword:
				return &KeywordConditionComponentFactory;
			case ::Conditions::ConditionComponentType::kText:
				return &TextConditionComponentFactory;
			case ::Conditions::ConditionComponentType::kBool:
				return &BoolConditionComponentFactory;
			case ::Conditions::ConditionComponentType::kComparison:
				return &ComparisonConditionComponentFactory;
			case ::Conditions::ConditionComponentType::kState:
				return &ConditionStateComponentFactory;
			}

			return nullptr;
		}
	}

	namespace Functions
	{
		APIResult FunctionsInterface::AddCustomFunction([[maybe_unused]] SKSE::PluginHandle a_pluginHandle, const char* a_pluginName, REL::Version a_pluginVersion, const char* a_functionName, ::Functions::FunctionFactory a_functionFactory) noexcept
		{
			return OpenAnimationReplacer::GetSingleton().AddCustomFunction(a_pluginName, a_pluginVersion, a_functionName, a_functionFactory);
		}

		::Functions::IFunction* WrappedFunctionFactory()
		{
			return new ::Functions::WrappedFunction();
		}

		::Functions::FunctionFactory FunctionsInterface::GetWrappedFunctionFactory() noexcept
		{
			return &WrappedFunctionFactory;
		}

		::Functions::IFunctionComponent* MultiFunctionComponentFactory(const ::Functions::IFunction* a_parentFunction, const char* a_name, const char* a_description)
		{
			return new ::Functions::MultiFunctionComponent(a_parentFunction, a_name, a_description);
		}

		::Functions::IFunctionComponent* FormFunctionComponentFactory(const ::Functions::IFunction* a_parentFunction, const char* a_name, const char* a_description)
		{
			return new ::Functions::FormFunctionComponent(a_parentFunction, a_name, a_description);
		}

		::Functions::IFunctionComponent* NumericFunctionComponentFactory(const ::Functions::IFunction* a_parentFunction, const char* a_name, const char* a_description)
		{
			return new ::Functions::NumericFunctionComponent(a_parentFunction, a_name, a_description);
		}

		::Functions::IFunctionComponent* NiPoint3FunctionComponentFactory(const ::Functions::IFunction* a_parentFunction, const char* a_name, const char* a_description)
		{
			return new ::Functions::NiPoint3FunctionComponent(a_parentFunction, a_name, a_description);
		}

		::Functions::IFunctionComponent* KeywordFunctionComponentFactory(const ::Functions::IFunction* a_parentFunction, const char* a_name, const char* a_description)
		{
			return new ::Functions::KeywordFunctionComponent(a_parentFunction, a_name, a_description);
		}

		::Functions::IFunctionComponent* TextFunctionComponentFactory(const ::Functions::IFunction* a_parentFunction, const char* a_name, const char* a_description)
		{
			return new ::Functions::TextFunctionComponent(a_parentFunction, a_name, a_description);
		}

		::Functions::IFunctionComponent* BoolFunctionComponentFactory(const ::Functions::IFunction* a_parentFunction, const char* a_name, const char* a_description)
		{
			return new ::Functions::BoolFunctionComponent(a_parentFunction, a_name, a_description);
		}

		::Functions::IFunctionComponent* ConditionFunctionComponentFactory(const ::Functions::IFunction* a_parentFunction, const char* a_name, const char* a_description)
		{
			return new ::Functions::ConditionFunctionComponent(a_parentFunction, a_name, a_description);
		}

		::Functions::FunctionComponentFactory FunctionsInterface::GetFunctionComponentFactory(::Functions::FunctionComponentType a_componentType) noexcept
		{
			switch (a_componentType) {
			case ::Functions::FunctionComponentType::kMulti:
				return &MultiFunctionComponentFactory;
			case ::Functions::FunctionComponentType::kForm:
				return &FormFunctionComponentFactory;
			case ::Functions::FunctionComponentType::kNumeric:
				return &NumericFunctionComponentFactory;
			case ::Functions::FunctionComponentType::kNiPoint3:
				return NiPoint3FunctionComponentFactory;
			case ::Functions::FunctionComponentType::kKeyword:
				return KeywordFunctionComponentFactory;
			case ::Functions::FunctionComponentType::kText:
				return TextFunctionComponentFactory;
			case ::Functions::FunctionComponentType::kBool:
				return BoolFunctionComponentFactory;
			case ::Functions::FunctionComponentType::kCondition:
				return ConditionFunctionComponentFactory;
			}

			return nullptr;
		}
	}

	namespace UI
	{
		void* UIInterface::GetImGuiContext() noexcept
		{
			return ImGui::GetCurrentContext();
		}

		void UIInterface::GetImGuiAllocatorFunctions(void* a_ptrAllocFunc, void* a_ptrFreeFunc, void** a_ptrUserData) noexcept
		{
			ImGui::GetAllocatorFunctions(static_cast<ImGuiMemAllocFunc*>(a_ptrAllocFunc), static_cast<ImGuiMemFreeFunc*>(a_ptrFreeFunc), a_ptrUserData);
		}

		void UIInterface::SecondColumn(float a_percent) noexcept
		{
			::UI::UICommon::SecondColumn(a_percent);
		}

		float UIInterface::GetFirstColumnWidth(float a_percent) noexcept
		{
			return ::UI::UICommon::FirstColumnWidth(a_percent);
		}
	}
}
