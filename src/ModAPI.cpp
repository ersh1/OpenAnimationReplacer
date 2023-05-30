#include "ModAPI.h"
#include "OpenAnimationReplacer.h"

namespace OAR_API
{
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

		::Conditions::IConditionComponent* MultiConditionComponentFactory(const char* a_name)
		{
			return new ::Conditions::MultiConditionComponent(a_name);
		}

		::Conditions::IConditionComponent* FormConditionComponentFactory(const char* a_name)
		{
			return new ::Conditions::FormConditionComponent(a_name);
		}

		::Conditions::IConditionComponent* NumericConditionComponentFactory(const char* a_name)
		{
			return new ::Conditions::NumericConditionComponent(a_name);
		}

		::Conditions::IConditionComponent* NiPoint3ConditionComponentFactory(const char* a_name)
		{
			return new ::Conditions::NiPoint3ConditionComponent(a_name);
		}

		::Conditions::IConditionComponent* KeywordConditionComponentFactory(const char* a_name)
		{
			return new ::Conditions::KeywordConditionComponent(a_name);
		}

		::Conditions::IConditionComponent* TextConditionComponentFactory(const char* a_name)
		{
			return new ::Conditions::TextConditionComponent(a_name);
		}

		::Conditions::IConditionComponent* BoolConditionComponentFactory(const char* a_name)
		{
			return new ::Conditions::BoolConditionComponent(a_name);
		}

		::Conditions::IConditionComponent* ComparisonConditionComponentFactory(const char* a_name)
		{
			return new ::Conditions::ComparisonConditionComponent(a_name);
		}

		::Conditions::IConditionComponent* RandomConditionComponentFactory(const char* a_name)
		{
			return new ::Conditions::RandomConditionComponent(a_name);
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
			case ::Conditions::ConditionComponentType::kRandom:
				return &RandomConditionComponentFactory;
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
