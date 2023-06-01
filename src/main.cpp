#include "Hooks.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"
#include "UI/UIManager.h"

#include "API/OpenAnimationReplacerAPI-Conditions.h"
#include "API/OpenAnimationReplacerAPI-UI.h"
#include "ModAPI.h"

#include "MergeMapperPluginAPI.h"
#include "AnimationFileHashCache.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
    switch (a_msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        OpenAnimationReplacer::GetSingleton().OnDataLoaded();
        break;
    case SKSE::MessagingInterface::kPostLoad:
        // check if DAR is present
        if (GetModuleHandle("DynamicAnimationReplacer.dll")) {
            util::report_and_fail("DynamicAnimationReplacer.dll is loaded!\n\nOpen Animation Replacer is an alternative with backwards compatibility and support for all game versions.\n\nUninstall Dynamic Animation Replacer to proceed."sv);
        }
        break;
    case SKSE::MessagingInterface::kPostPostLoad:
        MergeMapperPluginAPI::GetMergeMapperInterface001();
        if (g_mergeMapperInterface) {
            const auto version = g_mergeMapperInterface->GetBuildNumber();
            logger::info("Got MergeMapper interface buildnumber {}", version);
        } else {
            logger::info("MergeMapper not detected");
        }
        if (!OpenAnimationReplacer::GetSingleton().AreFactoriesInitialized()) {
            OpenAnimationReplacer::GetSingleton().InitFactories();
        }
        break;
    }
}

namespace
{
    void InitializeLog()
    {
#ifndef NDEBUG
        auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format("{}.log"sv, Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
        const auto level = spdlog::level::trace;
#else
		const auto level = spdlog::level::info;
#endif

        auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
        log->set_level(level);
        log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = Plugin::NAME.data();
    a_info->version = Plugin::VERSION.pack();

    if (a_skse->IsEditor()) {
        logger::critical("Loaded in editor, marking as incompatible"sv);
        return false;
    }

    const auto ver = a_skse->RuntimeVersion();
    if (REL::Module::IsSE() && ver < SKSE::RUNTIME_SSE_1_5_39 || REL::Module::IsVR() && ver < SKSE::RUNTIME_LATEST_VR) {
        logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
        return false;
    }

    return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
    SKSE::PluginVersionData v;

    v.PluginVersion(Plugin::VERSION);
    v.PluginName(Plugin::NAME);
    v.AuthorName("Ersh");
    v.UsesAddressLibrary(true);
    v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST });
    v.HasNoStructUse(true);

    return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
#ifndef NDEBUG
    while (!IsDebuggerPresent()) {
        Sleep(100);
    }
#endif
    REL::Module::reset(); // Clib-NG bug workaround

    // check if DAR is present
    if (GetModuleHandle("DynamicAnimationReplacer.dll")) {
        util::report_and_fail("DynamicAnimationReplacer.dll is loaded!\n\nOpen Animation Replacer is an alternative with backwards compatibility and support for all game versions.\n\nUninstall Dynamic Animation Replacer to proceed."sv);
    }

    InitializeLog();
    logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());

	SKSE::Init(a_skse);

    const auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", MessageHandler)) {
        return false;
    }

    AnimationFileHashCache::GetSingleton().ReadCacheFromDisk();

    Settings::Initialize();
    Settings::ReadSettings();

    Hooks::Install();

    return true;
}

extern "C" DLLEXPORT OAR_API::Conditions::IConditionsInterface1* SKSEAPI RequestPluginAPI_Conditions(const OAR_API::Conditions::InterfaceVersion a_interfaceVersion, const char* a_pluginName, REL::Version a_pluginVersion)
{
    const auto api = OAR_API::Conditions::ConditionsInterface::GetSingleton();

    if (a_pluginName == nullptr) {
        logger::info("OpenAnimationReplacer::RequestPluginAPI_Conditions called with a nullptr plugin name");
        return nullptr;
    }

    logger::info("OpenAnimationReplacer::RequestPluginAPI_Conditions called, InterfaceVersion {} (Plugin name: {}, version: {}", static_cast<uint8_t>(a_interfaceVersion) + 1, a_pluginName, a_pluginVersion);

    switch (a_interfaceVersion) {
    case OAR_API::Conditions::InterfaceVersion::V1:
        logger::info("OpenAnimationReplacer::RequestPluginAPI_Conditions returned the API singleton");
        return api;
    }

    logger::info("OpenAnimationReplacer::RequestPluginAPI_Conditions requested the wrong interface version");
    return nullptr;
}

extern "C" DLLEXPORT OAR_API::UI::IUIInterface1* SKSEAPI RequestPluginAPI_UI(const OAR_API::UI::InterfaceVersion a_interfaceVersion, const char* a_pluginName, REL::Version a_pluginVersion)
{
    const auto api = OAR_API::UI::UIInterface::GetSingleton();

    if (a_pluginName == nullptr) {
        logger::info("OpenAnimationReplacer::RequestPluginAPI_UI called with a nullptr plugin name");
        return nullptr;
    }

    logger::info("OpenAnimationReplacer::RequestPluginAPI_UI called, InterfaceVersion {} (Plugin name: {}, version: {}", static_cast<uint8_t>(a_interfaceVersion) + 1, a_pluginName, a_pluginVersion);

    switch (a_interfaceVersion) {
    case OAR_API::UI::InterfaceVersion::V1:
        logger::info("OpenAnimationReplacer::RequestPluginAPI_UI returned the API singleton");
        return api;
    }

    logger::info("OpenAnimationReplacer::RequestPluginAPI_UI requested the wrong interface version");
    return nullptr;
}
