#include "Parsing.h"

#include <future>
#include <mmio/mmio.hpp>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>

#include "OpenAnimationReplacer.h"
#include "AnimationFileHashCache.h"
#include "Settings.h"

namespace Parsing
{
    ReplacementAnimationToAdd::ReplacementAnimationToAdd(std::string_view a_fullPath, std::string_view a_animationPath, uint16_t a_originalBindingIndex) :
        fullPath(a_fullPath),
        animationPath(a_animationPath),
        originalBindingIndex(a_originalBindingIndex)
    {
        if (Settings::bFilterOutDuplicateAnimations) {
            hash = AnimationFileHashCache::CalculateHash(*this);
        }
    }


    ConditionsTxtFile::ConditionsTxtFile(const std::filesystem::path& a_fileName) :
        file(a_fileName),
		filename(a_fileName.string())
    {
        if (!file.is_open()) {
            //util::report_and_fail("Error opening _conditions.txt file");
            logger::error("Error opening {} file", a_fileName.string());
        }
    }

    ConditionsTxtFile::~ConditionsTxtFile()
    {
        file.close();
    }

    std::unique_ptr<Conditions::ConditionSet> ConditionsTxtFile::GetConditions(std::string& a_currentLine, bool a_bInOrBlock /*= false*/)
    {
        auto conditions = std::make_unique<Conditions::ConditionSet>();

        do {
            if (file.fail()) {
                if (file.eof()) {
                    break;
                }
                //util::report_and_fail("Error reading from _conditions.txt file");
				logger::error("Error reading from {} file", filename);
                return std::move(conditions);
            }

            a_currentLine = Utils::TrimWhitespace(a_currentLine);
            if (!a_currentLine.empty()) {
                const bool bEndsWithOR = a_currentLine.ends_with("OR"sv);
                if (bEndsWithOR && !a_bInOrBlock) {
                    // start an OR block - create an OR condition and add all conditions to it until we reach AND
					auto orCondition = OpenAnimationReplacer::GetSingleton().CreateCondition("OR");
					static_cast<Conditions::ORCondition*>(orCondition.get())->conditionsComponent->conditionSet = GetConditions(a_currentLine, true);
                    conditions->AddCondition(orCondition);
                } else {
                    // create and add a new condition
                    if (auto newCondition = Conditions::CreateConditionFromString(a_currentLine)) {
                        conditions->AddCondition(newCondition);
                    }

                    if (!bEndsWithOR && a_bInOrBlock) {
                        // end an OR block
                        return std::move(conditions);
                    }
                }
            }
        } while (std::getline(file, a_currentLine));

        return std::move(conditions);
    }

    std::unique_ptr<Conditions::ConditionSet> ParseConditionsTxt(const std::filesystem::path& a_txtPath)
    {
        ConditionsTxtFile txt(a_txtPath);

        std::string line;
        std::getline(txt.file, line);

        return txt.GetConditions(line);
    }

    bool DeserializeMod(const std::filesystem::path& a_jsonPath, ModParseResult& a_outParseResult)
    {
        mmio::mapped_file_source file;
        if (file.open(a_jsonPath)) {
            rapidjson::StringStream stream{ reinterpret_cast<const char*>(file.data()) };

            rapidjson::Document doc;
            doc.ParseStream(stream);

            if (doc.HasParseError()) {
                logger::error("Failed to parse file: {}", a_jsonPath.string());
                return false;
            }

            // read mod name (required)
            if (const auto nameIt = doc.FindMember("name"); nameIt != doc.MemberEnd() && nameIt->value.IsString()) {
                a_outParseResult.name = nameIt->value.GetString();
            } else {
                logger::error("Failed to find mod name in file: {}", a_jsonPath.string());
                return false;
            }

            // read mod author (optional)
            if (const auto authorIt = doc.FindMember("author"); authorIt != doc.MemberEnd() && authorIt->value.IsString()) {
                a_outParseResult.author = authorIt->value.GetString();
            }

            // read mod description (optional)
            if (const auto descriptionIt = doc.FindMember("description"); descriptionIt != doc.MemberEnd() && descriptionIt->value.IsString()) {
                a_outParseResult.description = descriptionIt->value.GetString();
            }

            a_outParseResult.path = a_jsonPath.parent_path().string();
            a_outParseResult.bSuccess = true;

            return true;
        }

        logger::error("Failed to open file: {}", a_jsonPath.string());
        return false;
    }

