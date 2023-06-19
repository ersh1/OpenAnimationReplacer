#pragma once

#include "Conditions.h"
#include "ReplacementAnimation.h"

struct ReplacementAnimData
{
	struct Variant
	{
		Variant(std::string_view a_filename) :
			filename(a_filename) {}

		Variant(std::string_view a_filename, float a_weight) :
			filename(a_filename),
			weight(a_weight) {}

		Variant(std::string_view a_filename, bool a_bDisabled) :
			filename(a_filename),
			bDisabled(a_bDisabled) {}

		Variant(std::string_view a_filename, float a_weight, bool a_bDisabled) :
			filename(a_filename),
		    weight(a_weight),
		    bDisabled(a_bDisabled) {}

		std::string filename;
		float weight = 1.f;
	    bool bDisabled = false;
	};

	ReplacementAnimData(std::string_view a_projectName, std::string_view a_path, bool a_bDisabled) :
        projectName(a_projectName),
        path(a_path),
        bDisabled(a_bDisabled) {}

	ReplacementAnimData(std::string_view a_projectName, std::string_view a_path, bool a_bDisabled, std::optional<std::vector<Variant>>& a_variants) :
		projectName(a_projectName),
		path(a_path),
		bDisabled(a_bDisabled),
        variants(std::move(a_variants)) {}

	ReplacementAnimData(const ReplacementAnimation* a_replacementAnimation) :
		projectName(a_replacementAnimation->GetProjectName()),
		path(a_replacementAnimation->GetAnimPath()) {}

    std::string projectName;
    std::string path;
	bool bDisabled = false;
	std::optional<std::vector<Variant>> variants = std::nullopt;
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
        kNameDescriptionOnly,
        kWithoutNameDescription
    };

    struct ReplacementAnimationToAdd
    {
		struct VariantToAdd
		{
			VariantToAdd(std::string_view a_fullPath) :
				fullPath(a_fullPath) {}

			std::string fullPath;
			std::optional<std::string> hash = std::nullopt;
		};

        ReplacementAnimationToAdd(std::string_view a_fullPath, std::string_view a_animationPath, uint16_t a_originalBindingIndex);
		ReplacementAnimationToAdd(std::string_view a_fullPath, std::string_view a_animationPath, uint16_t a_originalBindingIndex, std::vector<VariantToAdd>& a_variants);

        std::string fullPath;
        std::string animationPath;
        uint16_t originalBindingIndex;
        std::optional<std::string> hash = std::nullopt;
		std::optional<std::vector<VariantToAdd>> variants = std::nullopt;
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
        bool bIgnoreNoTriggersFlag = false;
        bool bInterruptible = false;
		bool bReplaceOnLoop = true;
		bool bReplaceOnEcho = false;
        bool bKeepRandomResultsOnLoop = false;
		bool bShareRandomResults = false;
        std::unique_ptr<Conditions::ConditionSet> conditionSet;
        std::vector<ReplacementAnimationToAdd> animationsToAdd;

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
    };

    [[nodiscard]] std::unique_ptr<Conditions::ConditionSet> ParseConditionsTxt(const std::filesystem::path& a_txtPath);
    [[nodiscard]] bool DeserializeMod(const std::filesystem::path& a_jsonPath, ModParseResult& a_outParseResult);
    [[nodiscard]] bool DeserializeSubMod(std::filesystem::path a_jsonPath, DeserializeMode a_deserializeMode, SubModParseResult& a_outParseResult);
    bool SerializeJson(std::filesystem::path a_jsonPath, const rapidjson::Document& a_doc);
    [[nodiscard]] std::string SerializeJsonToString(const rapidjson::Document& a_doc);

    [[nodiscard]] std::string StripProjectPath(std::string_view a_path);
    [[nodiscard]] std::string StripReplacerPath(std::string_view a_path);
    [[nodiscard]] std::string ConvertVariantsPath(std::string_view a_path);

    [[nodiscard]] uint16_t GetOriginalAnimationBindingIndex(RE::hkbCharacterStringData* a_stringData, std::string_view a_animationName);

    [[nodiscard]] ModParseResult ParseModDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData);
    [[nodiscard]] SubModParseResult ParseModSubdirectory(const std::filesystem::directory_entry& a_subDirectory, RE::hkbCharacterStringData* a_stringData, bool a_bIsLegacy = false);
    [[nodiscard]] SubModParseResult ParseLegacyCustomConditionsDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData);
    [[nodiscard]] std::vector<SubModParseResult> ParseLegacyPluginDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData);
    [[nodiscard]] std::optional<ReplacementAnimationToAdd> ParseReplacementAnimationEntry(RE::hkbCharacterStringData* a_stringData, std::string_view a_fullPath);
	[[nodiscard]] std::optional<ReplacementAnimationToAdd> ParseReplacementAnimationVariants(RE::hkbCharacterStringData* a_stringData, std::string_view a_fullVariantsPath);
    [[nodiscard]] std::vector<ReplacementAnimationToAdd> ParseAnimationsInDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData, bool a_bIsLegacy = false);
}
