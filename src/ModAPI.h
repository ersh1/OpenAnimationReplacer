#pragma once
#include "API/OpenAnimationReplacerAPI-Animations.h"
#include "API/OpenAnimationReplacerAPI-Conditions.h"
#include "API/OpenAnimationReplacerAPI-Functions.h"
#include "API/OpenAnimationReplacerAPI-UI.h"
#include "BaseConditions.h"
#include "BaseFunctions.h"

namespace OAR_API
{
	namespace Animations
	{
		class AnimationsInterface : public IAnimationsInterface
		{
		public:
			static AnimationsInterface* GetSingleton() noexcept
			{
				static AnimationsInterface singleton;
				return std::addressof(singleton);
			}

			// InterfaceVersion1
			[[nodiscard]] ReplacementAnimationInfo GetCurrentReplacementAnimationInfo(RE::hkbClipGenerator* a_clipGenerator) noexcept override;
			void ClearConditionStateData(RE::hkbClipGenerator* a_clipGenerator) noexcept override;
			void ClearConditionStateData(RE::TESObjectREFR* a_refr) noexcept override;

		private:
			AnimationsInterface() = default;
			AnimationsInterface(const AnimationsInterface&) = delete;
			AnimationsInterface(AnimationsInterface&&) = delete;
			virtual ~AnimationsInterface() = default;

			AnimationsInterface& operator=(const AnimationsInterface&) = delete;
			AnimationsInterface& operator=(AnimationsInterface&&) = delete;
		};
	}

	namespace Conditions
	{
		class ConditionsInterface : public IConditionsInterface
		{
		public:
			static ConditionsInterface* GetSingleton() noexcept
			{
				static ConditionsInterface singleton;
				return std::addressof(singleton);
			}

			// InterfaceVersion2
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

	namespace Functions
	{
		class FunctionsInterface : public IFunctionsInterface
		{
		public:
			static FunctionsInterface* GetSingleton() noexcept
			{
				static FunctionsInterface singleton;
				return std::addressof(singleton);
			}

			// InterfaceVersion1
			APIResult AddCustomFunction(SKSE::PluginHandle a_pluginHandle, const char* a_pluginName, REL::Version a_pluginVersion, const char* a_functionName, ::Functions::FunctionFactory a_functionFactory) noexcept override;
			::Functions::FunctionFactory GetWrappedFunctionFactory() noexcept override;
			::Functions::FunctionComponentFactory GetFunctionComponentFactory(::Functions::FunctionComponentType a_componentType) noexcept override;

		private:
			FunctionsInterface() = default;
			FunctionsInterface(const FunctionsInterface&) = delete;
			FunctionsInterface(FunctionsInterface&&) = delete;
			virtual ~FunctionsInterface() = default;

			FunctionsInterface& operator=(const FunctionsInterface&) = delete;
			FunctionsInterface& operator=(FunctionsInterface&&) = delete;
		};
	}

	namespace UI
	{
		class UIInterface : public IUIInterface
		{
		public:
			static UIInterface* GetSingleton() noexcept
			{
				static UIInterface singleton;
				return std::addressof(singleton);
			}

			// InterfaceVersion2
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
		constexpr REL::Version GetRequiredVersion() const override { return { 0, 0, 0 }; }

	protected:
		bool EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] void* a_parentSubMod) const override { return false; }
	};
}

namespace Functions
{
	class WrappedFunction : public FunctionBase
	{
	public:
		RE::BSString GetName() const override { return "WrappedFunction"sv.data(); }
		RE::BSString GetDescription() const override { return "Internal object used in custom functions"sv.data(); }
		constexpr REL::Version GetRequiredVersion() const override { return { 0, 0, 0 }; }

	protected:
		bool RunImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] void* a_parentSubMod, [[maybe_unused]] Trigger* a_trigger) const override { return false; }
	};
}
