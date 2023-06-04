#pragma once

#include <Simpleini.h>

struct Settings
{
    enum class AnimationLogMode : uint32_t
    {
        kLogReplacementOnly,
        kLogPotentialReplacements,
        kLogAll,
    };

    static void Initialize();
    static void ReadSettings();
    static void WriteSettings();

    static void ReadBoolSetting(const CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, bool& a_setting);
    static void ReadFloatSetting(const CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, float& a_setting);
    static void ReadUInt16Setting(const CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, uint16_t& a_setting);
    static void ReadUInt32Setting(const CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, uint32_t& a_setting);

    static void WriteBoolSetting(const char* a_sectionName, const char* a_settingName, const bool& a_setting);
    static void WriteFloatSetting(const char* a_sectionName, const char* a_settingName, const float& a_setting);
    static void WriteUInt16Setting(const char* a_sectionName, const char* a_settingName, const uint16_t& a_setting);
    static void WriteUInt32Setting(const char* a_sectionName, const char* a_settingName, const uint32_t& a_setting);

    // General
    static inline uint16_t uAnimationLimit = 0x7FFF;
    static inline uint32_t uHavokHeapSize = 0x40000000;
    static inline bool bAsyncParsing = true;
    static inline bool bLoadDefaultBehaviorsInMainMenu = true;

    // Duplicate filtering
    static inline bool bFilterOutDuplicateAnimations = true;
    //static inline bool bCacheAnimationFileHashes = false;

    // UI
    static inline bool bEnableUI = true;
    static inline bool bShowWelcomeBanner = true;
    static inline uint32_t uToggleUIKeyData[4];
    static inline float fUIScale = 1.f;

    static inline bool bEnableAnimationQueueProgressBar = true;
    static inline float fAnimationQueueLingerTime = 5.f;

    static inline bool bEnableAnimationLog = false;
    static inline uint32_t uAnimationLogMaxEntries = 10;
    static inline uint32_t uAnimationActivateLogMode = 0;
    static inline uint32_t uAnimationEchoLogMode = 0;
    static inline uint32_t uAnimationLoopLogMode = 0;
    static inline bool bAnimationLogOnlyActiveGraph = true;
    static inline bool bAnimationLogWriteToTextLog = false;

    // Workarounds
    static inline bool bLegacyKeepRandomResultsByDefault = true;

    // Experimental
    static inline bool bDisablePreloading = false;
    static inline bool bIncreaseAnimationLimit = false;

    // Internal
    static inline float fBlendTimeOnInterrupt = 0.3f;
    static inline float fBlendTimeOnLoop = 0.3f;
    static inline float fQueueFadeTime = 1.f;
    static inline float fAnimationLogEntryFadeTime = 0.5f;
    static inline float fWelcomeBannerFadeTime = 1.f;

    static inline uint16_t maxAnimLimitDefault = 0x7FFF;
    static inline uint16_t maxAnimLimitIncreased = 0xFFFE;
    static uint16_t GetMaxAnimLimit();
    static void ClampAnimLimit();

    constexpr static inline std::string_view iniPath = "Data/SKSE/Plugins/OpenAnimationReplacer.ini";
    constexpr static inline std::string_view imguiIni = "Data/SKSE/Plugins/OpenAnimationReplacer_ImGui.ini";
    constexpr static inline std::string_view animationFileHashCachePath = "Data/SKSE/Plugins/OpenAnimationReplacer_animFileHashCache.bin";
};
