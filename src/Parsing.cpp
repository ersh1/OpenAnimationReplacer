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

	bool DeserializeMod(const std::filesystem::path& a_jsonPath, DeserializeMode a_deserializeMode, ModParseResult& a_outParseResult)
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

			if (a_deserializeMode != DeserializeMode::kDataOnly) {
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
			}

			if (a_deserializeMode == DeserializeMode::kInfoOnly) {
				// we're only here to get the info, so we're done
				return true;
			}

			// read condition presets (optional)
			if (const auto presetIt = doc.FindMember("conditionPresets"); presetIt != doc.MemberEnd() && presetIt->value.IsArray()) {
				for (auto& conditionPresetValue : presetIt->value.GetArray()) {
					if (conditionPresetValue.IsObject()) {
						const auto conditionPresetObject = conditionPresetValue.GetObj();

						if (const auto conditionPresetNameIt = conditionPresetObject.FindMember("name"); conditionPresetNameIt != conditionPresetObject.MemberEnd() && conditionPresetNameIt->value.IsString()) {
							std::string conditionPresetName = conditionPresetNameIt->value.GetString();

							std::string conditionPresetDescription = "";  // optional
							if (const auto conditionPresetDescriptionIt = conditionPresetObject.FindMember("description"); conditionPresetDescriptionIt != conditionPresetObject.MemberEnd() && conditionPresetDescriptionIt->value.IsString()) {
								conditionPresetDescription = conditionPresetDescriptionIt->value.GetString();
							}

							if (const auto conditionPresetConditionSetIt = conditionPresetObject.FindMember("conditions"); conditionPresetConditionSetIt != conditionPresetObject.MemberEnd() && conditionPresetConditionSetIt->value.IsArray()) {
								auto conditionPreset = std::make_unique<Conditions::ConditionPreset>(conditionPresetName, conditionPresetDescription);
								for (auto& conditionValue : conditionPresetConditionSetIt->value.GetArray()) {
									auto condition = Conditions::CreateConditionFromJson(conditionValue);
									if (!condition->IsValid()) {
										logger::error("Failed to parse condition in file: {}", a_jsonPath.string());

										rapidjson::StringBuffer buffer;
										rapidjson::PrettyWriter writer(buffer);
										doc.Accept(writer);

										logger::error("Dumping entire json file from memory: {}", buffer.GetString());
									}

									conditionPreset->AddCondition(condition);
								}

								a_outParseResult.conditionPresets.push_back(std::move(conditionPreset));
							}
						}
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

			if (a_deserializeMode != DeserializeMode::kDataOnly) {
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

			if (a_deserializeMode == DeserializeMode::kInfoOnly) {
				// we're only here to get the info, so we're done
				return true;
			}

			// read submod priority (required)
			if (auto it = doc.FindMember("priority"); it != doc.MemberEnd() && it->value.IsInt()) {
				a_outParseResult.priority = it->value.GetInt();
			} else {
				logger::error("Failed to find submod priority in file: {}", a_jsonPath.string());
				return false;
			}

			// read submod disabled (optional)
			if (auto it = doc.FindMember("disabled"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bDisabled = it->value.GetBool();
			}

			// read disabled animations (optional, json field deprecated and replaced by replacementAnimDatas - reading it kept for compatibility with older config versions)
			if (auto it = doc.FindMember("disabledAnimations"); it != doc.MemberEnd() && it->value.IsArray()) {
				for (auto& disabledAnimation : it->value.GetArray()) {
					if (disabledAnimation.IsObject()) {
						auto projectNameIt = disabledAnimation.FindMember("projectName");
						if (auto pathIt = disabledAnimation.FindMember("path"); projectNameIt != disabledAnimation.MemberEnd() && projectNameIt->value.IsString() && pathIt != disabledAnimation.MemberEnd() && pathIt->value.IsString()) {
							a_outParseResult.replacementAnimDatas.emplace_back(projectNameIt->value.GetString(), pathIt->value.GetString(), true);
						}
					}
				}
			}

			// read deprecated settings - for backwards compatibility
			if (auto it = doc.FindMember("keepRandomResultsOnLoop"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bKeepRandomResultsOnLoop_DEPRECATED = it->value.GetBool();
			}
			if (auto it = doc.FindMember("shareRandomResults"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bShareRandomResults_DEPRECATED = it->value.GetBool();
			}

			// read replacement animation datas (optional)
			if (auto it = doc.FindMember("replacementAnimDatas"); it != doc.MemberEnd() && it->value.IsArray()) {
				for (auto& replacementAnimData : it->value.GetArray()) {
					if (replacementAnimData.IsObject()) {
						auto projectNameIt = replacementAnimData.FindMember("projectName");
						if (auto pathIt = replacementAnimData.FindMember("path"); projectNameIt != replacementAnimData.MemberEnd() && projectNameIt->value.IsString() && pathIt != replacementAnimData.MemberEnd() && pathIt->value.IsString()) {
							bool bDisabled = false;
							if (auto disabledIt = replacementAnimData.FindMember("disabled"); disabledIt != replacementAnimData.MemberEnd() && disabledIt->value.IsBool()) {
								bDisabled = disabledIt->value.GetBool();
							}

							// read replacement animation variants
							std::optional<std::vector<ReplacementAnimData::Variant>> variants = std::nullopt;
							std::optional<VariantMode> variantMode = std::nullopt;
							std::optional<Conditions::StateDataScope> variantStateScope = std::nullopt;
							if (a_outParseResult.bShareRandomResults_DEPRECATED) {
								variantStateScope = Conditions::StateDataScope::kSubMod;
							}
							bool bBlendBetweenVariants = true;
							bool bResetRandomOnLoopOrEcho = !a_outParseResult.bKeepRandomResultsOnLoop_DEPRECATED;
							bool bSharePlayedHistory = false;

							if (auto variantsIt = replacementAnimData.FindMember("variants"); variantsIt != replacementAnimData.MemberEnd() && variantsIt->value.IsArray()) {
								int32_t variantIndex = 0;
								for (auto& variantObj : variantsIt->value.GetArray()) {
									if (variantObj.IsObject()) {
										if (auto variantFilenameIt = variantObj.FindMember("filename"); variantFilenameIt != variantObj.MemberEnd() && variantFilenameIt->value.IsString()) {
											bool bVariantDisabled = false;
											float variantWeight = 1.f;
											bool bVariantPlayOnce = false;

											if (auto variantDisabledIt = variantObj.FindMember("disabled"); variantDisabledIt != variantObj.MemberEnd() && variantDisabledIt->value.IsBool()) {
												bVariantDisabled = variantDisabledIt->value.GetBool();
											}
											if (auto weightIt = variantObj.FindMember("weight"); weightIt != variantObj.MemberEnd() && weightIt->value.IsNumber()) {
												variantWeight = weightIt->value.GetFloat();
											}
											if (auto variantPlayOnceIt = variantObj.FindMember("playOnce"); variantPlayOnceIt != variantObj.MemberEnd() && variantPlayOnceIt->value.IsBool()) {
												bVariantPlayOnce = variantPlayOnceIt->value.GetBool();
											}

											ReplacementAnimData::Variant variant(variantFilenameIt->value.GetString(), bVariantDisabled, variantWeight, variantIndex++, bVariantPlayOnce);

											if (!variants.has_value()) {
												variants.emplace();
											}

											variants->emplace_back(variant);
										}
									}
								}
							}

							if (auto variantModeIt = replacementAnimData.FindMember("variantMode"); variantModeIt != replacementAnimData.MemberEnd() && variantModeIt->value.IsNumber()) {
								variantMode = static_cast<VariantMode>(variantModeIt->value.GetInt());
							}

							if (auto variantStateScopeIt = replacementAnimData.FindMember("variantStateScope"); variantStateScopeIt != replacementAnimData.MemberEnd() && variantStateScopeIt->value.IsNumber()) {
								variantStateScope = static_cast<Conditions::StateDataScope>(variantStateScopeIt->value.GetInt());
							}

							if (auto blendIt = replacementAnimData.FindMember("blendBetweenVariants"); blendIt != replacementAnimData.MemberEnd() && blendIt->value.IsBool()) {
								bBlendBetweenVariants = blendIt->value.GetBool();
							}

							if (auto resetRandomIt = replacementAnimData.FindMember("resetRandomOnLoopOrEcho"); resetRandomIt != replacementAnimData.MemberEnd() && resetRandomIt->value.IsBool()) {
								bResetRandomOnLoopOrEcho = resetRandomIt->value.GetBool();
							}

							if (auto sharePlayedHistoryIt = replacementAnimData.FindMember("sharePlayedHistory"); sharePlayedHistoryIt != replacementAnimData.MemberEnd() && sharePlayedHistoryIt->value.IsBool()) {
								bSharePlayedHistory = sharePlayedHistoryIt->value.GetBool();
							}

							a_outParseResult.replacementAnimDatas.emplace_back(projectNameIt->value.GetString(), pathIt->value.GetString(), bDisabled, variants, variantMode, variantStateScope, bBlendBetweenVariants, bResetRandomOnLoopOrEcho, bSharePlayedHistory);
						}
					}
				}
			}

			// read override animations folder (optional)
			if (auto it = doc.FindMember("overrideAnimationsFolder"); it != doc.MemberEnd() && it->value.IsString()) {
				a_outParseResult.overrideAnimationsFolder = it->value.GetString();
			}

			// read required project name (optional)
			if (auto it = doc.FindMember("requiredProjectName"); it != doc.MemberEnd() && it->value.IsString()) {
				a_outParseResult.requiredProjectName = it->value.GetString();
			}

			// read ignore no triggers flag (optional)
			if (auto it = doc.FindMember("ignoreDontConvertAnnotationsToTriggersFlag"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bIgnoreDontConvertAnnotationsToTriggersFlag = it->value.GetBool();
			} else if (auto oldNameIt = doc.FindMember("ignoreNoTriggersFlag"); oldNameIt != doc.MemberEnd() && oldNameIt->value.IsBool()) {  // old name
				a_outParseResult.bIgnoreDontConvertAnnotationsToTriggersFlag = oldNameIt->value.GetBool();
			}

			// read triggersOnly (optional)
			if (auto it = doc.FindMember("triggersFromAnnotationsOnly"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bTriggersFromAnnotationsOnly = it->value.GetBool();
			}

			// read interruptible (optional)
			if (auto it = doc.FindMember("interruptible"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bInterruptible = it->value.GetBool();
			}

			// read custom blend time on interrupt (optional) - only if interruptible is true
			if (a_outParseResult.bInterruptible) {
				if (auto it = doc.FindMember("hasCustomBlendTimeOnInterrupt"); it != doc.MemberEnd() && it->value.IsBool()) {
					a_outParseResult.bCustomBlendTimeOnInterrupt = it->value.GetBool();
				}
				if (a_outParseResult.bCustomBlendTimeOnInterrupt) {
					if (auto it = doc.FindMember("blendTimeOnInterrupt"); it != doc.MemberEnd() && it->value.IsNumber()) {
						a_outParseResult.blendTimeOnInterrupt = it->value.GetFloat();
					}
				}
			}

			// read replace on loop (optional)
			if (auto it = doc.FindMember("replaceOnLoop"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bReplaceOnLoop = it->value.GetBool();
			}

			// read custom blend time on loop (optional) - only if replace on loop is true
			if (a_outParseResult.bReplaceOnLoop) {
				if (auto it = doc.FindMember("hasCustomBlendTimeOnLoop"); it != doc.MemberEnd() && it->value.IsBool()) {
					a_outParseResult.bCustomBlendTimeOnLoop = it->value.GetBool();
				}
				if (a_outParseResult.bCustomBlendTimeOnLoop) {
					if (auto it = doc.FindMember("blendTimeOnLoop"); it != doc.MemberEnd() && it->value.IsNumber()) {
						a_outParseResult.blendTimeOnLoop = it->value.GetFloat();
					}
				}
			}

			// read replace on echo (optional)
			if (auto it = doc.FindMember("replaceOnEcho"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bReplaceOnEcho = it->value.GetBool();
			}

			// read custom blend time on echo (optional) - only if replace on echo is true
			if (a_outParseResult.bReplaceOnEcho) {
				if (auto it = doc.FindMember("hasCustomBlendTimeOnEcho"); it != doc.MemberEnd() && it->value.IsBool()) {
					a_outParseResult.bCustomBlendTimeOnEcho = it->value.GetBool();
				}
				if (a_outParseResult.bCustomBlendTimeOnEcho) {
					if (auto it = doc.FindMember("blendTimeOnEcho"); it != doc.MemberEnd() && it->value.IsNumber()) {
						a_outParseResult.blendTimeOnEcho = it->value.GetFloat();
					}
				}
			}

			// read conditions
			if (auto it = doc.FindMember("conditions"); it != doc.MemberEnd() && it->value.IsArray()) {
				for (auto& conditionValue : it->value.GetArray()) {
					auto condition = Conditions::CreateConditionFromJson(conditionValue, a_outParseResult.conditionSet.get());
					if (!Utils::ConditionHasPresetCondition(condition.get()) && !condition->IsValid()) {
						logger::error("Failed to parse condition in file: {}", a_jsonPath.string());

						rapidjson::StringBuffer buffer;
						rapidjson::PrettyWriter writer(buffer);
						doc.Accept(writer);

						logger::error("Dumping entire json file from memory: {}", buffer.GetString());
					}

					// backwards compatibility with deprecated setting
					if (condition->GetName() == "Random") {
						auto randomCondition = static_cast<Conditions::RandomCondition*>(condition.get());
						if (a_outParseResult.bKeepRandomResultsOnLoop_DEPRECATED) {
							randomCondition->stateComponent->SetShouldResetOnLoopOrEcho(false);
						}
						if (a_outParseResult.bShareRandomResults_DEPRECATED) {
							randomCondition->stateComponent->SetStateDataScope(Conditions::StateDataScope::kSubMod);
						}
					}

					a_outParseResult.conditionSet->AddCondition(condition);
				}
			}

			if (auto it = doc.FindMember("pairedConditions"); it != doc.MemberEnd() && it->value.IsArray()) {
				for (auto& conditionValue : it->value.GetArray()) {
					auto condition = Conditions::CreateConditionFromJson(conditionValue, a_outParseResult.synchronizedConditionSet.get());
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

		std::size_t substringStartPos = Utils::FindStringIgnoreCase(a_path, "OpenAnimationReplacer"sv);
		if (substringStartPos == std::string::npos) {
			substringStartPos = Utils::FindStringIgnoreCase(a_path, "DynamicAnimationReplacer"sv);
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
		if (!Utils::Exists(a_directory)) {
			return;
		}

		static constexpr auto oarFolderName = "openanimationreplacer"sv;
		static constexpr auto legacyFolderName = "dynamicanimationreplacer"sv;
		static constexpr auto mohiddenFolderName = ".mohidden"sv;

		std::vector<std::future<void>> futures;

		for (std::filesystem::recursive_directory_iterator i(a_directory), end; i != end; ++i) {
			auto entry = *i;
			if (!Utils::IsDirectory(entry)) {
				continue;
			}

			if (IsPathValid(entry.path())) {
				std::string stemString = entry.path().stem().string();
				if (Utils::CompareStringsIgnoreCase(stemString, oarFolderName)) {
					// we're in an OAR folder
					if (Settings::bAsyncParsing) {
						for (const auto& subEntry : std::filesystem::directory_iterator(entry)) {
							if (Utils::IsDirectory(subEntry)) {
								// we're in a mod folder. we have the subfolders here and a json.
								//Locker locker(a_outParseResults.modParseResultsLock);
								a_outParseResults.modParseResultFutures.emplace_back(std::async(std::launch::async, ParseModDirectory, subEntry));
							}
						}
					} else {
						for (const auto& subEntry : std::filesystem::directory_iterator(entry)) {
							if (Utils::IsDirectory(subEntry)) {
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
						if (Utils::IsDirectory(subEntry)) {
							if (IsPathValid(subEntry.path())) {
								if (Utils::CompareStringsIgnoreCase(subEntry.path().stem().string(), "_CustomConditions"sv)) {
									// we're in the _CustomConditions directory
									for (const auto& subSubEntry : std::filesystem::directory_iterator(subEntry)) {
										if (Utils::IsDirectory(subSubEntry)) {
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
					}
					i.disable_recursion_pending();
				}
			} else {
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

		if (IsPathValid(a_directory.path())) {
			bool bDeserializeSuccess = false;

			// check whether the config json file exists first
			const auto configJsonPath = a_directory.path() / "config.json"sv;
			if (Utils::IsRegularFile(configJsonPath)) {
				result.configSource = ConfigSource::kAuthor;

				// check whether user json exists
				const auto userJsonPath = a_directory.path() / "user.json"sv;
				if (Utils::IsRegularFile(userJsonPath)) {
					result.configSource = ConfigSource::kUser;

					// read info from the author json
					if (!DeserializeMod(configJsonPath, DeserializeMode::kInfoOnly, result)) {
						return result;
					}
				}

				// parse the config json file
				if (result.configSource == ConfigSource::kUser) {
					bDeserializeSuccess = DeserializeMod(userJsonPath, DeserializeMode::kDataOnly, result);
				} else {
					bDeserializeSuccess = DeserializeMod(configJsonPath, DeserializeMode::kFull, result);
				}
			}

			if (bDeserializeSuccess) {
				// parse the subfolders
				if (Settings::bAsyncParsing) {
					std::vector<std::future<SubModParseResult>> futures;
					for (const auto& entry : std::filesystem::directory_iterator(a_directory)) {
						if (Utils::IsDirectory(entry)) {
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
						if (Utils::IsDirectory(entry)) {
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

		if (IsPathValid(a_subDirectory.path())) {
			bool bDeserializeSuccess = false;

			if (a_bIsLegacy) {
				const auto userJsonPath = a_subDirectory.path() / "user.json"sv;
				if (Utils::IsRegularFile(userJsonPath)) {
					result.configSource = ConfigSource::kUser;

					// parse json
					bDeserializeSuccess = DeserializeSubMod(userJsonPath, DeserializeMode::kDataOnly, result);
				} else {
					return result;
				}
			} else {
				// check whether the config json file exists first
				const auto configJsonPath = a_subDirectory.path() / "config.json"sv;
				if (Utils::IsRegularFile(configJsonPath)) {
					result.configSource = ConfigSource::kAuthor;

					// check whether user json exists
					const auto userJsonPath = a_subDirectory.path() / "user.json"sv;
					if (Utils::IsRegularFile(userJsonPath)) {
						result.configSource = ConfigSource::kUser;

						// read info from the author json
						if (!DeserializeSubMod(configJsonPath, DeserializeMode::kInfoOnly, result)) {
							return result;
						}
					}

					// parse json
					if (result.configSource == ConfigSource::kUser) {
						bDeserializeSuccess = DeserializeSubMod(userJsonPath, DeserializeMode::kDataOnly, result);
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
					if (Utils::IsDirectory(overrideDirectory)) {
						result.animationFiles = ParseAnimationsInDirectory(overrideDirectory, a_bIsLegacy);
					} else {
						result.bSuccess = false;
					}
				}
			}
		}

		return result;
	}

	SubModParseResult ParseLegacyCustomConditionsDirectory(const std::filesystem::directory_entry& a_directory)
	{
		SubModParseResult result;

		if (IsPathValid(a_directory.path())) {
			// check whether _conditions.txt file exists first
			const std::filesystem::path txtPath = a_directory.path() / "_conditions.txt"sv;
			if (Utils::Exists(txtPath)) {
				// check whether the user json file exists, if yes, treat it as a OAR submod
				const auto jsonPath = a_directory.path() / "user.json"sv;
				if (Utils::IsRegularFile(jsonPath)) {
					result = ParseModSubdirectory(a_directory, true);
					result.name = std::to_string(result.priority);
					return result;
				}
			}

			int32_t priority = 0;
			std::string directoryName = a_directory.path().filename().string();

			if (directoryName.find_first_not_of("-0123456789"sv) == std::string::npos) {
				auto [ptr, ec]{ std::from_chars(directoryName.data(), directoryName.data() + directoryName.size(), priority) };
				if (ec == std::errc()) {
					if (Utils::Exists(txtPath)) {
						result.configSource = ConfigSource::kLegacy;
						result.name = std::to_string(priority);
						result.priority = priority;
						result.conditionSet = ParseConditionsTxt(txtPath);  // parse conditions.txt
						result.animationFiles = ParseAnimationsInDirectory(a_directory, true);
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
		}

		return result;
	}

	std::vector<SubModParseResult> ParseLegacyPluginDirectory(const std::filesystem::directory_entry& a_directory)
	{
		std::vector<SubModParseResult> results;

		for (const auto& subEntry : std::filesystem::directory_iterator(a_directory)) {
			if (Utils::IsDirectory(subEntry)) {
				// check whether there's any file here
				if (is_empty(subEntry)) {
					continue;
				}

				// check whether the user json file exists, if yes, treat it as a OAR submod
				auto jsonPath = subEntry.path() / "user.json"sv;
				if (Utils::IsRegularFile(jsonPath)) {
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

				if (IsPathValid(subEntry.path())) {
					RE::FormID formID;
					std::string directoryName = subEntry.path().filename().string();

					auto [ptr, ec]{ std::from_chars(directoryName.data(), directoryName.data() + directoryName.size(), formID, 16) };
					if (ec == std::errc()) {
						std::string fileString = a_directory.path().stem().string();
						std::string extensionString = a_directory.path().extension().string();

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
			if (IsPathValid(fileEntry.path())) {
				if (Utils::IsRegularFile(fileEntry) && Utils::CompareStringsIgnoreCase(fileEntry.path().extension().string(), ".hkx"sv)) {
					variants.emplace_back(fileEntry.path().string());
				}
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
				if (Utils::IsDirectory(fileEntry)) {
					if (IsPathValid(fileEntry.path())) {
						std::string directoryNameString = fileEntry.path().filename().string();

						if (directoryNameString.starts_with("_variants_"sv)) {
							// parse variants directory
							filenamesToSkip.emplace_back(ConvertVariantsPath(directoryNameString));

							if (auto anim = ParseReplacementAnimationVariants(fileEntry.path().string())) {
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
			}

			// then iterate over files
			for (const auto& fileEntry : std::filesystem::directory_iterator(a_directory)) {
				if (Utils::IsRegularFile(fileEntry)) {
					if (IsPathValid(fileEntry.path())) {
						if (Utils::CompareStringsIgnoreCase(fileEntry.path().extension().string(), ".hkx"sv)) {
							// check if we should skip this file because the variants directory exists
							auto filenameString = fileEntry.path().filename().string();
							const bool bSkip = std::ranges::any_of(filenamesToSkip, [&](const auto& a_filename) {
								return filenameString == a_filename;
							});

							if (bSkip) {
								logger::warn("skipping {}{} at {} because a variants directory exists for this animation", filenameString, fileEntry.path().extension().string(), fileEntry.path().string());
								continue;
							}

							if (auto anim = ParseReplacementAnimationEntry(fileEntry.path().string())) {
								result.emplace_back(*anim);
							}
						}
					}
				}
			}
		} else {
			for (const auto& fileEntry : std::filesystem::recursive_directory_iterator(a_directory)) {
				if (IsPathValid(fileEntry.path())) {
					if (Utils::IsRegularFile(fileEntry) && Utils::CompareStringsIgnoreCase(fileEntry.path().extension().string(), ".hkx"sv)) {
						if (auto anim = ParseReplacementAnimationEntry(fileEntry.path().string())) {
							result.emplace_back(*anim);
						}
					}
				}
			}
		}

		return result;
	}

	bool IsPathValid(std::filesystem::path a_path)
	{
		// skip invalid paths
		std::string pathString;
		try {
			pathString = a_path.string();
		} catch (const std::system_error&) {
			auto pathU8String = a_path.u8string();
			std::string_view pathSv(reinterpret_cast<const char*>(pathU8String.data()), pathU8String.size());
			logger::warn("invalid path at {}, skipping", pathSv);
			return false;
		}

		// skip hidden folders
		static constexpr auto mohiddenFolderName = ".mohidden"sv;
		if (Utils::ContainsStringIgnoreCase(pathString, mohiddenFolderName)) {
			return false;
		}

		return true;
	}
}
