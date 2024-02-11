#include "OpenAnimationReplacerAPI-UI.h"

#include <imgui.h>

OAR_API::UI::IUIInterface* g_oarUIInterface = nullptr;

namespace OAR_API::UI
{
    IUIInterface* GetAPI(const InterfaceVersion a_interfaceVersion /*= InterfaceVersion::Latest*/)
    {
        if (g_oarUIInterface) {
            return g_oarUIInterface;
        }

        const auto pluginHandle = GetModuleHandle("OpenAnimationReplacer.dll");
        const auto requestAPIFunction = reinterpret_cast<_RequestPluginAPI_UI>(GetProcAddress(pluginHandle, "RequestPluginAPI_UI"));
        if (!requestAPIFunction) {
            return nullptr;
        }

        const auto plugin = SKSE::PluginDeclaration::GetSingleton();
        g_oarUIInterface = requestAPIFunction(a_interfaceVersion, plugin->GetName().data(), plugin->GetVersion());
        return g_oarUIInterface;
    }

    bool InitializeImGuiContext()
    {
        if (GetAPI()) {
            const auto ctx = static_cast<ImGuiContext*>(g_oarUIInterface->GetImGuiContext());
            ImGuiMemAllocFunc memAllocFunction = nullptr;
            ImGuiMemFreeFunc memFreeFunction = nullptr;
            void* userData = nullptr;

            g_oarUIInterface->GetImGuiAllocatorFunctions(&memAllocFunction, &memFreeFunction, &userData);
            if (ctx && memAllocFunction && memFreeFunction) {
                ImGui::SetCurrentContext(ctx);
                ImGui::SetAllocatorFunctions(memAllocFunction, memFreeFunction, userData);

                bImGuiContextInitialized = true;
            }
        }

        return bImGuiContextInitialized;
    }
}
