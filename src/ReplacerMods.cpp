#include "ReplacerMods.h"

#include <ranges>

#include "DetectedProblems.h"
#include "Offsets.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"

bool SubMod::AddReplacementAnimation(std::string_view a_animPath, uint16_t a_originalIndex, ReplacerProjectData* a_replacerProjectData, RE::hkbCharacterStringData* a_stringData)
{
	bool bAdded = false;

    if (const auto search = _replacementAnimationFiles.find(a_animPath.data()); search != _replacementAnimationFiles.end()) {

	    std::unique_ptr<ReplacementAnimation> newReplacementAnimation = nullptr;

		auto& animFile = search->second;
		if (animFile.variants) {
			std::vector<ReplacementAnimation::Variant> variants;
			for (auto& variantToAdd : *animFile.variants) {
				if (uint16_t newIndex = a_replacerProjectData->TryAddAnimationToAnimationBundleNames(variantToAdd.fullPath, variantToAdd.hash); newIndex != static_cast<uint16_t>(-1)) {
					variants.emplace_back(newIndex, Utils::GetFileNameWithExtension(variantToAdd.fullPath));
				}
			}

			newReplacementAnimation = std::make_unique<ReplacementAnimation>(variants, a_originalIndex, _priority, animFile.fullPath, a_stringData->name.data(), _conditionSet.get());
		} else if (uint16_t newIndex = a_replacerProjectData->TryAddAnimationToAnimationBundleNames(animFile.fullPath, animFile.hash); newIndex != static_cast<uint16_t>(-1)) {
			newReplacementAnimation = std::make_unique<ReplacementAnimation>(newIndex, a_originalIndex, _priority, animFile.fullPath, a_stringData->name.data(), _conditionSet.get());
		}

		if (newReplacementAnimation) {
			bAdded = true;
			newReplacementAnimation->_parentSubMod = this;

			{
				WriteLocker locker(_dataLock);
				_replacementAnimations.emplace_back(newReplacementAnimation.get());

				// sort replacement animations by path
				std::ranges::sort(_replacementAnimations, [](const auto& a_lhs, const auto& a_rhs) {
					return a_lhs->_path < a_rhs->_path;
				});
			}

			// load anim data
			const auto animDataSearch = std::ranges::find_if(_replacementAnimDatas, [&](const ReplacementAnimData& a_replacementAnimData) {
				return a_replacementAnimData.projectName == a_stringData->name.data() && a_replacementAnimData.path == a_animPath;
            });

			if (animDataSearch != _replacementAnimDatas.end()) {
			    newReplacementAnimation->LoadAnimData(*animDataSearch);
            }

			a_replacerProjectData->AddReplacementAnimation(a_stringData, a_originalIndex, newReplacementAnimation);
			AddReplacerProject(a_replacerProjectData);
		}
    }

	return bAdded;
}

void SubMod::SetAnimationFiles(const std::vector<ReplacementAnimationFile>& a_animationFiles)
{
	auto& openAnimationReplacer = OpenAnimationReplacer::GetSingleton();

	WriteLocker locker(_dataLock);

	for (const auto& animFile : a_animationFiles) {
		auto originalPath = animFile.GetOriginalPath();

		_replacementAnimationFiles.emplace(originalPath, animFile);
		openAnimationReplacer.CacheAnimationPathSubMod(originalPath, this);
	}
}

void SubMod::LoadParseResult(const Parsing::SubModParseResult& a_parseResult)
{
	ResetAnimations();

    _path = a_parseResult.path;
    _configSource = a_parseResult.configSource;
    _name = a_parseResult.name;
    _description = a_parseResult.description;
    _priority = a_parseResult.priority;
    _bDisabled = a_parseResult.bDisabled;
	_replacementAnimDatas = a_parseResult.replacementAnimDatas;
    _overrideAnimationsFolder = a_parseResult.overrideAnimationsFolder;
    _requiredProjectName = a_parseResult.requiredProjectName;
    _bIgnoreDontConvertAnnotationsToTriggersFlag = a_parseResult.bIgnoreDontConvertAnnotationsToTriggersFlag;
	_bTriggersFromAnnotationsOnly = a_parseResult.bTriggersFromAnnotationsOnly;
    _bInterruptible = a_parseResult.bInterruptible;
	_bReplaceOnLoop = a_parseResult.bReplaceOnLoop;
	_bReplaceOnEcho = a_parseResult.bReplaceOnEcho;
    _bKeepRandomResultsOnLoop = a_parseResult.bKeepRandomResultsOnLoop;
	_bShareRandomResults = a_parseResult.bShareRandomResults;
    _conditionSet->MoveConditions(a_parseResult.conditionSet.get());
	if (a_parseResult.synchronizedConditionSet) {
		SetHasSynchronizedAnimations();
		_synchronizedConditionSet->MoveConditions(a_parseResult.synchronizedConditionSet.get());
	}
	
	LoadReplacementAnimationDatas(_replacementAnimDatas);

    SetDirtyRecursive(false);
}

