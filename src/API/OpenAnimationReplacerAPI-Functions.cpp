#include "OpenAnimationReplacerAPI-Functions.h"

OAR_API::Functions::IFunctionsInterface* g_oarFunctionsInterface = nullptr;

namespace OAR_API::Functions
{
	IFunctionsInterface* GetAPI(const InterfaceVersion a_interfaceVersion /*= InterfaceVersion::Latest*/)
	{
		if (g_oarFunctionsInterface) {
			return g_oarFunctionsInterface;
		}

		const auto pluginHandle = GetModuleHandle("OpenAnimationReplacer.dll");
		const auto requestAPIFunction = reinterpret_cast<_RequestPluginAPI_Functions>(GetProcAddress(pluginHandle, "RequestPluginAPI_Functions"));
		if (!requestAPIFunction) {
			return nullptr;
		}

		const auto plugin = SKSE::PluginDeclaration::GetSingleton();
		g_oarFunctionsInterface = requestAPIFunction(a_interfaceVersion, plugin->GetName().data(), plugin->GetVersion());
		return g_oarFunctionsInterface;
	}
}
