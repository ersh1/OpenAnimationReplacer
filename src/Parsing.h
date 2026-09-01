#pragma once

#include "Conditions.h"
#include "Functions.h"
#include "ReplacementAnimation.h"
#include "Settings.h"

#include <atomic>
#include <chrono>
#include <future>
#include <shared_mutex>

struct ReplacementAnimData
{
	struct Variant
	{
		Variant(std::string_view a_filename, bool a_bDisabled, float a_weight, int32_t a_order, bool a_bPlayOnce) :
			filename(a_filename),
			bDisabled(a_bDisabled),
			weight(a_weight),
			order(a_order),
			bPlayOnce(a_bPlayOnce)
		{}

		std::string filename;
		bool bDisabled = false;
		float weight = 1.f;
		int32_t order = -1;
		bool bPlayOnce = false;
	};

	ReplacementAnimData(std::string_view a_projectName, std::string_view a_path, bool a_bDisabled) :
		projectName(a_projectName),
		path(a_path),
		bDisabled(a_bDisabled)
	{}

	ReplacementAnimData(std::string_view a_projectName, std::string_view a_path, bool a_bDisabled, std::optional<std::vector<Variant>>& a_variants, std::optional<VariantMode> a_variantMode, std::optional<Conditions::StateDataScope> a_variantStateScope, bool a_bBlendBetweenVariants, bool a_bResetRandomOnLoopOrEcho, bool a_bSharePlayedHistory) :
		projectName(a_projectName),
		path(a_path),
		bDisabled(a_bDisabled),
		variants(std::move(a_variants)),
		variantMode(a_variantMode),
		variantStateScope(a_variantStateScope),
		bBlendBetweenVariants(a_bBlendBetweenVariants),
		bResetRandomOnLoopOrEcho(a_bResetRandomOnLoopOrEcho),
		bSharePlayedHistory(a_bSharePlayedHistory)
	{}

	std::string projectName;
	std::string path;
	bool bDisabled = false;
	std::optional<std::vector<Variant>> variants = std::nullopt;
	std::optional<VariantMode> variantMode = std::nullopt;
	std::optional<Conditions::StateDataScope> variantStateScope = std::nullopt;
	bool bBlendBetweenVariants = true;
	bool bResetRandomOnLoopOrEcho = true;
	bool bSharePlayedHistory = false;
};

namespace Parsing
{
	inline constexpr bool bEnableParseTiming = false;

	enum class TimingBucket : uint8_t
	{
		kModJson,
		kSubModJson,
		kConditionsTxt,
		kAnimationDirectoryScan,
		kAnimationFileHash,
		kSetAnimationFiles,
		kCacheAnimationPathSubMods,
		kTotal
	};

	enum class TimingCounter : uint8_t
	{
		kAnimationHashCalculated,
		kAnimationHashCacheHit,
		kAnimationHashFailed,
		kDirectoryEntriesSeen,
		kDirectoryDirectoriesSeen,
		kDirectoryFilesSeen,
		kDirectoryHkxFilesFound,
		kDirectoryInvalidPaths,
		kDirectoryHiddenRecursionSkips,
		kTotal
	};

	void ResetTimingStats();
	void LogTimingStats();
	void AddTiming(TimingBucket a_bucket, std::chrono::nanoseconds a_duration);
	void AddTimingCounter(TimingCounter a_counter);

	class ScopedTimer
	{
	public:
		explicit ScopedTimer(TimingBucket a_bucket) :
			_bucket(a_bucket)
		{
			if constexpr (bEnableParseTiming) {
				_startTime = std::chrono::steady_clock::now();
			}
		}

		ScopedTimer(const ScopedTimer&) = delete;
		ScopedTimer& operator=(const ScopedTimer&) = delete;

		~ScopedTimer()
		{
			if constexpr (bEnableParseTiming) {
				AddTiming(_bucket, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - _startTime));
			}
		}