void SubMod::LoadReplacementAnimationDatas(const std::vector<ReplacementAnimData>& a_replacementAnimDatas)
{
	for (const auto& replacementAnimData : a_replacementAnimDatas) {
		auto search = std::ranges::find_if(_replacementAnimations, [&](const ReplacementAnimation* a_replacementAnimation) {
		    return a_replacementAnimation->GetProjectName() == replacementAnimData.projectName && a_replacementAnimation->GetAnimPath() == replacementAnimData.path;
		});

		if (search != _replacementAnimations.end()) {
			(*search)->LoadAnimData(replacementAnimData);
		}
	}
}

void SubMod::ResetAnimations()
{
	for (const auto& anim : _replacementAnimations) {
		anim->SetDisabled(false);
		anim->ResetVariants();
	}
}

void SubMod::UpdateAnimations() const
{
    // Update stuff in each anim
    for (const auto& anim : _replacementAnimations) {
        anim->SetPriority(_priority);
        anim->SetDisabledByParent(_bDisabled);
        anim->SetIgnoreDontConvertAnnotationsToTriggersFlag(_bIgnoreDontConvertAnnotationsToTriggersFlag);
		anim->SetTriggersFromAnnotationsOnly(_bTriggersFromAnnotationsOnly);
        anim->SetInterruptible(_bInterruptible);
		anim->SetReplaceOnLoop(_bReplaceOnLoop);
		anim->SetReplaceOnEcho(_bReplaceOnEcho);
        anim->SetKeepRandomResultsOnLoop(_bKeepRandomResultsOnLoop);
		anim->SetShareRandomResults(_bShareRandomResults);
		anim->UpdateVariantCache();
    }

    // Update stuff in each anim replacements struct
    if (_parentMod) {
        _parentMod->SortSubMods();
    }

    ForEachReplacerProject([](const auto a_replacerProject) {
        a_replacerProject->ForEach([](auto a_animReplacements) {
            a_animReplacements->TestInterruptible();
            a_animReplacements->TestReplaceOnEcho();
            a_animReplacements->TestKeepRandomResultsOnLoop();
            a_animReplacements->SortByPriority();
        });
    });
}

bool SubMod::DoesUserConfigExist() const
{
    std::filesystem::path jsonPath(_path);
    jsonPath = jsonPath / "user.json"sv;

    return is_regular_file(jsonPath);
}

void SubMod::DeleteUserConfig() const
{
    std::filesystem::path jsonPath(_path);
    jsonPath = jsonPath / "user.json"sv;

    if (is_regular_file(jsonPath)) {
        std::filesystem::remove(jsonPath);
    }
}

bool SubMod::HasInvalidConditions() const
{
    return _conditionSet->HasInvalidConditions() || (_synchronizedConditionSet && _synchronizedConditionSet->HasInvalidConditions());
}

void SubMod::SetHasSynchronizedAnimations()
{
	if (!_synchronizedConditionSet) {
		_synchronizedConditionSet = std::make_unique<Conditions::ConditionSet>(this);

		for (auto& anim : _replacementAnimations) {
		    anim->SetSynchronizedConditionSet(_synchronizedConditionSet.get());
        }
	}
}

bool SubMod::ReloadConfig()
{
    Parsing::SubModParseResult parseResult;

    std::filesystem::path directoryPath(_path);

    bool bDeserializeSuccess = false;

    // check if this was originally a legacy mod
    auto directoryPathStr = directoryPath.string();
    bool bOriginallyLegacy = directoryPathStr.find("DynamicAnimationReplacer"sv) != std::string::npos;

    auto configPath = directoryPath / "config.json"sv;
    auto userPath = directoryPath / "user.json"sv;
    bool bConfigJsonFound = is_regular_file(configPath);
    bool bUserJsonFound = is_regular_file(userPath);
    if (bConfigJsonFound || bUserJsonFound) {
        if (bOriginallyLegacy) {
            // if it was originally a legacy mod, only read user.json
            if (bUserJsonFound) {
                ResetToLegacy();
                parseResult.name = _name;
                parseResult.description = _description;
                parseResult.configSource = Parsing::ConfigSource::kUser;
                bDeserializeSuccess = DeserializeSubMod(userPath, Parsing::DeserializeMode::kWithoutNameDescription, parseResult);
            }
        } else {
            if (bUserJsonFound) {
                parseResult.configSource = Parsing::ConfigSource::kUser;
                if (!DeserializeSubMod(configPath, Parsing::DeserializeMode::kNameDescriptionOnly, parseResult)) {
                    return false;
                }
                bDeserializeSuccess = DeserializeSubMod(userPath, Parsing::DeserializeMode::kWithoutNameDescription, parseResult);
            } else {
                parseResult.configSource = Parsing::ConfigSource::kAuthor;
                bDeserializeSuccess = DeserializeSubMod(configPath, Parsing::DeserializeMode::kFull, parseResult);
            }
        }

        if (bDeserializeSuccess) {
            LoadParseResult(parseResult);
            auto& detectedProblems = DetectedProblems::GetSingleton();
            detectedProblems.CheckForSubModsSharingPriority();
            detectedProblems.CheckForSubModsWithInvalidConditions();
            UpdateAnimations();
            return true;
        }
    }

    if (!bDeserializeSuccess && bOriginallyLegacy) {
        ResetToLegacy();
        if (directoryPathStr.find("_CustomConditions"sv) != std::string::npos) {
            if (auto txtPath = directoryPath / "_conditions.txt"sv; exists(txtPath)) {
                auto newConditionSet = Parsing::ParseConditionsTxt(txtPath);
                _conditionSet->MoveConditions(newConditionSet.get());

                SetDirtyRecursive(false);

                auto& detectedProblems = DetectedProblems::GetSingleton();
                detectedProblems.CheckForSubModsSharingPriority();
                detectedProblems.CheckForSubModsWithInvalidConditions();

                return true;
            }
        } else {
            // no conditions.txt, so this is a legacy mod with plugin name and formid
            auto formIDString = directoryPath.stem().string();
            auto fileString = directoryPath.parent_path().stem().string();
            auto extensionString = directoryPath.parent_path().extension().string();
            auto modName = fileString + extensionString;

            RE::FormID formID;
            auto newConditionSet = std::make_shared<Conditions::ConditionSet>();
            std::from_chars(formIDString.data(), formIDString.data() + formIDString.size(), formID, 16);
            if (auto form = Utils::LookupForm(formID, modName)) {
                std::string argument = modName + "|" + formIDString;
                auto condition = OpenAnimationReplacer::GetSingleton().CreateCondition("IsActorBase");
                static_cast<Conditions::IsActorBaseCondition*>(condition.get())->formComponent->SetTESFormValue(form);
                newConditionSet->AddCondition(condition);
            }
            _conditionSet->MoveConditions(newConditionSet.get());

            SetDirtyRecursive(false);

            auto& detectedProblems = DetectedProblems::GetSingleton();
            detectedProblems.CheckForSubModsSharingPriority();
            detectedProblems.CheckForSubModsWithInvalidConditions();

            return true;
        }
    }

    return false;
}

