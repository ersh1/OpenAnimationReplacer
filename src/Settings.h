#pragma once

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
	static inline float fAnimationLogWidth = 650.f;
	static inline uint32_t uAnimationLogMaxEntries = 10;
	static inline uint32_t uAnimationActivateLogMode = 2;
	static inline uint32_t uAnimationEchoLogMode = 0;
	static inline uint32_t uAnimationLoopLogMode = 0;
	static inline bool bAnimationLogOnlyActiveGraph = true;
	static inline bool bAnimationLogWriteToTextLog = false;

	static inline bool bEnableAnimationEventLog = false;
	static inline bool bAnimationLogOrderDescending = true;
	static inline float fAnimationEventLogWidth = 500.f;
	static inline float fAnimationEventLogHeight = 600.f;
	static inline bool bAnimationEventLogWriteToTextLog = false;

	static inline float fAnimationLogsOffsetX = 0.f;
	static inline float fAnimationLogsOffsetY = 30.f;

	// Workarounds
	static inline bool bLegacyKeepRandomResultsByDefault = true;

	// Experimental
	static inline bool bDisablePreloading = false;
	static inline bool bIncreaseAnimationLimit = false;

	// Internal
	static inline float fDefaultBlendTimeOnInterrupt = 0.3f;
	static inline float fDefaultBlendTimeOnLoop = 0.3f;
	static inline float fDefaultBlendTimeOnEcho = 0.1f;
	static inline float fSharedRandomLifetime = 0.5f;
	static inline float fSequentialVariantLifetime = 0.5f;
	static inline float fQueueFadeTime = 1.f;
	static inline uint32_t uQueueMinSize = 10;
	static inline float fAnimationLogEntryFadeTime = 0.5f;
	static inline float fAnimationEventLogEntryColorTimeLong = 1.f;
	static inline float fWelcomeBannerFadeTime = 1.f;

	static inline uint16_t maxAnimLimitDefault = 0x7FFF;
	static inline uint16_t maxAnimLimitIncreased = 0xFFFE;
	static uint16_t GetMaxAnimLimit();
	static void ClampAnimLimit();

	constexpr static inline std::string_view iniPath = "Data/SKSE/Plugins/OpenAnimationReplacer.ini";
	constexpr static inline std::string_view imguiIni = "Data/SKSE/Plugins/OpenAnimationReplacer_ImGui.ini";
	constexpr static inline std::string_view animationFileHashCachePath = "Data/SKSE/Plugins/OpenAnimationReplacer_animFileHashCache.bin";

	constexpr static inline std::string_view synchronizedClipSourcePrefix = "NPC";
	constexpr static inline std::string_view synchronizedClipTargetPrefix = "2_";
};