	private:
		TimingBucket _bucket;
		std::chrono::steady_clock::time_point _startTime;
	};

	enum class ConfigSource : uint8_t
	{
		kAuthor = 0,
		kUser,
		kLegacy,
		kLegacyActorBase
	};

	enum class DeserializeMode : uint8_t
	{
		kFull = 0,
		kInfoOnly,
		kDataOnly
	};

	struct ConditionsTxtFile
	{
	public:
		ConditionsTxtFile(const std::filesystem::path& a_fileName);
		~ConditionsTxtFile();

		std::unique_ptr<Conditions::ConditionSet> GetConditions(std::string& a_currentLine, bool a_bInOrBlock = false);

		std::ifstream file;
		std::string filename;
	};

	struct SubModParseResult
	{
		SubModParseResult()
		{
			conditionSet = std::make_unique<Conditions::ConditionSet>();
		}

		bool bSuccess = false;

		std::string path;
		std::string name;
		std::string description;
		int32_t priority = 0;
		bool bDisabled = false;
		std::vector<ReplacementAnimData> replacementAnimDatas{};
		std::string overrideAnimationsFolder;
		std::string requiredProjectName;
		bool bIgnoreDontConvertAnnotationsToTriggersFlag = false;
		bool bTriggersFromAnnotationsOnly = false;
		bool bInterruptible = false;
		bool bCustomBlendTimeOnInterrupt = false;
		float blendTimeOnInterrupt = Settings::fDefaultBlendTimeOnInterrupt;
		bool bReplaceOnLoop = true;
		bool bCustomBlendTimeOnLoop = false;
		float blendTimeOnLoop = Settings::fDefaultBlendTimeOnLoop;
		bool bReplaceOnEcho = false;
		bool bCustomBlendTimeOnEcho = false;
		float blendTimeOnEcho = Settings::fDefaultBlendTimeOnEcho;
		bool bKeepRandomResultsOnLoop_DEPRECATED = false;
		bool bShareRandomResults_DEPRECATED = false;
		bool bRunFunctionsOnLoop = true;
		bool bRunFunctionsOnEcho = true;
		std::unique_ptr<Conditions::ConditionSet> conditionSet;
		std::unique_ptr<Conditions::ConditionSet> synchronizedConditionSet;
		std::unique_ptr<Functions::FunctionSet> functionSetOnActivate;
		std::unique_ptr<Functions::FunctionSet> functionSetOnDeactivate;
		std::unique_ptr<Functions::FunctionSet> functionSetOnTrigger;
		std::vector<ReplacementAnimationFile> animationFiles;

		ConfigSource configSource = ConfigSource::kAuthor;
	};

	struct ModParseResult
	{
		bool bSuccess = false;

		std::vector<SubModParseResult> subModParseResults;

		std::string path;
		std::string name;
		std::string author;
		std::string description;

		std::vector<std::unique_ptr<Conditions::ConditionPreset>> conditionPresets;

		ConfigSource configSource = ConfigSource::kAuthor;
	};

	struct ParseResults
	{
		std::vector<std::future<ModParseResult>> modParseResultFutures;
		std::vector<std::future<SubModParseResult>> legacyParseResultFutures;
		std::chrono::milliseconds waitCacheDuration{ 0 };
	};

	[[nodiscard]] std::unique_ptr<Conditions::ConditionSet> ParseConditionsTxt(const std::filesystem::path& a_txtPath);
	[[nodiscard]] bool DeserializeMod(const std::filesystem::path& a_jsonPath, DeserializeMode a_deserializeMode, ModParseResult& a_outParseResult);
	[[nodiscard]] bool DeserializeSubMod(std::filesystem::path a_jsonPath, DeserializeMode a_deserializeMode, SubModParseResult& a_outParseResult);
	bool SerializeJson(std::filesystem::path a_jsonPath, const rapidjson::Document& a_doc);
	[[nodiscard]] std::string SerializeJsonToString(const rapidjson::Document& a_doc);

	[[nodiscard]] std::string StripProjectPath(std::string_view a_path);
	[[nodiscard]] std::string StripReplacerPath(std::string_view a_path);
	[[nodiscard]] std::string ConvertVariantsPath(std::string_view a_path);

	[[nodiscard]] uint16_t GetOriginalAnimationBindingIndex(RE::hkbCharacterStringData* a_stringData, std::string_view a_animationName);

	struct CachedDirectoryEntry
	{
		std::filesystem::path path;
		bool isDirectory = false;
	};

	struct CachedAnimationFile
	{
		std::filesystem::path path;
		bool isVariantsDirectory = false;
		std::vector<std::filesystem::path> variantPaths;
	};

	struct CachedSubModDirectory
	{
		std::filesystem::path path;
		std::vector<CachedAnimationFile> animationFiles;
		std::vector<CachedSubModDirectory> subdirectories;
	};

	struct CachedModDirectory
	{
		std::filesystem::path path;
		std::vector<CachedSubModDirectory> subModDirectories;
	};

	struct CachedOARDirectory
	{
		std::filesystem::path path;
		std::vector<CachedModDirectory> modDirectories;
	};

	struct CachedLegacySubMod
	{
		std::filesystem::path path;
		std::vector<CachedAnimationFile> animationFiles;
	};

	struct CachedLegacyEntry
	{
		std::filesystem::path path;
		bool isCustomConditions = false;
		std::vector<CachedLegacySubMod> subMods;
	};

	struct CachedLegacyDirectory
	{
		std::filesystem::path path;
		std::vector<CachedDirectoryEntry> entries;
		std::vector<CachedLegacyEntry> detailedEntries;
	};

	struct DirectoryCache
	{
		std::vector<CachedOARDirectory> oarDirectories;
		std::vector<CachedLegacyDirectory> legacyDirectories;
		std::atomic<bool> bIsComplete{ false };
		mutable std::shared_mutex cacheLock;
	};

	void ParseDirectory(const std::filesystem::directory_entry& a_directory, ParseResults& a_outParseResults);
	[[nodiscard]] ModParseResult ParseModDirectory(const std::filesystem::directory_entry& a_directory);
	[[nodiscard]] ModParseResult ParseModDirectory(const CachedModDirectory& a_cachedMod);
	[[nodiscard]] SubModParseResult ParseModSubdirectory(const std::filesystem::directory_entry& a_subDirectory, bool a_bIsLegacy = false);
	[[nodiscard]] SubModParseResult ParseModSubdirectory(const CachedSubModDirectory& a_cachedSubMod, bool a_bIsLegacy = false);
	[[nodiscard]] SubModParseResult ParseLegacyCustomConditionsDirectory(const std::filesystem::directory_entry& a_directory);
	[[nodiscard]] SubModParseResult ParseLegacyCustomConditionsDirectory(const CachedLegacySubMod& a_cachedSubMod);
	[[nodiscard]] std::vector<SubModParseResult> ParseLegacyPluginDirectory(const std::filesystem::directory_entry& a_directory);
	[[nodiscard]] std::vector<SubModParseResult> ParseLegacyPluginDirectory(const CachedLegacyEntry& a_cachedEntry);
	[[nodiscard]] std::optional<ReplacementAnimationFile> ParseReplacementAnimationEntry(std::string_view a_fullPath);
	[[nodiscard]] std::optional<ReplacementAnimationFile> ParseReplacementAnimationVariants(std::string_view a_fullVariantsPath);
	[[nodiscard]] std::optional<ReplacementAnimationFile> ParseReplacementAnimationVariants(const CachedAnimationFile& a_cachedVariants);
	[[nodiscard]] std::vector<ReplacementAnimationFile> ParseAnimationsInDirectory(const std::filesystem::directory_entry& a_directory, bool a_bIsLegacy = false);
	[[nodiscard]] std::vector<ReplacementAnimationFile> ParseAnimationsFromCache(const CachedSubModDirectory& a_cachedSubMod, bool a_bIsLegacy = false);

	[[nodiscard]] bool IsPathValid(const std::filesystem::path& a_path);
	void StartDirectoryCaching();
}