    bool DeserializeSubMod(std::filesystem::path a_jsonPath, SubModParseResult& a_outParseResult)
    {
        mmio::mapped_file_source file;
        if (file.open(a_jsonPath)) {
            rapidjson::StringStream stream{ reinterpret_cast<const char*>(file.data()) };

            rapidjson::Document doc;
            doc.ParseStream(stream);

            if (doc.HasParseError()) {
                logger::error("Failed to parse file: {}", a_jsonPath.string());
                return false;
            }

            // read submod name (required)
            if (auto nameIt = doc.FindMember("name"); nameIt != doc.MemberEnd() && nameIt->value.IsString()) {
                a_outParseResult.name = nameIt->value.GetString();
            } else {
                logger::error("Failed to find mod name in file: {}", a_jsonPath.string());
                return false;
            }

            // read submod description (optional)
            if (auto descriptionIt = doc.FindMember("description"); descriptionIt != doc.MemberEnd() && descriptionIt->value.IsString()) {
                a_outParseResult.description = descriptionIt->value.GetString();
            }

            // read submod priority (required)
            if (auto priorityIt = doc.FindMember("priority"); priorityIt != doc.MemberEnd() && priorityIt->value.IsInt()) {
                a_outParseResult.priority = priorityIt->value.GetInt();
            } else {
                logger::error("Failed to find submod priority in file: {}", a_jsonPath.string());
                return false;
            }

            // read submod disabled (optional)
            if (auto disabledIt = doc.FindMember("disabled"); disabledIt != doc.MemberEnd() && disabledIt->value.IsBool()) {
                a_outParseResult.bDisabled = disabledIt->value.GetBool();
            }

            // read disabled animations (optional)
            if (auto disabledAnimationsIt = doc.FindMember("disabledAnimations"); disabledAnimationsIt != doc.MemberEnd() && disabledAnimationsIt->value.IsArray()) {
                for (auto& disabledAnimation : disabledAnimationsIt->value.GetArray()) {
                    if (disabledAnimation.IsObject()) {
                        auto projectNameIt = disabledAnimation.FindMember("projectName");
                        if (auto pathIt = disabledAnimation.FindMember("path"); projectNameIt != doc.MemberEnd() && projectNameIt->value.IsString() && pathIt != doc.MemberEnd() && pathIt->value.IsString()) {
                            a_outParseResult.disabledAnimations.emplace(projectNameIt->value.GetString(), pathIt->value.GetString());
                        }
                    }
                }
            }

            // read override animations folder (optional)
            if (auto overrideAnimationsFolderIt = doc.FindMember("overrideAnimationsFolder"); overrideAnimationsFolderIt != doc.MemberEnd() && overrideAnimationsFolderIt->value.IsString()) {
                a_outParseResult.overrideAnimationsFolder = overrideAnimationsFolderIt->value.GetString();
            }

            // read required project name (optional)
            if (auto requiredProjectNameIt = doc.FindMember("requiredProjectName"); requiredProjectNameIt != doc.MemberEnd() && requiredProjectNameIt->value.IsString()) {
                a_outParseResult.requiredProjectName = requiredProjectNameIt->value.GetString();
            }

            // read ignore no triggers flag (optional)
            if (auto ignoreNoTriggersIt = doc.FindMember("ignoreNoTriggersFlag"); ignoreNoTriggersIt != doc.MemberEnd() && ignoreNoTriggersIt->value.IsBool()) {
                a_outParseResult.bIgnoreNoTriggersFlag = ignoreNoTriggersIt->value.GetBool();
            }

            // read interruptible (optional)
            if (auto interruptibleIt = doc.FindMember("interruptible"); interruptibleIt != doc.MemberEnd() && interruptibleIt->value.IsBool()) {
                a_outParseResult.bInterruptible = interruptibleIt->value.GetBool();
            }

            // read keep random results on loop (optional)
            if (auto keepRandomIt = doc.FindMember("keepRandomResultsOnLoop"); keepRandomIt != doc.MemberEnd() && keepRandomIt->value.IsBool()) {
                a_outParseResult.bKeepRandomResultsOnLoop = keepRandomIt->value.GetBool();
            }

            // read conditions
            if (auto conditionsIt = doc.FindMember("conditions"); conditionsIt != doc.MemberEnd() && conditionsIt->value.IsArray()) {
                for (auto& conditionValue : conditionsIt->value.GetArray()) {
                    if (auto condition = Conditions::CreateConditionFromJson(conditionValue)) {
                        a_outParseResult.conditionSet->AddCondition(condition);
                    } else {
                        logger::error("Failed to parse condition in file: {}", a_jsonPath.string());

                        rapidjson::StringBuffer buffer;
                        rapidjson::PrettyWriter writer(buffer);
                        doc.Accept(writer);

                        logger::error("Dumping entire json file from memory: {}", buffer.GetString());

                        return false;
                    }
                }
            }

            a_outParseResult.path = a_jsonPath.parent_path().string();
            a_outParseResult.bSuccess = true;

            return true;
        }

        logger::error("Failed to open file: {}", a_jsonPath.string());
        return false;
    }

