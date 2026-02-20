#pragma once
#include "OpenAnimationReplacer-FunctionTypes.h"

/*
* For modders: Copy this file into your own project if you wish to use this API
*/
namespace OAR_API::Functions
{
	// Available Open Animation Replacer interface versions
	enum class InterfaceVersion : uint8_t
	{
		V1,

		Latest = V1
	};

	// Error types that may be returned by Open Animation Replacer
	enum class APIResult : uint8_t
	{
		// A new custom function was added
		OK,

		// A function with that name already exists
		AlreadyRegistered,

		// The arguments were invalid
		Invalid,

		// Failed
		Failed
	};

	// Open Animation Replacer's functions interface
	class IFunctionsInterface1
	{
	public:
		/// <summary>
		/// Adds a new custom function.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <param name="a_myPluginName">The name of your plugin</param>
		/// <param name="a_myPluginVersion">The version of your plugin</param>
		/// <param name="a_functionName">The name of the custom function</param>
		/// <param name="a_functionFactory">The function factory</param>
		/// <returns>OK, AlreadyRegistered, Invalid, Failed</returns>
		[[nodiscard]] virtual APIResult AddCustomFunction(SKSE::PluginHandle a_myPluginHandle, const char* a_myPluginName, REL::Version a_myPluginVersion, const char* a_functionName, ::Functions::FunctionFactory a_functionFactory) noexcept = 0;

		/// <summary>
		/// Gets a wrapped function factory - used internally in the constructor of the CustomEvent class.
		/// </summary>
		/// <returns>The function factory.</returns>
		[[nodiscard]] virtual ::Functions::FunctionFactory GetWrappedFunctionFactory() noexcept = 0;

		/// <summary>
		/// Gets a function component factory for a specified function component type.
		/// </summary>
		/// <param name="a_componentType">The type of the function component.</param>
		/// <returns>The function component factory.</returns>
		[[nodiscard]] virtual ::Functions::FunctionComponentFactory GetFunctionComponentFactory(::Functions::FunctionComponentType a_componentType) noexcept = 0;
	};

	using IFunctionsInterface = IFunctionsInterface1;

	using _RequestPluginAPI_Functions = IFunctionsInterface* (*)(InterfaceVersion a_interfaceVersion, const char* a_pluginName, REL::Version a_pluginVersion);

	/// <summary>
	/// Request the Open Animation Replacer Functions API interface.
	/// </summary>
	/// <param name="a_interfaceVersion">The interface version to request</param>
	/// <returns>The pointer to the API singleton, or nullptr if request failed</returns>
	IFunctionsInterface* GetAPI(InterfaceVersion a_interfaceVersion = InterfaceVersion::Latest);

	/// <summary>
	/// A helper function that will try to add a new custom function to Open Animation Replacer.
	/// Call it inside SKSEMessagingInterface::kMessage_PostLoad or before! It will have no effect otherwise, because after that point Open Animation Replacer will have already initialized its map of function factories.
	/// </summary>
	/// <returns>OK, AlreadyExists, Invalid, Failed</returns>
	template <typename T>
	APIResult AddCustomFunction()
	{
		auto factory = ::Functions::CustomFunction::GetFactory<T>();
		if (GetAPI()) {
			const auto plugin = SKSE::PluginDeclaration::GetSingleton();
			return GetAPI()->AddCustomFunction(SKSE::GetPluginHandle(), plugin->GetName().data(), plugin->GetVersion(),
				T::FUNCTION_NAME.data(), factory);
		}

		return APIResult::Failed;
	}
}

extern OAR_API::Functions::IFunctionsInterface* g_oarFunctionsInterface;
