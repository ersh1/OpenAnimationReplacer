#pragma once

#include "Conditions.h"
#include "ReplacementAnimation.h"
#include "Settings.h"

#include <future>

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

	ReplacementAnimData(std::string_view a_projectName, std::string_view a_path, bool a_bDisabled, std::optional<std::vector<Variant>>& a_variants, std::optional<ReplacementAnimation::VariantMode> a_variantMode, bool a_bLetReplaceOnLoop) :
		projectName(a_projectName),
		path(a_path),
		bDisabled(a_bDisabled),
		variants(std::move(a_variants)),
        variantMode(a_variantMode),
		bLetReplaceOnLoop(a_bLetReplaceOnLoop)
    {}

	ReplacementAnimData(const ReplacementAnimation* a_replacementAnimation) :
		projectName(a_replacementAnimation->GetProjectName()),
		path(a_replacementAnimation->GetAnimPath())
    {}

	std::string projectName;
	std::string path;
	bool bDisabled = false;
	std::optional<std::vector<Variant>> variants = std::nullopt;
	std::optional<ReplacementAnimation::VariantMode> variantMode = std::nullopt;
	bool bLetReplaceOnLoop = false;
};

namespace Parsing
{
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
		bool bKeepRandomResultsOnLoop = false;
		bool bShareRandomResults = false;
		std::unique_ptr<Conditions::ConditionSet> conditionSet;
		std::unique_ptr<Conditions::ConditionSet> synchronizedConditionSet;
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
		//ExclusiveLock modParseResultsLock;
		std::vector<std::future<ModParseResult>> modParseResultFutures;

		//ExclusiveLock legacyParseResultsLock;
		std::vector<std::future<SubModParseResult>> legacyParseResultFutures;
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

	void ParseDirectory(const std::filesystem::directory_entry& a_directory, ParseResults& a_outParseResults);
	[[nodiscard]] ModParseResult ParseModDirectory(const std::filesystem::directory_entry& a_directory);
	[[nodiscard]] SubModParseResult ParseModSubdirectory(const std::filesystem::directory_entry& a_subDirectory, bool a_bIsLegacy = false);
	[[nodiscard]] SubModParseResult ParseLegacyCustomConditionsDirectory(const std::filesystem::directory_entry& a_directory);
	[[nodiscard]] std::vector<SubModParseResult> ParseLegacyPluginDirectory(const std::filesystem::directory_entry& a_directory);
	[[nodiscard]] std::optional<ReplacementAnimationFile> ParseReplacementAnimationEntry(std::string_view a_fullPath);
	[[nodiscard]] std::optional<ReplacementAnimationFile> ParseReplacementAnimationVariants(std::string_view a_fullVariantsPath);
	[[nodiscard]] std::vector<ReplacementAnimationFile> ParseAnimationsInDirectory(const std::filesystem::directory_entry& a_directory, bool a_bIsLegacy = false);
}
