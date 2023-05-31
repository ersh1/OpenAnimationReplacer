#pragma once

#include "Conditions.h"
#include "ReplacementAnimation.h"

struct DisabledAnimation
{
    DisabledAnimation(std::string_view a_projectName, std::string_view a_path) :
        projectName(a_projectName),
        path(a_path) {}

    DisabledAnimation(ReplacementAnimation* a_replacementAnimation) :
        projectName(a_replacementAnimation->GetProjectName()),
        path(a_replacementAnimation->GetAnimPath()) {}

    // required for using DisabledAnimation in a std:set
    bool operator<(const DisabledAnimation& a_rhs) const
    {
        if (projectName < a_rhs.projectName) {
            return true;
        }
        if (projectName == a_rhs.projectName && path < a_rhs.path) {
            return true;
        }

        return false;
    }

    std::string projectName;
    std::string path;
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
        ReplacementAnimationToAdd(std::string_view a_fullPath, std::string_view a_animationPath, uint16_t a_originalBindingIndex);

        std::string fullPath;
        std::string animationPath;
        uint16_t originalBindingIndex;
        std::optional<uint16_t> replacementIndex = std::nullopt;
        std::optional<std::string> hash = std::nullopt;
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
        std::set<DisabledAnimation> disabledAnimations{};
        std::string overrideAnimationsFolder;
        std::string requiredProjectName;
        bool bIgnoreNoTriggersFlag = false;
        bool bInterruptible = false;
        bool bKeepRandomResultsOnLoop = false;
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
    [[nodiscard]] std::string StripRandomPath(std::string_view a_path);

    [[nodiscard]] uint16_t GetOriginalAnimationBindingIndex(RE::hkbCharacterStringData* a_stringData, std::string_view a_animationName);

    [[nodiscard]] ModParseResult ParseModDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData);
    [[nodiscard]] SubModParseResult ParseModSubdirectory(const std::filesystem::directory_entry& a_subDirectory, RE::hkbCharacterStringData* a_stringData, bool a_bUserOnly = false);
    [[nodiscard]] SubModParseResult ParseLegacyCustomConditionsDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData);
    [[nodiscard]] std::vector<SubModParseResult> ParseLegacyPluginDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData);
    [[nodiscard]] std::optional<ReplacementAnimationToAdd> ParseReplacementAnimationEntry(RE::hkbCharacterStringData* a_stringData, std::string_view a_animationPath);
    [[nodiscard]] std::vector<ReplacementAnimationToAdd> ParseAnimationsInDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData);
}