void SubMod::SaveConfig(ConditionEditMode a_editMode, bool a_bResetDirty/* = true*/)
{
    if (a_editMode == ConditionEditMode::kNone) {
        return;
    }

    const auto prevSource = _configSource;

    std::filesystem::path jsonPath(_path);

    switch (a_editMode) {
    case ConditionEditMode::kAuthor:
        jsonPath = jsonPath / "config.json"sv;
        _configSource = Parsing::ConfigSource::kAuthor;
        break;
    case ConditionEditMode::kUser:
        jsonPath = jsonPath / "user.json"sv;
        _configSource = Parsing::ConfigSource::kUser;
        break;
    }

    rapidjson::Document doc(rapidjson::kObjectType);

    Serialize(doc, a_editMode);

    Parsing::SerializeJson(jsonPath, doc);

    if (a_bResetDirty) {
        SetDirtyRecursive(false);
    } else {
        _configSource = prevSource;
    }
}

void SubMod::Serialize(rapidjson::Document& a_doc, ConditionEditMode a_editMode) const
{
    rapidjson::Value::AllocatorType& allocator = a_doc.GetAllocator();

    if (a_editMode == ConditionEditMode::kAuthor) {
        // write submod name
        rapidjson::Value nameValue(rapidjson::StringRef(_name.data(), _name.length()));
        a_doc.AddMember("name", nameValue, allocator);

        // write submod description
        if (!_description.empty()) {
            rapidjson::Value descriptionValue(rapidjson::StringRef(_description.data(), _description.length()));
            a_doc.AddMember("description", descriptionValue, allocator);
        }
    }

    // write submod priority
    rapidjson::Value priorityValue(_priority);
    a_doc.AddMember("priority", priorityValue, allocator);

    // write submod disabled
    if (_bDisabled) {
        rapidjson::Value disabledValue(_bDisabled);
        a_doc.AddMember("disabled", disabledValue, allocator);
    }

    // write replacement anim datas (only save those that have non-default settings)
	rapidjson::Value replacementAnimationDatasValue(rapidjson::kArrayType);
	for (auto& replacementAnimation : _replacementAnimations) {
		if (replacementAnimation->ShouldSaveToJson()) {
			rapidjson::Value animValue(rapidjson::kObjectType);

			animValue.AddMember("projectName", rapidjson::StringRef(replacementAnimation->GetProjectName().data(), replacementAnimation->GetProjectName().length()), allocator);
			animValue.AddMember("path", rapidjson::StringRef(replacementAnimation->GetAnimPath().data(), replacementAnimation->GetAnimPath().length()), allocator);
			if (replacementAnimation->GetDisabled()) {
				animValue.AddMember("disabled", replacementAnimation->GetDisabled(), allocator);
			}

			if (replacementAnimation->HasVariants()) {
				rapidjson::Value variantsValue(rapidjson::kArrayType);
				replacementAnimation->ForEachVariant([&](const ReplacementAnimation::Variant& a_variant) {
					if (a_variant.ShouldSaveToJson()) {
						rapidjson::Value variantValue(rapidjson::kObjectType);

						variantValue.AddMember("filename", rapidjson::StringRef(a_variant.GetFilename().data(), a_variant.GetFilename().length()), allocator);
						if (a_variant.GetWeight() != 1.f) {
							variantValue.AddMember("weight", a_variant.GetWeight(), allocator);
						}
						if (a_variant.IsDisabled()) {
							variantValue.AddMember("disabled", a_variant.IsDisabled(), allocator);
						}

						variantsValue.PushBack(variantValue, allocator);
					}

					return RE::BSVisit::BSVisitControl::kContinue;
				});

				if (!variantsValue.Empty()) {
					animValue.AddMember("variants", variantsValue, allocator);
				}
			}

			replacementAnimationDatasValue.PushBack(animValue, allocator);
		}
	}
	if (!replacementAnimationDatasValue.Empty()) {
		a_doc.AddMember("replacementAnimDatas", replacementAnimationDatasValue, allocator);
	}

    // write override animations folder
    if (!_overrideAnimationsFolder.empty()) {
        rapidjson::Value overrideAnimationsFolderValue(rapidjson::StringRef(_overrideAnimationsFolder.data(), _overrideAnimationsFolder.length()));
        a_doc.AddMember("overrideAnimationsFolder", overrideAnimationsFolderValue, allocator);
    }

    // write required project name
    if (!_requiredProjectName.empty()) {
        rapidjson::Value requiredProjectNameValue(rapidjson::StringRef(_requiredProjectName.data(), _requiredProjectName.length()));
        a_doc.AddMember("requiredProjectName", requiredProjectNameValue, allocator);
    }

    // write ignore DONT_CONVERT_ANNOTATIONS_TO_TRIGGERS flag
    if (_bIgnoreDontConvertAnnotationsToTriggersFlag) {
        rapidjson::Value ignoreNoTriggersValue(_bIgnoreDontConvertAnnotationsToTriggersFlag);
        a_doc.AddMember("ignoreDontConvertAnnotationsToTriggersFlag", ignoreNoTriggersValue, allocator);
    }

	// write triggers from annotations only
	if (_bTriggersFromAnnotationsOnly) {
		rapidjson::Value triggersOnlyValue(_bTriggersFromAnnotationsOnly);
		a_doc.AddMember("triggersFromAnnotationsOnly", triggersOnlyValue, allocator);
	}

    // write interruptible
    if (_bInterruptible) {
        rapidjson::Value interruptibleValue(_bInterruptible);
        a_doc.AddMember("interruptible", interruptibleValue, allocator);
    }

	// write replace on loop (true is default so skip)
	if (!_bReplaceOnLoop) {
		rapidjson::Value interruptibleValue(_bReplaceOnLoop);
		a_doc.AddMember("replaceOnLoop", interruptibleValue, allocator);
	}

	// write replace on echo
	if (_bReplaceOnEcho) {
		rapidjson::Value interruptibleValue(_bReplaceOnEcho);
		a_doc.AddMember("replaceOnEcho", interruptibleValue, allocator);
	}

    // write keep random result on loop
    if (_bKeepRandomResultsOnLoop) {
        rapidjson::Value keepRandomResultsOnLoopValue(_bKeepRandomResultsOnLoop);
        a_doc.AddMember("keepRandomResultsOnLoop", keepRandomResultsOnLoopValue, allocator);
    }

	// write share random results
	if (_bShareRandomResults) {
		rapidjson::Value shareRandomResultsValue(_bShareRandomResults);
		a_doc.AddMember("shareRandomResults", shareRandomResultsValue, allocator);
	}

    // write conditions
    rapidjson::Value conditionArrayValue = _conditionSet->Serialize(allocator);
    a_doc.AddMember("conditions", conditionArrayValue, allocator);

	// write paired conditions
	if (_synchronizedConditionSet) {
	    rapidjson::Value pairedConditionArrayValue = _synchronizedConditionSet->Serialize(allocator);
        a_doc.AddMember("pairedConditions", pairedConditionArrayValue, allocator);
	}
}

