#pragma once
#include "API/OpenAnimationReplacerAPI-Conditions.h"
#include "API/OpenAnimationReplacerAPI-UI.h"
#include "BaseConditions.h"

namespace OAR_API
{
	namespace Conditions
	{
		class ConditionsInterface : public IConditionsInterface1
		{
		public:
			static ConditionsInterface* GetSingleton() noexcept
			{
				static ConditionsInterface singleton;
				return std::addressof(singleton);
			}

			// InterfaceVersion1
			APIResult AddCustomCondition(SKSE::PluginHandle a_pluginHandle, const char* a_pluginName, REL::Version a_pluginVersion, const char* a_conditionName, ::Conditions::ConditionFactory a_conditionFactory) noexcept override;
			::Conditions::ConditionFactory GetWrappedConditionFactory() noexcept override;

			::Conditions::ConditionComponentFactory GetConditionComponentFactory(::Conditions::ConditionComponentType a_componentType) noexcept override;

		private:
			ConditionsInterface() = default;
			ConditionsInterface(const ConditionsInterface&) = delete;
			ConditionsInterface(ConditionsInterface&&) = delete;
			virtual ~ConditionsInterface() = default;

			ConditionsInterface& operator=(const ConditionsInterface&) = delete;
			ConditionsInterface& operator=(ConditionsInterface&&) = delete;
		};
	}

	namespace UI
	{
		class UIInterface : public IUIInterface1
		{
		public:
			static UIInterface* GetSingleton() noexcept
			{
				static UIInterface singleton;
				return std::addressof(singleton);
			}

			// InterfaceVersion1
			void* GetImGuiContext() noexcept override;
			void GetImGuiAllocatorFunctions(void* a_ptrAllocFunc, void* a_ptrFreeFunc, void** a_ptrUserData) noexcept override;

			void SecondColumn(float a_percent) noexcept override;
			float GetFirstColumnWidth(float a_percent) noexcept override;

		private:
			UIInterface() = default;
			UIInterface(const UIInterface&) = delete;
			UIInterface(UIInterface&&) = delete;
			virtual ~UIInterface() = default;

			UIInterface& operator=(const UIInterface&) = delete;
			UIInterface& operator=(UIInterface&&) = delete;
		};
	}

}

namespace Conditions
{
    class WrappedCondition : public ConditionBase
    {
    public:
        RE::BSString GetName() const override { return "WrappedCondition"sv.data(); }
        RE::BSString GetDescription() const override { return "Internal object used in custom conditions"sv.data(); }
        constexpr REL::Version GetRequiredVersion() const override { return {0, 0, 0}; }

    protected:
        bool EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const override { return false; }
    };
}
