#include "OpenAnimationReplacerAPI-Conditions.h"

OAR_API::Conditions::IConditionsInterface2* g_oarConditionsInterface = nullptr;

namespace OAR_API::Conditions
{
    IConditionsInterface2* GetAPI(const InterfaceVersion a_interfaceVersion /*= InterfaceVersion::V2*/)
    {
        if (g_oarConditionsInterface) {
            return g_oarConditionsInterface;
        }

        const auto pluginHandle = GetModuleHandle("OpenAnimationReplacer.dll");
        const auto requestAPIFunction = static_cast<_RequestPluginAPI_Conditions>(GetProcAddress(pluginHandle, "RequestPluginAPI_Conditions"));
        if (!requestAPIFunction) {
            return nullptr;
        }

        const auto plugin = SKSE::PluginDeclaration::GetSingleton();
        g_oarConditionsInterface = requestAPIFunction(a_interfaceVersion, plugin->GetName().data(), plugin->GetVersion());
        return g_oarConditionsInterface;
    }
}