std::string SubMod::SerializeToString() const
{
    rapidjson::Document doc(rapidjson::kObjectType);

    Serialize(doc, ConditionEditMode::kAuthor);

    return Parsing::SerializeJsonToString(doc);
}

ReplacerMod* SubMod::GetParentMod() const
{
    return _parentMod;
}

void SubMod::ResetToLegacy()
{
    const std::filesystem::path directoryPath(_path);

    if (_path.find("_CustomConditions"sv) != std::string::npos) {
        _name = directoryPath.stem().string();
        std::from_chars(_name.data(), _name.data() + _name.length(), _priority);
        _configSource = Parsing::ConfigSource::kLegacy;
    } else {
        const std::string fileString = directoryPath.parent_path().stem().string();
        const std::string extensionString = directoryPath.parent_path().extension().string();

        const std::string modName = fileString + extensionString;
        const std::string formIDString = directoryPath.stem().string();
        _name = modName;
        _name += '|';
        _name += formIDString;
        _priority = 0;
        _configSource = Parsing::ConfigSource::kLegacyActorBase;
    }

    _description = "";
    _bDisabled = false;
    _overrideAnimationsFolder = "";
    _requiredProjectName = "";
    _bIgnoreDontConvertAnnotationsToTriggersFlag = false;
	_bTriggersFromAnnotationsOnly = false;
    _bInterruptible = false;
	_bReplaceOnLoop = true;
	_bReplaceOnEcho = false;
    _bKeepRandomResultsOnLoop = false;
	_bShareRandomResults = false;

	ResetReplacementAnimationsToLegacy();

    SetDirty(false);

    UpdateAnimations();
}

