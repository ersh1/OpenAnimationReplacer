#include "Parsing.h"

#include <future>
#include <mmio/mmio.hpp>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>

#include "OpenAnimationReplacer.h"
#include "Settings.h"

namespace Parsing
{
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

						if (!bEndsWithOR && a_bInOrBlock) {
							// end an OR block
							return std::move(conditions);
						}
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
            //rapidjson::StringStream stream{ reinterpret_cast<const char*>(file.data()) };
			rapidjson::MemoryStream stream{ reinterpret_cast<const char*>(file.data()), file.size() };

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

    bool DeserializeSubMod(std::filesystem::path a_jsonPath, DeserializeMode a_deserializeMode, SubModParseResult& a_outParseResult)
    {
        mmio::mapped_file_source file;
        if (file.open(a_jsonPath)) {
            //rapidjson::StringStream stream{ reinterpret_cast<const char*>(file.data()) };
            rapidjson::MemoryStream stream{ reinterpret_cast<const char*>(file.data()), file.size() };

            rapidjson::Document doc;
            doc.ParseStream(stream);

            if (doc.HasParseError()) {
                logger::error("Failed to parse file: {}", a_jsonPath.string());
                return false;
            }

            if (a_deserializeMode != DeserializeMode::kWithoutNameDescription) {
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
            }

            if (a_deserializeMode == DeserializeMode::kNameDescriptionOnly) {
                // we're only here to get the name and description, so we're done
                return true;
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

            // read disabled animations (optional, json field deprecated and replaced by replacementAnimDatas - reading it kept for compatibility with older config versions)
            if (auto disabledAnimationsIt = doc.FindMember("disabledAnimations"); disabledAnimationsIt != doc.MemberEnd() && disabledAnimationsIt->value.IsArray()) {
                for (auto& disabledAnimation : disabledAnimationsIt->value.GetArray()) {
                    if (disabledAnimation.IsObject()) {
                        auto projectNameIt = disabledAnimation.FindMember("projectName");
                        if (auto pathIt = disabledAnimation.FindMember("path"); projectNameIt != doc.MemberEnd() && projectNameIt->value.IsString() && pathIt != doc.MemberEnd() && pathIt->value.IsString()) {
                            a_outParseResult.replacementAnimDatas.emplace_back(projectNameIt->value.GetString(), pathIt->value.GetString(), true);
                        }
                    }
                }
            }

			// read replacement animation datas (optional)
			if (auto replacementAnimDatasIt = doc.FindMember("replacementAnimDatas"); replacementAnimDatasIt != doc.MemberEnd() && replacementAnimDatasIt->value.IsArray()) {
				for (auto& replacementAnimData : replacementAnimDatasIt->value.GetArray()) {
					if (replacementAnimData.IsObject()) {
						auto projectNameIt = replacementAnimData.FindMember("projectName");
						if (auto pathIt = replacementAnimData.FindMember("path"); projectNameIt != doc.MemberEnd() && projectNameIt->value.IsString() && pathIt != doc.MemberEnd() && pathIt->value.IsString()) {
							bool bDisabled = false;
							if (auto disabledIt = replacementAnimData.FindMember("disabled"); disabledIt != doc.MemberEnd() && disabledIt->value.IsBool()) {
								bDisabled = disabledIt->value.GetBool();
							}

							// read replacement animation variants
							std::optional<std::vector<ReplacementAnimData::Variant>> variants = std::nullopt;
							if (auto variantsIt = replacementAnimData.FindMember("variants"); variantsIt != doc.MemberEnd() && variantsIt->value.IsArray()) {
								for (auto& variantObj : variantsIt->value.GetArray()) {
									if (variantObj.IsObject()) {
										if (auto variantFilenameIt = variantObj.FindMember("filename"); variantFilenameIt != doc.MemberEnd() && variantFilenameIt->value.IsString()) {
											ReplacementAnimData::Variant variant(variantFilenameIt->value.GetString());

											if (auto weightIt = variantObj.FindMember("weight"); weightIt != doc.MemberEnd() && weightIt->value.IsNumber()) {
												variant.weight = weightIt->value.GetFloat();
											}
											if (auto variantDisabledIt = variantObj.FindMember("disabled"); variantDisabledIt != doc.MemberEnd() && variantDisabledIt->value.IsBool()) {
												variant.bDisabled = variantDisabledIt->value.GetBool();
											}

											if (!variants.has_value()) {
												variants.emplace();
											}
											
										    variants->emplace_back(variant);
										}
									}
								}
							}

							a_outParseResult.replacementAnimDatas.emplace_back(projectNameIt->value.GetString(), pathIt->value.GetString(), bDisabled, variants);
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
			if (auto ignoreDontConvertAnnotationsToTriggersFlagIt = doc.FindMember("ignoreDontConvertAnnotationsToTriggersFlag"); ignoreDontConvertAnnotationsToTriggersFlagIt != doc.MemberEnd() && ignoreDontConvertAnnotationsToTriggersFlagIt->value.IsBool()) {
				a_outParseResult.bIgnoreDontConvertAnnotationsToTriggersFlag = ignoreDontConvertAnnotationsToTriggersFlagIt->value.GetBool();
			} else if (auto ignoreNoTriggersIt = doc.FindMember("ignoreNoTriggersFlag"); ignoreNoTriggersIt != doc.MemberEnd() && ignoreNoTriggersIt->value.IsBool()) {  // old name
				a_outParseResult.bIgnoreDontConvertAnnotationsToTriggersFlag = ignoreNoTriggersIt->value.GetBool();
            }

			// read triggersOnly (optional)
			if (auto triggersFromAnnotationsOnlyIt = doc.FindMember("triggersFromAnnotationsOnly"); triggersFromAnnotationsOnlyIt != doc.MemberEnd() && triggersFromAnnotationsOnlyIt->value.IsBool()) {
				a_outParseResult.bTriggersFromAnnotationsOnly = triggersFromAnnotationsOnlyIt->value.GetBool();
			}

            // read interruptible (optional)
            if (auto interruptibleIt = doc.FindMember("interruptible"); interruptibleIt != doc.MemberEnd() && interruptibleIt->value.IsBool()) {
                a_outParseResult.bInterruptible = interruptibleIt->value.GetBool();
            }

			// read replace on loop (optional)
			if (auto replaceOnLoopIt = doc.FindMember("replaceOnLoop"); replaceOnLoopIt != doc.MemberEnd() && replaceOnLoopIt->value.IsBool()) {
				a_outParseResult.bReplaceOnLoop = replaceOnLoopIt->value.GetBool();
			}

			// read replace on echo (optional)
			if (auto replaceOnEchoIt = doc.FindMember("replaceOnEcho"); replaceOnEchoIt != doc.MemberEnd() && replaceOnEchoIt->value.IsBool()) {
				a_outParseResult.bReplaceOnEcho = replaceOnEchoIt->value.GetBool();
			}

            // read keep random results on loop (optional)
            if (auto keepRandomIt = doc.FindMember("keepRandomResultsOnLoop"); keepRandomIt != doc.MemberEnd() && keepRandomIt->value.IsBool()) {
                a_outParseResult.bKeepRandomResultsOnLoop = keepRandomIt->value.GetBool();
            }

			// read share random results (optional)
			if (auto shareRandomIt = doc.FindMember("shareRandomResults"); shareRandomIt != doc.MemberEnd() && shareRandomIt->value.IsBool()) {
				a_outParseResult.bShareRandomResults = shareRandomIt->value.GetBool();
			}

            // read conditions
            if (auto conditionsIt = doc.FindMember("conditions"); conditionsIt != doc.MemberEnd() && conditionsIt->value.IsArray()) {
                for (auto& conditionValue : conditionsIt->value.GetArray()) {
                    auto condition = Conditions::CreateConditionFromJson(conditionValue);
                    if (!condition->IsValid()) {
                        logger::error("Failed to parse condition in file: {}", a_jsonPath.string());

                        rapidjson::StringBuffer buffer;
                        rapidjson::PrettyWriter writer(buffer);
                        doc.Accept(writer);

                        logger::error("Dumping entire json file from memory: {}", buffer.GetString());
                    }

                    a_outParseResult.conditionSet->AddCondition(condition);
                }
            }

			if (auto pairedConditionsIt = doc.FindMember("pairedConditions"); pairedConditionsIt != doc.MemberEnd() && pairedConditionsIt->value.IsArray()) {
				for (auto& conditionValue : pairedConditionsIt->value.GetArray()) {
					auto condition = Conditions::CreateConditionFromJson(conditionValue);
					if (!condition->IsValid()) {
						logger::error("Failed to parse paired condition in file: {}", a_jsonPath.string());

						rapidjson::StringBuffer buffer;
						rapidjson::PrettyWriter writer(buffer);
						doc.Accept(writer);

						logger::error("Dumping entire json file from memory: {}", buffer.GetString());
					}

					if (!a_outParseResult.synchronizedConditionSet) {
						a_outParseResult.synchronizedConditionSet = std::make_unique<Conditions::ConditionSet>();
					}

					a_outParseResult.synchronizedConditionSet->AddCondition(condition);
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

    std::string ConvertVariantsPath(std::string_view a_path)
    {
        // removes the variants substring "_variants_" and appends ".hkx" to the end
		constexpr std::string_view substring = "_variants_"sv;

        const std::size_t substringStartPos = a_path.find(substring);
		if (substringStartPos == std::string::npos) {
			return a_path.data();
		}

		const std::size_t substringEndPos = substringStartPos + substring.length();

        std::string ret(a_path.substr(0, substringStartPos));
		ret.append(a_path.substr(substringEndPos));
		ret.append(".hkx");

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

	template <typename T>
	std::future<T> MakeFuture(T& a_t)
	{
		std::promise<T> p;
		p.set_value(std::forward<T>(a_t));
		return p.get_future();
	}

    void ParseDirectory(const std::filesystem::directory_entry& a_directory, ParseResults& a_outParseResults)
    {
		static constexpr auto oarFolderName = "openanimationreplacer"sv;
		static constexpr auto legacyFolderName = "dynamicanimationreplacer"sv;

		std::vector<std::future<void>> futures;

		for (std::filesystem::recursive_directory_iterator i(a_directory), end; i != end; ++i) {
			auto entry = *i;
			if (!entry.is_directory()) {
				continue;
			}

			std::string stemString;
			try {
				stemString = entry.path().stem().string();
			} catch (const std::system_error&) {
				auto path = entry.path().u8string();
				std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
				logger::warn("invalid directory name at {}, skipping", pathSv);
				continue;
			}

			if (Utils::CompareStringsIgnoreCase(stemString, oarFolderName)) {
				// we're in an OAR folder
				if (Settings::bAsyncParsing) {
					for (const auto& subEntry : std::filesystem::directory_iterator(entry)) {
						if (is_directory(subEntry)) {
							// we're in a mod folder. we have the subfolders here and a json.
							//Locker locker(a_outParseResults.modParseResultsLock);
							a_outParseResults.modParseResultFutures.emplace_back(std::async(std::launch::async, ParseModDirectory, subEntry));
						}
					}
				} else {
					for (const auto& subEntry : std::filesystem::directory_iterator(entry)) {
						if (std::filesystem::is_directory(subEntry)) {
							// we're in a mod folder. we have the subfolders here and a json.
							auto modParseResult = ParseModDirectory(subEntry);
							a_outParseResults.modParseResultFutures.emplace_back(MakeFuture(modParseResult));
						}
					}
				}
				i.disable_recursion_pending();
			} else if (Utils::CompareStringsIgnoreCase(stemString, legacyFolderName)) {
				// we're in the DAR folder
				for (const auto& subEntry : std::filesystem::directory_iterator(entry)) {
					if (is_directory(subEntry)) {
						std::string subEntryStemString;
						try {
							subEntryStemString = subEntry.path().stem().string();
						} catch (const std::system_error&) {
							auto path = subEntry.path().u8string();
							std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
							logger::warn("invalid directory name at {}, skipping", pathSv);
							continue;
						}
						if (Utils::CompareStringsIgnoreCase(subEntryStemString, "_CustomConditions"sv)) {
							// we're in the _CustomConditions directory
							for (const auto& subSubEntry : std::filesystem::directory_iterator(subEntry)) {
								if (std::filesystem::is_directory(subSubEntry)) {
									//Locker locker(a_outParseResults.legacyParseResultsLock);
									a_outParseResults.legacyParseResultFutures.emplace_back(std::async(std::launch::async, ParseLegacyCustomConditionsDirectory, subSubEntry));
								}
							}
						} else {
							// we're probably in a folder with a plugin name
							for (auto subModParseResults = ParseLegacyPluginDirectory(subEntry); auto& subModParseResult : subModParseResults) {
								if (subModParseResult.bSuccess) {
									a_outParseResults.legacyParseResultFutures.emplace_back(MakeFuture(subModParseResult));
								}
							}
						}
					}
				}
				i.disable_recursion_pending();
			}
		}

		if (Settings::bAsyncParsing) {
			for (auto& future : futures) {
				future.get();
			}
		}
    }

    ModParseResult ParseModDirectory(const std::filesystem::directory_entry& a_directory)
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
							futures.emplace_back(std::async(std::launch::async, ParseModSubdirectory, entry, false));
						}
					}

					for (auto& future : futures) {
						auto subModParseResult = future.get();
						if (subModParseResult.bSuccess) {
							result.subModParseResults.emplace_back(std::move(subModParseResult));
						}
					}
				} else {
					for (const auto& entry : std::filesystem::directory_iterator(a_directory)) {
						if (is_directory(entry)) {
							// we're in a mod subfolder. we have the animations here and a json.
							auto subModParseResult = ParseModSubdirectory(entry);
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

    SubModParseResult ParseModSubdirectory(const std::filesystem::directory_entry& a_subDirectory, bool a_bIsLegacy)
    {
		SubModParseResult result;

		bool bDeserializeSuccess = false;

		if (a_bIsLegacy) {
			const auto userJsonPath = a_subDirectory.path() / "user.json"sv;
			if (is_regular_file(userJsonPath)) {
				result.configSource = ConfigSource::kUser;

				// parse json
				bDeserializeSuccess = DeserializeSubMod(userJsonPath, DeserializeMode::kWithoutNameDescription, result);
			} else {
				return result;
			}
		} else {
			// check whether the config json file exists first
			const auto configJsonPath = a_subDirectory.path() / "config.json"sv;
			if (is_regular_file(configJsonPath)) {
				result.configSource = ConfigSource::kAuthor;

				// check whether user json exists
				const auto userJsonPath = a_subDirectory.path() / "user.json"sv;
				if (is_regular_file(userJsonPath)) {
					result.configSource = ConfigSource::kUser;

					// read name and description from the author json
					if (!DeserializeSubMod(configJsonPath, DeserializeMode::kNameDescriptionOnly, result)) {
						return result;
					}
				}

				// parse json
				if (result.configSource == ConfigSource::kUser) {
					bDeserializeSuccess = DeserializeSubMod(userJsonPath, DeserializeMode::kWithoutNameDescription, result);
				} else {
					bDeserializeSuccess = DeserializeSubMod(configJsonPath, DeserializeMode::kFull, result);
				}
			} else {
				return result;
			}
		}

		if (bDeserializeSuccess) {
			if (result.overrideAnimationsFolder.empty()) {
				result.animationFiles = ParseAnimationsInDirectory(a_subDirectory, a_bIsLegacy);
			} else {
				const auto overridePath = a_subDirectory.path().parent_path() / result.overrideAnimationsFolder;
				const auto overrideDirectory = std::filesystem::directory_entry(overridePath);
				if (is_directory(overrideDirectory)) {
					result.animationFiles = ParseAnimationsInDirectory(overrideDirectory, a_bIsLegacy);
				} else {
					result.bSuccess = false;
				}
			}
		}

		return result;
    }

    SubModParseResult ParseLegacyCustomConditionsDirectory(const std::filesystem::directory_entry& a_directory)
    {
        SubModParseResult result;

        // check whether _conditions.txt file exists first
        const std::filesystem::path txtPath = a_directory.path() / "_conditions.txt"sv;
        if (exists(txtPath)) {
            // check whether the user json file exists, if yes, treat it as a OAR submod
            const auto jsonPath = a_directory.path() / "user.json"sv;
            if (is_regular_file(jsonPath)) {
                result = ParseModSubdirectory(a_directory, true);
                result.name = std::to_string(result.priority);
                return result;
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

		if (directoryName.find_first_not_of("-0123456789"sv) == std::string::npos) {
			auto [ptr, ec]{ std::from_chars(directoryName.data(), directoryName.data() + directoryName.size(), priority) };
			if (ec == std::errc()) {
				if (exists(txtPath)) {
					result.configSource = ConfigSource::kLegacy;
					result.name = std::to_string(priority);
					result.priority = priority;
					result.conditionSet = ParseConditionsTxt(txtPath);  // parse conditions.txt
					result.animationFiles = ParseAnimationsInDirectory(a_directory, true);
					result.bKeepRandomResultsOnLoop = Settings::bLegacyKeepRandomResultsByDefault;
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
		} else {
			const auto subEntryPath = a_directory.path().u8string();
			std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
			logger::warn("invalid directory name at {}, skipping", subEntryPathSv);
		}

        result.path = a_directory.path().string();

        return result;
    }

    std::vector<SubModParseResult> ParseLegacyPluginDirectory(const std::filesystem::directory_entry& a_directory)
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
                    auto result = ParseModSubdirectory(subEntry, true);

                    const std::string fileString = a_directory.path().stem().string();
                    const std::string extensionString = a_directory.path().extension().string();

                    const std::string modName = fileString + extensionString;
                    const std::string formIDString = subEntry.path().filename().string();
                    result.name = modName;
                    result.name += '|';
                    result.name += formIDString;

                    results.emplace_back(std::move(result));
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
                        result.animationFiles = ParseAnimationsInDirectory(subEntry, true);
                        result.bKeepRandomResultsOnLoop = Settings::bLegacyKeepRandomResultsByDefault;
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

    std::optional<ReplacementAnimationFile> ParseReplacementAnimationEntry(std::string_view a_fullPath)
    {
        return ReplacementAnimationFile(a_fullPath);
    }

    std::optional<ReplacementAnimationFile> ParseReplacementAnimationVariants(std::string_view a_fullVariantsPath)
    {
		std::vector<ReplacementAnimationFile::Variant> variants;

		// iterate over all files
		for (const auto& fileEntry : std::filesystem::directory_iterator(a_fullVariantsPath)) {
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
				std::string fullPath;
				try {
					fullPath = fileEntry.path().string();
				} catch (const std::system_error&) {
					auto path = fileEntry.path().u8string();
					std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
					logger::warn("invalid filename at {}, skipping", pathSv);
					continue;
				}

			    variants.emplace_back(fullPath);
			}
		}

		if (variants.empty()) {
		    return std::nullopt;
		}

		return ReplacementAnimationFile(a_fullVariantsPath, variants);
    }

    std::vector<ReplacementAnimationFile> ParseAnimationsInDirectory(const std::filesystem::directory_entry& a_directory, bool a_bIsLegacy /* = false*/)
    {
		std::vector<ReplacementAnimationFile> result;

		if (!a_bIsLegacy) {
			std::vector<std::string> filenamesToSkip{};

			// iterate over directories first
			for (const auto& fileEntry : std::filesystem::directory_iterator(a_directory)) {
				if (is_directory(fileEntry)) {
					std::string directoryNameString;
					try {
						directoryNameString = fileEntry.path().filename().string();
					} catch (const std::system_error&) {
						auto path = fileEntry.path().filename().u8string();
						std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
						logger::warn("invalid directory name at {}, skipping", pathSv);
						continue;
					}

					if (directoryNameString.starts_with("_variants_"sv)) {
					    // parse variants directory
						std::string directoryEntryPath;
						try {
							directoryEntryPath = fileEntry.path().string();
						} catch (const std::system_error&) {
							auto path = fileEntry.path().u8string();
							std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
							logger::warn("invalid directory at {}, skipping", pathSv);
							continue;
						}

						filenamesToSkip.emplace_back(ConvertVariantsPath(directoryNameString));

						if (auto anim = ParseReplacementAnimationVariants(directoryEntryPath)) {
							result.emplace_back(*anim);
						}

					} else {
						// parse child directory normally
						// append result
						auto res = ParseAnimationsInDirectory(fileEntry, a_bIsLegacy);
						result.reserve(result.size() + res.size());
						result.insert(result.end(), std::make_move_iterator(res.begin()), std::make_move_iterator(res.end()));
					}
				}
			}

			// then iterate over files
			for (const auto& fileEntry : std::filesystem::directory_iterator(a_directory)) {
				if (is_regular_file(fileEntry)) {
					std::string extensionString;
					try {
						extensionString = fileEntry.path().extension().string();
					} catch (const std::system_error&) {
						auto path = fileEntry.path().extension().u8string();
						std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
						logger::warn("invalid filename at {}, skipping", pathSv);
						continue;
					}

					if (Utils::CompareStringsIgnoreCase(extensionString, ".hkx"sv)) {
					    std::string filenameString;
					    try {
						    filenameString = fileEntry.path().filename().string();
					    } catch (const std::system_error&) {
						    auto path = fileEntry.path().filename().u8string();
						    std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
						    logger::warn("invalid filename at {}, skipping", pathSv);
						    continue;
					    }

					    std::string fileEntryPath;
					    try {
						    fileEntryPath = fileEntry.path().string();
					    } catch (const std::system_error&) {
						    auto path = fileEntry.path().u8string();
						    std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
						    logger::warn("invalid filename at {}, skipping", pathSv);
						    continue;
					    }

					    // check if we should skip this file because the variants directory exists
					    const bool bSkip = std::ranges::any_of(filenamesToSkip, [&](const auto& a_filename) {
						    return filenameString == a_filename;
					    });

					    if (bSkip) {
						    logger::warn("skipping {}{} at {} because a variants directory exists for this animation", filenameString, extensionString, fileEntryPath);
					        continue;
					    }

					    if (auto anim = ParseReplacementAnimationEntry(fileEntryPath)) {
						    result.emplace_back(*anim);
					    }
					}
				}
			}
		} else {
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
						auto path = fileEntry.path().u8string();
						std::string_view pathSv(reinterpret_cast<const char*>(path.data()), path.size());
						logger::warn("invalid filename at {}, skipping", pathSv);
						continue;
					}
					if (auto anim = ParseReplacementAnimationEntry(fileEntryPath)) {
						result.emplace_back(*anim);
					}
				}
			}
		}

        return result;
    }
}
