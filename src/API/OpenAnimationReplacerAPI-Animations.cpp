#include "OpenAnimationReplacerAPI-Animations.h"

OAR_API::Animations::IAnimationsInterface* g_oarAnimationsInterface = nullptr;

namespace OAR_API::Animations
{
	IAnimationsInterface* GetAPI(const InterfaceVersion a_interfaceVersion /*= InterfaceVersion::Latest*/)
	{
		if (g_oarAnimationsInterface) {
			return g_oarAnimationsInterface;
		}

		const auto pluginHandle = GetModuleHandle("OpenAnimationReplacer.dll");
		const auto requestAPIFunction = reinterpret_cast<_RequestPluginAPI_Animations>(GetProcAddress(pluginHandle, "RequestPluginAPI_Animations"));
		if (!requestAPIFunction) {
			return nullptr;
		}

		const auto plugin = SKSE::PluginDeclaration::GetSingleton();
		g_oarAnimationsInterface = requestAPIFunction(a_interfaceVersion, plugin->GetName().data(), plugin->GetVersion());
		return g_oarAnimationsInterface;
	}
}