void SubMod::ResetReplacementAnimationsToLegacy()
{
	for (const auto& anim : _replacementAnimations) {
		anim->SetDisabled(false);
		anim->ResetVariants();
		anim->SetIgnoreDontConvertAnnotationsToTriggersFlag(_bIgnoreDontConvertAnnotationsToTriggersFlag);
		anim->SetTriggersFromAnnotationsOnly(_bTriggersFromAnnotationsOnly);
		anim->SetInterruptible(_bInterruptible);
		anim->SetReplaceOnLoop(_bReplaceOnLoop);
		anim->SetReplaceOnEcho(_bReplaceOnEcho);
		anim->SetKeepRandomResultsOnLoop(_bKeepRandomResultsOnLoop);
		anim->SetShareRandomResults(_bShareRandomResults);
		anim->UpdateVariantCache();
	}
}

void SubMod::AddReplacerProject(ReplacerProjectData* a_replacerProject)
{
    {
		ReadLocker locker(_dataLock);

		if (std::ranges::find(_replacerProjects, a_replacerProject) != _replacerProjects.end()) {
		    return;
		}
    }

    WriteLocker locker(_dataLock);

    _replacerProjects.emplace_back(a_replacerProject);

    // sort replacer projects by name
    std::ranges::sort(_replacerProjects, [](const auto& a_lhs, const auto& a_rhs) {
        return a_lhs->stringData->name.data() < a_rhs->stringData->name.data();
    });
}

void SubMod::ForEachReplacerProject(const std::function<void(ReplacerProjectData*)>& a_func) const
{
    ReadLocker locker(_dataLock);

    for (auto& replacerProject : _replacerProjects) {
        a_func(replacerProject);
    }
}

void SubMod::ForEachReplacementAnimation(const std::function<void(ReplacementAnimation*)>& a_func) const
{
    ReadLocker locker(_dataLock);

    for (auto& replacementAnimation : _replacementAnimations) {
        a_func(replacementAnimation);
    }
}

void SubMod::ForEachReplacementAnimationFile(const std::function<void(const ReplacementAnimationFile&)>& a_func) const
{
	ReadLocker locker(_dataLock);

	std::map<std::string, const ReplacementAnimationFile*> sortedReplacementAnimationFiles;

    for (const auto& entry : _replacementAnimationFiles) {
		sortedReplacementAnimationFiles.emplace(entry.first.string(), &entry.second);
    }

	for (const auto& entry : sortedReplacementAnimationFiles | std::views::values) {
	    a_func(*entry);
    }
}

float SubMod::GetSharedRandom(ActiveClip* a_activeClip, const Conditions::IRandomConditionComponent* a_randomComponent)
{
	{
	    ReadLocker locker(_randomLock);

		if (const auto search = _sharedRandomFloats.find(a_activeClip->GetBehaviorGraph()); search != _sharedRandomFloats.end()) {
			return search->second.GetRandomFloat(a_activeClip, a_randomComponent);
		}
	}

	{
		WriteLocker locker(_randomLock);

		const auto behaviorGraph = a_activeClip->GetBehaviorGraph();

		auto [it, bSuccess] = _sharedRandomFloats.try_emplace(behaviorGraph, this, behaviorGraph);
		return it->second.GetRandomFloat(a_activeClip, a_randomComponent);
	}
}

float SubMod::GetVariantRandom(ActiveClip* a_activeClip)
{
	{
		ReadLocker locker(_randomLock);

		if (const auto search = _sharedRandomFloats.find(a_activeClip->GetBehaviorGraph()); search != _sharedRandomFloats.end()) {
			return search->second.GetVariantFloat(a_activeClip);
		}
	}

	{
		WriteLocker locker(_randomLock);

		const auto behaviorGraph = a_activeClip->GetBehaviorGraph();

		auto [it, bSuccess] = _sharedRandomFloats.try_emplace(behaviorGraph, this, behaviorGraph);
		return it->second.GetVariantFloat(a_activeClip);
	}
}

void SubMod::ClearSharedRandom(const RE::hkbBehaviorGraph* a_behaviorGraph)
{
	WriteLocker locker(_randomLock);

	_sharedRandomFloats.erase(a_behaviorGraph);
}

float SubMod::SharedRandomFloats::GetRandomFloat(ActiveClip* a_activeClip, const Conditions::IRandomConditionComponent* a_randomComponent)
{
	AddActiveClip(a_activeClip);

	// Returns a saved random float if it exists, otherwise generates a new one and saves it
	{
		ReadLocker locker(_randomLock);
		const auto search = _randomFloats.find(a_randomComponent);
		if (search != _randomFloats.end()) {
			return search->second;
		}
	}

	WriteLocker locker(_randomLock);
	float randomFloat = Utils::GetRandomFloat(a_randomComponent->GetMinValue(), a_randomComponent->GetMaxValue());
	_randomFloats.emplace(a_randomComponent, randomFloat);

	return randomFloat;
}