    bool SerializeJson(std::filesystem::path a_jsonPath, const rapidjson::Document& a_doc)
    {
        errno_t err = 0;
        const std::unique_ptr<FILE, decltype(&fclose)> fp{
            [&a_jsonPath, &err] {
                FILE* fp = nullptr;
                err = _wfopen_s(&fp, a_jsonPath.c_str(), L"w");
                return fp;
            }(),
            &fclose
        };

        if (err != 0) {
            logger::error("Failed to open file: {}", a_jsonPath.string());
            return false;
        }

        char writeBuffer[256]{};
        rapidjson::FileWriteStream os{ fp.get(), writeBuffer, sizeof(writeBuffer) };

        rapidjson::PrettyWriter writer(os);
        //writer.SetMaxDecimalPlaces(3);

        a_doc.Accept(writer);

        return true;
    }

    std::string SerializeJsonToString(const rapidjson::Document& a_doc)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter writer(buffer);
        //writer.SetMaxDecimalPlaces(3);

        a_doc.Accept(writer);

        return buffer.GetString();
    }

    std::string StripProjectPath(std::string_view a_path)
    {
        // strips the beginning of the path (Actors\Character\)
        constexpr auto rootPathEnd = "Animations\\";
        const auto rootPathEndPos = a_path.find(rootPathEnd);

        return a_path.substr(rootPathEndPos).data();
    }

    std::string StripReplacerPath(std::string_view a_path)
    {
        // strips the OAR/DAR substring ([Open/Dynamic]AnimationReplacer\subdirectory\subdirectory")
        constexpr auto separator = "\\";

        std::size_t substringStartPos = a_path.find("OpenAnimationReplacer"sv);
        if (substringStartPos == std::string::npos) {
            substringStartPos = a_path.find("DynamicAnimationReplacer"sv);
            if (substringStartPos == std::string::npos) {
                return a_path.data();
            }
        }

        std::size_t substringEndPos = substringStartPos + a_path.substr(substringStartPos).find(separator) + 1;
        substringEndPos = substringEndPos + a_path.substr(substringEndPos).find(separator) + 1;
        substringEndPos = substringEndPos + a_path.substr(substringEndPos).find(separator) + 1;

        std::string ret(a_path.substr(0, substringStartPos));
        ret.append(a_path.substr(substringEndPos));

        return ret;
    }

    std::string StripRandomPath(std::string_view a_path)
    {
        // strips the random substring ([_random\subdirectory\subdirectory")
        constexpr auto separator = "\\";

        const std::size_t substringStartPos = a_path.find("_random"sv);
        std::size_t substringEndPos = substringStartPos + a_path.substr(substringStartPos).find(separator) + 1;
        substringEndPos = substringEndPos + a_path.substr(substringEndPos).find(separator) + 1;
        substringEndPos = substringEndPos + a_path.substr(substringEndPos).find(separator) + 1;

        std::string ret(a_path.substr(0, substringStartPos));
        ret.append(a_path.substr(substringEndPos));

        return ret;
    }

    uint16_t GetOriginalAnimationBindingIndex(RE::hkbCharacterStringData* a_stringData, std::string_view a_animationName)
    {
        if (a_stringData) {
			auto& animationBundleNames = a_stringData->animationNames;
            if (!animationBundleNames.empty()) {
                for (uint16_t id = 0; id < animationBundleNames.size(); ++id) {
                    if (Utils::CompareStringsIgnoreCase(animationBundleNames[id].data(), a_animationName)) {
                        return id;
                    }
                }
            }
        }

        return static_cast<uint16_t>(-1);
    }

    ModParseResult ParseModDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData)
    {
        ModParseResult result;

        // check whether the config json file exists first
        const auto jsonPath = a_directory.path() / "config.json"sv;

        if (is_regular_file(jsonPath)) {
            // parse the config json file
            if (DeserializeMod(jsonPath, result)) {
                // parse the subfolders
                if (Settings::bAsyncParsing) {
                    std::vector<std::future<SubModParseResult>> futures;
                    for (const auto& entry : std::filesystem::directory_iterator(a_directory)) {
                        if (is_directory(entry)) {
                            // we're in a mod subfolder. we have the animations here and a json.
                            futures.emplace_back(std::async(std::launch::async, ParseModSubdirectory, entry, a_stringData));
                        }
                    }

                    for (auto& future : futures) {
                        auto subModParseResult = future.get();
                        if (subModParseResult.bSuccess) {
                            result.subModParseResults.emplace_back(std::move(subModParseResult));
                        } /* else {
					result.bSuccess = false;
				    }*/
                    }
                } else {
                    for (const auto& entry : std::filesystem::directory_iterator(a_directory)) {
                        if (is_directory(entry)) {
                            // we're in a mod subfolder. we have the animations here and a json.
                            auto subModParseResult = ParseModSubdirectory(entry, a_stringData);
                            if (subModParseResult.bSuccess) {
								result.subModParseResults.emplace_back(std::move(subModParseResult));
                            }
                        }
                    }
                }
            }
        }

        return result;
    }

    SubModParseResult ParseModSubdirectory(const std::filesystem::directory_entry& a_subDirectory, RE::hkbCharacterStringData* a_stringData)
    {
        SubModParseResult result;

        // check whether the user json file exists first
        auto jsonPath = a_subDirectory.path() / "user.json"sv;
        if (is_regular_file(jsonPath)) {
            // check whether there's any other file there (except for the user.json file), so we know it's not just a remnant of an uninstalled mod
            // not perfect but eh, haven't got a better idea
            const auto fileCount = std::distance(std::filesystem::directory_iterator(a_subDirectory), std::filesystem::directory_iterator{});
            if (fileCount <= 1) {
                result.bSuccess = false;
                return result;
            }

            result.configSource = ConfigSource::kUser;
        } else {
            // try the config.json file instead
            jsonPath = a_subDirectory.path() / "config.json"sv;
            result.configSource = ConfigSource::kAuthor;
        }

        if (is_regular_file(jsonPath)) {
            // parse the config json file
            if (DeserializeSubMod(jsonPath, result)) {
                // check required project name
                if (result.requiredProjectName.empty() || result.requiredProjectName == a_stringData->name.data()) {
                    if (!result.overrideAnimationsFolder.empty()) {
                        const auto overridePath = a_subDirectory.path().parent_path() / result.overrideAnimationsFolder;
                        const auto overrideDirectory = std::filesystem::directory_entry(overridePath);
                        if (is_directory(overrideDirectory)) {
                            result.animationsToAdd = ParseAnimationsInDirectory(overrideDirectory, a_stringData);
                        } else {
                            result.bSuccess = false;
                        }
                    } else {
                        result.animationsToAdd = ParseAnimationsInDirectory(a_subDirectory, a_stringData);
                    }
                } else {
                    result.bSuccess = false;
                }
            }
        }

        return result;
    }

    SubModParseResult ParseLegacyCustomConditionsDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData)
    {
        SubModParseResult result;

        // check whether _conditions.txt file exists first
        const std::filesystem::path txtPath = a_directory.path() / "_conditions.txt"sv;
        if (exists(txtPath)) {
            // check whether the user json file exists, if yes, treat it as a OAR submod
            const auto jsonPath = a_directory.path() / "user.json"sv;
            if (is_regular_file(jsonPath)) {
                return ParseModSubdirectory(a_directory, a_stringData);
            }
        }

        int32_t priority = 0;
        std::string directoryName;
        try {
            directoryName = a_directory.path().filename().string();
        } catch (const std::system_error&) {
            const auto path = a_directory.path().u8string();
            std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
            logger::warn("invalid directory name at {}, skipping", pathSv);
            return result;
        }
        auto priorityStringEnd = directoryName.find_first_not_of("-0123456789"sv);
        if (priorityStringEnd == std::string::npos) {
            priorityStringEnd = directoryName.size();
        }
        auto [ptr, ec]{ std::from_chars(directoryName.data(), directoryName.data() + priorityStringEnd, priority) };
        if (ec == std::errc()) {
            if (exists(txtPath)) {
                result.configSource = ConfigSource::kLegacy;
                result.name = std::to_string(priority);
                result.priority = priority;
				result.conditionSet = ParseConditionsTxt(txtPath); // parse conditions.txt
                result.animationsToAdd = ParseAnimationsInDirectory(a_directory, a_stringData);
                result.bSuccess = true;
            } else {
                const auto subEntryPath = a_directory.path().u8string();
                std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
                logger::warn("directory at {} is missing the _conditions.txt file, skipping", subEntryPathSv);
            }
        } else {
            const auto subEntryPath = a_directory.path().u8string();
            std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
            logger::warn("invalid directory name at {}, skipping", subEntryPathSv);
        }

        result.path = a_directory.path().string();

        return result;
    }

    std::vector<SubModParseResult> ParseLegacyPluginDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData)
    {
        std::vector<SubModParseResult> results;

        for (const auto& subEntry : std::filesystem::directory_iterator(a_directory)) {
            if (is_directory(subEntry)) {
                // check whether there's any file here
                if (is_empty(subEntry)) {
                    continue;
                }

                // check whether the user json file exists, if yes, treat it as a OAR submod
                auto jsonPath = subEntry.path() / "user.json"sv;
                if (is_regular_file(jsonPath)) {
                    results.emplace_back(ParseModSubdirectory(subEntry, a_stringData));
                    continue;
                }

                RE::FormID formID;
                std::string directoryName;
                try {
                    directoryName = subEntry.path().filename().string();
                } catch (const std::system_error&) {
                    auto path = subEntry.path().u8string();
                    std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
                    logger::warn("invalid directory name at {}, skipping", pathSv);
                    continue;
                }
                auto [ptr, ec]{ std::from_chars(directoryName.data(), directoryName.data() + directoryName.size(), formID, 16) };
                if (ec == std::errc()) {
                    std::string fileString, extensionString;
                    try {
                        fileString = a_directory.path().stem().string();
                        extensionString = a_directory.path().extension().string();
                    } catch (const std::system_error&) {
                        auto path = a_directory.path().u8string();
                        std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
                        logger::warn("invalid directory name at {}, skipping", pathSv);
                        continue;
                    }

                    std::string modName = fileString + extensionString;
                    if (auto form = Utils::LookupForm(formID, modName)) {
                        auto conditionSet = std::make_unique<Conditions::ConditionSet>();
						std::string argument = modName;
						argument += '|';
						argument += directoryName;
						auto condition = OpenAnimationReplacer::GetSingleton().CreateCondition("IsActorBase");
						static_cast<Conditions::IsActorBaseCondition*>(condition.get())->formComponent->SetTESFormValue(form);
                        conditionSet->AddCondition(condition);

                        SubModParseResult result;

                        result.configSource = ConfigSource::kLegacyActorBase;
                        result.name = argument;
                        result.priority = 0;
                        result.conditionSet = std::move(conditionSet);
                        result.animationsToAdd = ParseAnimationsInDirectory(subEntry, a_stringData);
                        result.bSuccess = true;
                        result.path = subEntry.path().string();

                        results.emplace_back(std::move(result));
                    }
                } else {
                    auto subEntryPath = subEntry.path().u8string();
                    std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
                    logger::warn("invalid directory name at {}, skipping", subEntryPathSv);
                }
            }
        }

        return results;
    }

    std::optional<ReplacementAnimationToAdd> ParseReplacementAnimationEntry(RE::hkbCharacterStringData* a_stringData, std::string_view a_animationPath)
    {
        const std::string replacementAnimationPath = StripProjectPath(a_animationPath);
        const std::string originalAnimationPath = StripReplacerPath(replacementAnimationPath);

        const auto originalIndex = GetOriginalAnimationBindingIndex(a_stringData, originalAnimationPath);
        if (originalIndex != static_cast<uint16_t>(-1)) {
            return ReplacementAnimationToAdd(a_animationPath, replacementAnimationPath, originalIndex);
        }

        return std::nullopt;
    }

    std::vector<ReplacementAnimationToAdd> ParseAnimationsInDirectory(const std::filesystem::directory_entry& a_directory, RE::hkbCharacterStringData* a_stringData)
    {
        std::vector<ReplacementAnimationToAdd> result;

        for (const auto& fileEntry : std::filesystem::recursive_directory_iterator(a_directory)) {
            std::string extensionString;
            try {
                extensionString = fileEntry.path().extension().string();
            } catch (const std::system_error&) {
                auto path = fileEntry.path().u8string();
                std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
                logger::warn("invalid filename at {}, skipping", pathSv);
                continue;
            }

            if (is_regular_file(fileEntry) && Utils::CompareStringsIgnoreCase(extensionString, ".hkx"sv)) {
                std::string fileEntryPath;
                try {
                    fileEntryPath = fileEntry.path().string();
                } catch (const std::system_error&) {
                    auto path = a_directory.path().u8string();
                    std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
                    logger::warn("invalid filename at {}, skipping", pathSv);
                    continue;
                }
                if (auto anim = ParseReplacementAnimationEntry(a_stringData, fileEntryPath)) {
                    result.emplace_back(*anim);
                }
            }
        }

        return result;
    }
}
