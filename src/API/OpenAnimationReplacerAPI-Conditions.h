#pragma once
#include "OpenAnimationReplacer-ConditionTypes.h"

/*
* For modders: Copy this file into your own project if you wish to use this API
*/
namespace OAR_API::Conditions
{
	// Available Open Animation Replacer interface versions
	enum class InterfaceVersion : uint8_t
	{
		V1,  // unsupported
		V2,
		V3,

		Latest = V3
	};

	// Error types that may be returned by Open Animation Replacer
	enum class APIResult : uint8_t
	{
		// A new custom condition was added
		OK,

		// A condition with that name already exists
		AlreadyRegistered,

		// The arguments were invalid
		Invalid,

		// Failed
		Failed
	};

	struct ReplacementAnimationInfo
	{
		std::string animationPath{};
		std::string projectName{};
		std::string variantFilename{};
		std::string subModName{};
		std::string modName{};
	};

	// Open Animation Replacer's conditions interface
	class IConditionsInterface2
	{
	public:
		/// <summary>
		/// Adds a new custom condition function.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <param name="a_myPluginName">The name of your plugin</param>
		/// <param name="a_myPluginVersion">The version of your plugin</param>
		/// <param name="a_conditionName">The name of the custom condition</param>
		/// <param name="a_conditionFactory">The factory function for the custom condition</param>
		/// <returns>OK, AlreadyRegistered, Invalid, Failed</returns>
		[[nodiscard]] virtual APIResult AddCustomCondition(SKSE::PluginHandle a_myPluginHandle, const char* a_myPluginName, REL::Version a_myPluginVersion, const char* a_conditionName, ::Conditions::ConditionFactory a_conditionFactory) noexcept = 0;

		/// <summary>
		/// Gets a wrapped condition factory - used internally in the constructor of the CustomCondition class.
		/// </summary>
		/// <returns>The condition component factory.</returns>
		[[nodiscard]] virtual ::Conditions::ConditionFactory GetWrappedConditionFactory() noexcept = 0;

		/// <summary>
		/// Gets a condition component factory for a specified condition component type.
		/// </summary>
		/// <param name="a_componentType">The type of the condition component.</param>
		/// <returns>The condition component factory.</returns>
		[[nodiscard]] virtual ::Conditions::ConditionComponentFactory GetConditionComponentFactory(::Conditions::ConditionComponentType a_componentType) noexcept = 0;
	};

	using IConditionsInterface = IConditionsInterface2;

	using _RequestPluginAPI_Conditions = IConditionsInterface* (*)(InterfaceVersion a_interfaceVersion, const char* a_pluginName, REL::Version a_pluginVersion);

	/// <summary>
	/// Request the Open Animation Replacer Conditions API interface.
	/// </summary>
	/// <param name="a_interfaceVersion">The interface version to request</param>
	/// <returns>The pointer to the API singleton, or nullptr if request failed</returns>
	IConditionsInterface* GetAPI(InterfaceVersion a_interfaceVersion = InterfaceVersion::Latest);

	/// <summary>
	/// A helper function that will try to add a new custom condition to Open Animation Replacer.
	/// Call it inside SKSEMessagingInterface::kMessage_PostLoad or before! It will have no effect otherwise, because after that point Open Animation Replacer will have already initialized its map of condition factories.
	/// </summary>
	/// <returns>OK, AlreadyExists, Invalid, Failed</returns>
	template <typename T>
	APIResult AddCustomCondition()
	{
		auto factory = ::Conditions::CustomCondition::GetFactory<T>();
		if (GetAPI()) {
			const auto plugin = SKSE::PluginDeclaration::GetSingleton();
			return GetAPI()->AddCustomCondition(SKSE::GetPluginHandle(), plugin->GetName().data(), plugin->GetVersion(),
				T::CONDITION_NAME.data(), factory);
		}

		return APIResult::Failed;
	}
}

extern OAR_API::Conditions::IConditionsInterface* g_oarConditionsInterface;