float SubMod::SharedRandomFloats::GetVariantFloat(ActiveClip* a_activeClip)
{
	AddActiveClip(a_activeClip);

	// Returns a saved variant float if it exists, otherwise generates a new one and saves it
	{
		ReadLocker locker(_randomLock);
		if (_variantFloat) {
		    return *_variantFloat;
		}
	}

	WriteLocker locker(_randomLock);
	_variantFloat = Utils::GetRandomFloat(0.f, 1.f);

	return *_variantFloat;
}

void SubMod::SharedRandomFloats::AddActiveClip(ActiveClip* a_activeClip)
{
	Locker locker(_clipLock);

	auto [it, bSuccess] = _registeredCallbacks.try_emplace(a_activeClip, std::make_shared<ActiveClip::DestroyedCallback>([&](auto a_destroyedClip) {
		RemoveActiveClip(a_destroyedClip);
	}));

	if (bSuccess) {
		auto weakPtr = std::weak_ptr(it->second);
		a_activeClip->RegisterDestroyedCallback(weakPtr);
	}

	// destroy queued removal job if any exists
	_queuedRemovalJob = nullptr;
}

void SubMod::SharedRandomFloats::RemoveActiveClip(ActiveClip* a_activeClip)
{
	Locker locker(_clipLock);

	_registeredCallbacks.erase(a_activeClip);

	// queue for removal
	if (_registeredCallbacks.empty()) {
		_queuedRemovalJob = std::make_shared<Jobs::RemoveSharedRandomFloatJob>(Settings::fSharedRandomLifetime, _parentSubMod, _behaviorGraph);
		OpenAnimationReplacer::GetSingleton().QueueWeakLatentJob(_queuedRemovalJob);
	}
}

void ReplacerMod::SetName(std::string_view a_name)
{
    const auto previousName = _name;
    _name = a_name;

    OpenAnimationReplacer::GetSingleton().OnReplacerModNameChanged(previousName, this);
}

void ReplacerMod::SaveConfig(ConditionEditMode a_editMode)
{
    if (a_editMode == ConditionEditMode::kNone) {
        return;
    }

    std::filesystem::path jsonPath(_path);

    switch (a_editMode) {
    case ConditionEditMode::kAuthor:
        jsonPath = jsonPath / "config.json"sv;
        break;
    case ConditionEditMode::kUser:
        jsonPath = jsonPath / "user.json"sv;
        break;
    }

    rapidjson::Document doc(rapidjson::kObjectType);

    Serialize(doc);

    Parsing::SerializeJson(jsonPath, doc);

    SetDirty(false);
}

void ReplacerMod::Serialize(rapidjson::Document& a_doc) const
{
    rapidjson::Value::AllocatorType& allocator = a_doc.GetAllocator();

    // write mod name
    rapidjson::Value nameValue(rapidjson::StringRef(_name.data(), _name.length()));
    a_doc.AddMember("name", nameValue, allocator);

    // write mod author
    rapidjson::Value authorValue(rapidjson::StringRef(_author.data(), _author.length()));
    a_doc.AddMember("author", authorValue, allocator);

    // write mod description
    rapidjson::Value descriptionValue(rapidjson::StringRef(_description.data(), _description.length()));
    a_doc.AddMember("description", descriptionValue, allocator);
}

void ReplacerMod::AddSubMod(std::unique_ptr<SubMod>& a_subMod)
{
    WriteLocker locker(_dataLock);

    a_subMod->_parentMod = this;

    const auto insertPos = std::ranges::upper_bound(_subMods, a_subMod, [](const auto& a_lhs, const auto& a_rhs) {
        return a_lhs->GetPriority() > a_rhs->GetPriority();
    });

    _subMods.insert(insertPos, std::move(a_subMod));
}

bool ReplacerMod::HasSubMod(std::string_view a_path) const
{
    ReadLocker locker(_dataLock);

    return GetSubMod(a_path) != nullptr;
}

SubMod* ReplacerMod::GetSubMod(std::string_view a_path) const
{
    ReadLocker locker(_dataLock);

    const auto it = std::ranges::find_if(_subMods, [&](const auto& a_subMod) {
        return a_subMod->_path == a_path;
    });

    if (it != _subMods.end()) {
        return (*it).get();
    }

    return nullptr;
}

RE::BSVisit::BSVisitControl ReplacerMod::ForEachSubMod(const std::function<RE::BSVisit::BSVisitControl(SubMod*)>& a_func) const
{
    using Result = RE::BSVisit::BSVisitControl;

    ReadLocker locker(_dataLock);

    for (auto& subMod : _subMods) {
        const auto result = a_func(subMod.get());
        if (result == Result::kStop) {
            return result;
        }
    }

    return Result::kContinue;
}

void ReplacerMod::SortSubMods()
{
    WriteLocker locker(_dataLock);

    std::ranges::sort(_subMods, [](const auto& a_lhs, const auto& a_rhs) {
        return a_lhs->GetPriority() > a_rhs->GetPriority();
    });
}

