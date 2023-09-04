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
						replacementAnimation->ForEachVariant([&](const ReplacementAnimation::Variant& a_variant) {
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

		void AnimationsInterface::ClearRandomFloats(RE::hkbClipGenerator* a_clipGenerator) noexcept
		{
			if (const auto clip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_clipGenerator)) {
				clip->ClearRandomFloats();
			}
		}

		void AnimationsInterface::ClearRandomFloats(RE::TESObjectREFR* a_refr) noexcept
		{
			const auto clips = OpenAnimationReplacer::GetSingleton().GetActiveClipsForRefr(a_refr);
			for (auto& clip : clips) {
				clip->ClearRandomFloats();
			}
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

		::Conditions::IConditionComponent* RandomConditionComponentFactory(const ::Conditions::ICondition* a_parentCondition, const char* a_name, const char* a_description)
		{
			return new ::Conditions::RandomConditionComponent(a_parentCondition, a_name, a_description);
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