ReplacementAnimation* AnimationReplacements::EvaluateConditionsAndGetReplacementAnimation(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const
{
    ReadLocker locker(_lock);

    if (!_replacements.empty()) {
        for (auto& replacementAnimation : _replacements) {
            if (replacementAnimation->EvaluateConditions(a_refr, a_clipGenerator)) {
                return replacementAnimation.get();
            }
        }
    }

    return nullptr;
}

ReplacementAnimation* AnimationReplacements::EvaluateSynchronizedConditionsAndGetReplacementAnimation(RE::TESObjectREFR* a_sourceRefr, RE::TESObjectREFR* a_targetRefr, RE::hkbClipGenerator* a_clipGenerator) const
{
	ReadLocker locker(_lock);

	if (!_replacements.empty()) {
		for (auto& replacementAnimation : _replacements) {
			if (replacementAnimation->EvaluateSynchronizedConditions(a_sourceRefr, a_targetRefr, a_clipGenerator)) {
				return replacementAnimation.get();
			}
		}
	}

	return nullptr;
}

void AnimationReplacements::AddReplacementAnimation(std::unique_ptr<ReplacementAnimation>& a_replacementAnimation)
{
    WriteLocker locker(_lock);

    _replacements.emplace_back(std::move(a_replacementAnimation));
}

void AnimationReplacements::SortByPriority()
{
    WriteLocker locker(_lock);

    if (!_replacements.empty()) {
        std::ranges::sort(_replacements, [](const auto& a_lhs, const auto& a_rhs) {
            return a_lhs->GetPriority() > a_rhs->GetPriority();
        });
    }
}

void AnimationReplacements::ForEachReplacementAnimation(const std::function<void(ReplacementAnimation*)>& a_func, bool a_bReverse /*= false*/) const
{
    ReadLocker locker(_lock);

    if (a_bReverse) {
        for (const auto& replacementAnimation : std::ranges::reverse_view(_replacements)) {
            a_func(replacementAnimation.get());
        }
    } else {
        for (auto& replacementAnimation : _replacements) {
            a_func(replacementAnimation.get());
        }
    }
}

void AnimationReplacements::TestInterruptible()
{
    ReadLocker locker(_lock);

    for (const auto& replacementAnimation : _replacements) {
        if (replacementAnimation->GetInterruptible()) {
            if (!_bOriginalInterruptible) {
                logger::info("original animation {} will be treated as interruptible because there are interruptible potential replacements", _originalPath);
            }
            _bOriginalInterruptible = true;
            return;
        }
    }

    _bOriginalInterruptible = false;
}

void AnimationReplacements::TestReplaceOnEcho()
{
	ReadLocker locker(_lock);

	for (const auto& replacementAnimation : _replacements) {
		if (replacementAnimation->GetReplaceOnEcho()) {
			if (!_bOriginalReplaceOnEcho) {
				logger::info("original animation {} will replace on echo because there are potential replacements that do", _originalPath);
			}
			_bOriginalReplaceOnEcho = true;
			return;
		}
	}

	_bOriginalReplaceOnEcho = false;
}

void AnimationReplacements::TestKeepRandomResultsOnLoop()
{
    ReadLocker locker(_lock);

    for (const auto& replacementAnimation : _replacements) {
        if (replacementAnimation->GetKeepRandomResultsOnLoop()) {
            if (!_bOriginalKeepRandomResultsOnLoop) {
                logger::info("original animation {} will keep random condition results on loop because there are potential replacements that do", _originalPath);
            }
            _bOriginalKeepRandomResultsOnLoop = true;
            return;
        }
    }

    _bOriginalKeepRandomResultsOnLoop = false;
}

void AnimationReplacements::MarkAsSynchronizedAnimation(bool a_bSynchronized)
{
	_bSynchronized = a_bSynchronized;

	ReadLocker locker(_lock);

	for (const auto& replacementAnimation : _replacements) {
		replacementAnimation->MarkAsSynchronizedAnimation(a_bSynchronized);
	}
}

ReplacementAnimation* ReplacerProjectData::EvaluateConditionsAndGetReplacementAnimation(RE::hkbClipGenerator* a_clipGenerator, uint16_t a_originalIndex, RE::TESObjectREFR* a_refr) const
{
    if (const auto replacementAnimations = GetAnimationReplacements(a_originalIndex)) {
        return replacementAnimations->EvaluateConditionsAndGetReplacementAnimation(a_refr, a_clipGenerator);
    }

    return nullptr;
}

uint16_t ReplacerProjectData::GetOriginalAnimationIndex(uint16_t a_currentIndex) const
{
    if (const auto it = replacementIndexToOriginalIndexMap.find(a_currentIndex); it != replacementIndexToOriginalIndexMap.end()) {
        return it->second;
    }
    return a_currentIndex;
}

uint16_t ReplacerProjectData::TryAddAnimationToAnimationBundleNames(std::string_view a_path, const std::optional<std::string>& a_hash)
{
	std::optional<std::string> hash = std::nullopt;

    // Check hash
	if (Settings::bFilterOutDuplicateAnimations && a_hash) {
		hash = a_hash;

		if (const auto search = _fileHashToIndexMap.find(*hash); search != _fileHashToIndexMap.end()) {
			++_filteredDuplicates;
			return search->second;
		}
    }

    // Check if the animation is already in the list and return the index if it is
    for (uint16_t i = 0; i < stringData->animationNames.size(); i++) {
		if (stringData->animationNames[i].data() == a_path) {
            return i;
        }
    }

    // Check if the animation can be added to the list
    const auto newIndex = static_cast<uint16_t>(stringData->animationNames.size());
    if (newIndex >= Settings::uAnimationLimit) {
        logger::error("array filled to the max! not adding");
        Utils::ErrorTooManyAnimations();
        return static_cast<uint16_t>(-1);
    }

    // Add the animation to the list
	stringData->animationNames.push_back(a_path.data());

    if (Settings::bFilterOutDuplicateAnimations && hash) {
        _fileHashToIndexMap[*hash] = newIndex;
    }

    return newIndex;
}

void ReplacerProjectData::AddReplacementAnimation(RE::hkbCharacterStringData* a_stringData, uint16_t a_originalIndex, std::unique_ptr<ReplacementAnimation>& a_replacementAnimation)
{
	auto addReplacementIndex = [&](uint16_t a_index) {
		replacementIndexToOriginalIndexMap.emplace(a_index, a_originalIndex);
		animationsToQueue.emplace_back(a_index);
	};

	if (a_replacementAnimation->HasVariants()) {
		a_replacementAnimation->ForEachVariant([&](const ReplacementAnimation::Variant& a_variant) {
			addReplacementIndex(a_variant.GetIndex());
			return RE::BSVisit::BSVisitControl::kContinue;
		});
	} else {
		const uint16_t replacementIndex = a_replacementAnimation->GetIndex();
		if (replacementIndex != static_cast<uint16_t>(-1)) {
			addReplacementIndex(replacementIndex);
		}
	}

	if (const auto it = originalIndexToAnimationReplacementsMap.find(a_originalIndex); it != originalIndexToAnimationReplacementsMap.end()) {
		it->second->AddReplacementAnimation(a_replacementAnimation);
	} else {
		auto newReplacementAnimations = std::make_unique<AnimationReplacements>(Utils::GetOriginalAnimationName(a_stringData, a_originalIndex));
		newReplacementAnimations->AddReplacementAnimation(a_replacementAnimation);
		originalIndexToAnimationReplacementsMap.emplace(a_originalIndex, std::move(newReplacementAnimations));
	}
}

void ReplacerProjectData::SortReplacementAnimationsByPriority(uint16_t a_originalIndex)
{
    if (const auto it = originalIndexToAnimationReplacementsMap.find(a_originalIndex); it != originalIndexToAnimationReplacementsMap.end()) {
        const auto& replacementAnimations = it->second;
        replacementAnimations->SortByPriority();
    }
}

void ReplacerProjectData::QueueReplacementAnimations(RE::hkbCharacter* a_character)
{
    OpenAnimationReplacer::bIsPreLoading = true;
    for (const auto& animIndex : animationsToQueue) {
        OpenAnimationReplacer::LoadAnimation(a_character, animIndex);
    }
    OpenAnimationReplacer::bIsPreLoading = false;

    animationsToQueue.clear();
}

void ReplacerProjectData::MarkSynchronizedReplacementAnimations(RE::hkbGenerator* a_rootGenerator)
{
	if (!a_rootGenerator) {
	    return;
	}

	RE::hkbNode::GetChildrenFlagBits getChildrenFlags = RE::hkbNode::GetChildrenFlagBits::kGeneratorsOnly;

	RE::UnkIteratorStruct iter;
	UnkNodeIterator_ctor(&iter, getChildrenFlags, a_rootGenerator);

	std::unordered_set<uint16_t> synchronizedClipIndexes;

    if (auto node = UnkNodeIterator_GetNext(&iter)) {
	    do {
			const auto classType = node->GetClassType();
			if (classType->name == *g_str_BSSynchronizedClipGenerator) {
				const auto synchronizedClipGenerator = static_cast<RE::BSSynchronizedClipGenerator*>(node);
				synchronizedClipIndexes.emplace(synchronizedClipGenerator->clipGenerator->animationBindingIndex);
			}
			node = UnkNodeIterator_GetNext(&iter);
	    } while (node);
	}

	for (const auto& index : synchronizedClipIndexes) {
	    if (const auto replacementAnimations = GetAnimationReplacements(index)) {
	        replacementAnimations->MarkAsSynchronizedAnimation(true);
        }
	}
}

AnimationReplacements* ReplacerProjectData::GetAnimationReplacements(uint16_t a_originalIndex) const
{
    if (const auto search = originalIndexToAnimationReplacementsMap.find(a_originalIndex); search != originalIndexToAnimationReplacementsMap.end()) {
        return search->second.get();
    }

    return nullptr;
}

void ReplacerProjectData::ForEach(const std::function<void(AnimationReplacements*)>& a_func)
{
    for (auto& val : originalIndexToAnimationReplacementsMap | std::views::values) {
        auto& replacementAnimations = val;
        a_func(replacementAnimations.get());
    }
}
