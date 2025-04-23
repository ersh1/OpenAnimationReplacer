#include "ReplacerMods.h"

#include <ranges>

#include "DetectedProblems.h"
#include "Offsets.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"

#include <unordered_set>

bool SubMod::StateDataUpdate(float a_deltaTime)
{
	return conditionStateData.UpdateData(a_deltaTime);
}

bool SubMod::StateDataOnLoopOrEcho(RE::ObjectRefHandle a_refHandle, ActiveClip* a_activeClip, bool a_bIsEcho)
{
	return conditionStateData.OnLoopOrEcho(a_refHandle, a_activeClip, a_bIsEcho);
}

bool SubMod::StateDataClearRefrData(RE::ObjectRefHandle a_refHandle)
{
	return conditionStateData.ClearRefrData(a_refHandle);
}

void SubMod::StateDataClearData()
{
	conditionStateData.Clear();
}

bool SubMod::AddReplacementAnimation(std::string_view a_animPath, uint16_t a_originalIndex, ReplacerProjectData* a_replacerProjectData, RE::hkbCharacterStringData* a_stringData)
{
	bool bAdded = false;

	if (const auto search = _replacementAnimationFiles.find(a_animPath.data()); search != _replacementAnimationFiles.end()) {
		std::unique_ptr<ReplacementAnimation> newReplacementAnimation = nullptr;

		auto& animFile = search->second;
		if (animFile.variants) {
			std::vector<Variant> variants;
			int32_t i = 0;
			for (auto& variantToAdd : *animFile.variants) {
				if (uint16_t newIndex = a_replacerProjectData->TryAddAnimationToAnimationBundleNames(variantToAdd.fullPath, variantToAdd.hash); newIndex != static_cast<uint16_t>(-1)) {
					variants.emplace_back(newIndex, Utils::GetFileNameWithExtension(variantToAdd.fullPath), i++);
				}
			}

			newReplacementAnimation = std::make_unique<ReplacementAnimation>(variants, a_originalIndex, animFile.fullPath, a_stringData->name.data(), _conditionSet.get());
		} else if (uint16_t newIndex = a_replacerProjectData->TryAddAnimationToAnimationBundleNames(animFile.fullPath, animFile.hash); newIndex != static_cast<uint16_t>(-1)) {
			newReplacementAnimation = std::make_unique<ReplacementAnimation>(newIndex, a_originalIndex, animFile.fullPath, a_stringData->name.data(), _conditionSet.get());
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
				return a_replacementAnimData.projectName == a_stringData->name.data() && a_replacementAnimData.path == animFile.fullPath;
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
	_bCustomBlendTimeOnInterrupt = a_parseResult.bCustomBlendTimeOnInterrupt;
	_blendTimeOnInterrupt = a_parseResult.blendTimeOnInterrupt;
	_bReplaceOnLoop = a_parseResult.bReplaceOnLoop;
	_bCustomBlendTimeOnLoop = a_parseResult.bCustomBlendTimeOnLoop;
	_blendTimeOnLoop = a_parseResult.blendTimeOnLoop;
	_bReplaceOnEcho = a_parseResult.bReplaceOnEcho;
	_bCustomBlendTimeOnEcho = a_parseResult.bCustomBlendTimeOnEcho;
	_blendTimeOnEcho = a_parseResult.blendTimeOnEcho;
	_bKeepRandomResultsOnLoop_DEPRECATED = a_parseResult.bKeepRandomResultsOnLoop_DEPRECATED;
	_bShareRandomResults_DEPRECATED = a_parseResult.bShareRandomResults_DEPRECATED;
	_conditionSet->MoveConditions(a_parseResult.conditionSet.get());
	if (a_parseResult.synchronizedConditionSet) {
		SetHasSynchronizedAnimations();
		_synchronizedConditionSet->MoveConditions(a_parseResult.synchronizedConditionSet.get());
	}

	HandleDeprecatedSettings();

	LoadReplacementAnimationDatas(_replacementAnimDatas);

	RestorePresetReferences();

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

void SubMod::HandleDeprecatedSettings() const
{
	// need to set relevant values in variants
	if (_bKeepRandomResultsOnLoop_DEPRECATED || _bShareRandomResults_DEPRECATED) {
		for (auto& anim : _replacementAnimations) {
			if (anim->HasVariants()) {
				auto& variants = anim->GetVariants();
				if (_bKeepRandomResultsOnLoop_DEPRECATED) {
					variants.SetShouldResetRandomOnLoopOrEcho(!_bKeepRandomResultsOnLoop_DEPRECATED);
				}
				if (_bShareRandomResults_DEPRECATED) {
					variants.SetVariantStateScope(Conditions::StateDataScope::kSubMod);
				}
			}
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
			a_animReplacements->SortByPriority();
		});
	});
}

RE::BSVisit::BSVisitControl TryRestorePreset(std::unique_ptr<Conditions::ICondition>& a_condition)
{
	if (a_condition->GetConditionType() == Conditions::ConditionType::kPreset) {
		const auto presetCondition = static_cast<Conditions::PRESETCondition*>(a_condition.get());
		presetCondition->conditionsComponent->TryFindPreset();
	} else {
		for (uint32_t i = 0; i < a_condition->GetNumComponents(); ++i) {
			const auto& component = a_condition->GetComponent(i);
			if (component->GetType() == Conditions::ConditionComponentType::kMulti) {
				auto multiConditionComponent = static_cast<Conditions::IMultiConditionComponent*>(component);
				multiConditionComponent->ForEachCondition(TryRestorePreset);
			}
		}
	}

	return RE::BSVisit::BSVisitControl::kContinue;
}

void SubMod::RestorePresetReferences()
{
	auto restorePresetReferences = [&](Conditions::ConditionSet* a_conditionSet) {
		if (const auto conditionSet = a_conditionSet) {
			conditionSet->ForEachCondition(TryRestorePreset);
		}
	};

	restorePresetReferences(_conditionSet.get());
	restorePresetReferences(_synchronizedConditionSet.get());
}

bool SubMod::HasCustomBlendTime(CustomBlendType a_type) const
{
	switch (a_type) {
	case CustomBlendType::kInterrupt:
		return _bCustomBlendTimeOnInterrupt;
	case CustomBlendType::kLoop:
		return _bCustomBlendTimeOnLoop;
	case CustomBlendType::kEcho:
		return _bCustomBlendTimeOnEcho;
	}

	return false;
}

float SubMod::GetCustomBlendTime(CustomBlendType a_type) const
{
	switch (a_type) {
	case CustomBlendType::kInterrupt:
		return _bCustomBlendTimeOnInterrupt ? _blendTimeOnInterrupt : Settings::fDefaultBlendTimeOnInterrupt;
	case CustomBlendType::kLoop:
		return _bCustomBlendTimeOnLoop ? _blendTimeOnLoop : Settings::fDefaultBlendTimeOnLoop;
	case CustomBlendType::kEcho:
		return _bCustomBlendTimeOnEcho ? _blendTimeOnEcho : Settings::fDefaultBlendTimeOnEcho;
	}

	return 0.f;
}

void SubMod::ToggleCustomBlendTime(CustomBlendType a_type, bool a_bEnable)
{
	switch (a_type) {
	case CustomBlendType::kInterrupt:
		_bCustomBlendTimeOnInterrupt = a_bEnable;
		break;
	case CustomBlendType::kLoop:
		_bCustomBlendTimeOnLoop = a_bEnable;
		break;
	case CustomBlendType::kEcho:
		_bCustomBlendTimeOnEcho = a_bEnable;
		break;
	}
}

void SubMod::SetCustomBlendTime(CustomBlendType a_type, float a_value)
{
	switch (a_type) {
	case CustomBlendType::kInterrupt:
		_blendTimeOnInterrupt = a_value;
		break;
	case CustomBlendType::kLoop:
		_blendTimeOnLoop = a_value;
		break;
	case CustomBlendType::kEcho:
		_blendTimeOnEcho = a_value;
		break;
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
	bool bOriginallyLegacy = Utils::ContainsStringIgnoreCase(directoryPathStr, "DynamicAnimationReplacer"sv);

	const auto configPath = directoryPath / "config.json"sv;
	const auto userPath = directoryPath / "user.json"sv;
	const bool bConfigJsonFound = Utils::IsRegularFile(configPath);
	const bool bUserJsonFound = Utils::IsRegularFile(userPath);
	if (bConfigJsonFound || bUserJsonFound) {
		if (bOriginallyLegacy) {
			// if it was originally a legacy mod, only read user.json
			if (bUserJsonFound) {
				ResetToLegacy();
				parseResult.name = _name;
				parseResult.description = _description;
				parseResult.configSource = Parsing::ConfigSource::kUser;
				bDeserializeSuccess = DeserializeSubMod(userPath, Parsing::DeserializeMode::kDataOnly, parseResult);
			}
		} else {
			if (bUserJsonFound) {
				parseResult.configSource = Parsing::ConfigSource::kUser;
				if (!DeserializeSubMod(configPath, Parsing::DeserializeMode::kInfoOnly, parseResult)) {
					return false;
				}
				bDeserializeSuccess = DeserializeSubMod(userPath, Parsing::DeserializeMode::kDataOnly, parseResult);
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
		if (Utils::ContainsStringIgnoreCase(directoryPathStr, "_CustomConditions"sv)) {
			if (auto txtPath = directoryPath / "_conditions.txt"sv; Utils::Exists(txtPath)) {
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

void SubMod::SaveConfig(ConditionEditMode a_editMode, bool a_bResetDirty /* = true*/)
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
		{
			rapidjson::Value value(rapidjson::StringRef(_name.data(), _name.length()));
			a_doc.AddMember("name", value, allocator);
		}

		// write submod description
		if (!_description.empty()) {
			rapidjson::Value value(rapidjson::StringRef(_description.data(), _description.length()));
			a_doc.AddMember("description", value, allocator);
		}
	}

	// write submod priority
	{
		rapidjson::Value value(_priority);
		a_doc.AddMember("priority", value, allocator);
	}

	// write submod disabled
	if (_bDisabled) {
		rapidjson::Value value(_bDisabled);
		a_doc.AddMember("disabled", value, allocator);
	}

	// write replacement anim datas (only save those that have non-default settings)
	{
		rapidjson::Value value(rapidjson::kArrayType);
		for (auto& replacementAnimation : _replacementAnimations) {
			if (replacementAnimation->ShouldSaveToJson()) {
				rapidjson::Value animValue(rapidjson::kObjectType);

				animValue.AddMember("projectName", rapidjson::StringRef(replacementAnimation->GetProjectName().data(), replacementAnimation->GetProjectName().length()), allocator);
				animValue.AddMember("path", rapidjson::StringRef(replacementAnimation->GetAnimPath().data(), replacementAnimation->GetAnimPath().length()), allocator);
				if (replacementAnimation->GetDisabled()) {
					animValue.AddMember("disabled", replacementAnimation->GetDisabled(), allocator);
				}

				if (replacementAnimation->HasVariants()) {
					auto variantMode = replacementAnimation->GetVariants().GetVariantMode();
					auto variantStateScope = replacementAnimation->GetVariants().GetVariantStateScope();
					animValue.AddMember("variantMode", static_cast<int>(variantMode), allocator);
					animValue.AddMember("variantStateScope", static_cast<int>(variantStateScope), allocator);
					animValue.AddMember("blendBetweenVariants", replacementAnimation->GetVariants().ShouldBlendBetweenVariants(), allocator);
					animValue.AddMember("resetRandomOnLoopOrEcho", replacementAnimation->GetVariants().ShouldResetRandomOnLoopOrEcho(), allocator);
					animValue.AddMember("sharePlayedHistory", replacementAnimation->GetVariants().ShouldSharePlayedHistory(), allocator);

					rapidjson::Value variantsValue(rapidjson::kArrayType);
					replacementAnimation->ForEachVariant([&](const Variant& a_variant) {
						if (a_variant.ShouldSaveToJson(variantMode)) {
							rapidjson::Value variantValue(rapidjson::kObjectType);

							variantValue.AddMember("filename", rapidjson::StringRef(a_variant.GetFilename().data(), a_variant.GetFilename().length()), allocator);

							if (a_variant.IsDisabled()) {
								variantValue.AddMember("disabled", a_variant.IsDisabled(), allocator);
							}

							if (variantMode == VariantMode::kRandom && a_variant.GetWeight() != 1.f) {
								variantValue.AddMember("weight", a_variant.GetWeight(), allocator);
							}

							if (a_variant.ShouldPlayOnce()) {
								variantValue.AddMember("playOnce", a_variant.ShouldPlayOnce(), allocator);
							}

							variantsValue.PushBack(variantValue, allocator);
						}

						return RE::BSVisit::BSVisitControl::kContinue;
					});

					if (!variantsValue.Empty()) {
						animValue.AddMember("variants", variantsValue, allocator);
					}
				}

				value.PushBack(animValue, allocator);
			}
		}
		if (!value.Empty()) {
			a_doc.AddMember("replacementAnimDatas", value, allocator);
		}
	}

	// write override animations folder
	if (!_overrideAnimationsFolder.empty()) {
		rapidjson::Value value(rapidjson::StringRef(_overrideAnimationsFolder.data(), _overrideAnimationsFolder.length()));
		a_doc.AddMember("overrideAnimationsFolder", value, allocator);
	}

	// write required project name
	if (!_requiredProjectName.empty()) {
		rapidjson::Value value(rapidjson::StringRef(_requiredProjectName.data(), _requiredProjectName.length()));
		a_doc.AddMember("requiredProjectName", value, allocator);
	}

	// write ignore DONT_CONVERT_ANNOTATIONS_TO_TRIGGERS flag
	if (_bIgnoreDontConvertAnnotationsToTriggersFlag) {
		rapidjson::Value value(_bIgnoreDontConvertAnnotationsToTriggersFlag);
		a_doc.AddMember("ignoreDontConvertAnnotationsToTriggersFlag", value, allocator);
	}

	// write triggers from annotations only
	if (_bTriggersFromAnnotationsOnly) {
		rapidjson::Value value(_bTriggersFromAnnotationsOnly);
		a_doc.AddMember("triggersFromAnnotationsOnly", value, allocator);
	}

	// write interruptible
	if (_bInterruptible) {
		rapidjson::Value value(_bInterruptible);
		a_doc.AddMember("interruptible", value, allocator);
	}

	// write custom blend time on interrupt
	if (_bInterruptible && _bCustomBlendTimeOnInterrupt) {
		rapidjson::Value value(_bCustomBlendTimeOnInterrupt);
		a_doc.AddMember("hasCustomBlendTimeOnInterrupt", value, allocator);

		rapidjson::Value blendValue(_blendTimeOnInterrupt);
		a_doc.AddMember("blendTimeOnInterrupt", blendValue, allocator);
	}

	// write replace on loop (true is default so skip)
	if (!_bReplaceOnLoop) {
		rapidjson::Value value(_bReplaceOnLoop);
		a_doc.AddMember("replaceOnLoop", value, allocator);
	}

	// write custom blend time on loop
	if (_bReplaceOnLoop && _bCustomBlendTimeOnLoop) {
		rapidjson::Value value(_bCustomBlendTimeOnLoop);
		a_doc.AddMember("hasCustomBlendTimeOnLoop", value, allocator);

		rapidjson::Value blendValue(_blendTimeOnLoop);
		a_doc.AddMember("blendTimeOnLoop", blendValue, allocator);
	}

	// write replace on echo
	if (_bReplaceOnEcho) {
		rapidjson::Value value(_bReplaceOnEcho);
		a_doc.AddMember("replaceOnEcho", value, allocator);
	}

	// write custom blend time on echo
	if (_bReplaceOnEcho && _bCustomBlendTimeOnEcho) {
		rapidjson::Value value(_bCustomBlendTimeOnEcho);
		a_doc.AddMember("hasCustomBlendTimeOnEcho", value, allocator);

		rapidjson::Value blendValue(_blendTimeOnEcho);
		a_doc.AddMember("blendTimeOnEcho", blendValue, allocator);
	}

	// write conditions
	{
		rapidjson::Value value = _conditionSet->Serialize(allocator);
		a_doc.AddMember("conditions", value, allocator);
	}

	// write paired conditions
	if (_synchronizedConditionSet) {
		rapidjson::Value value = _synchronizedConditionSet->Serialize(allocator);
		a_doc.AddMember("pairedConditions", value, allocator);
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

	if (Utils::ContainsStringIgnoreCase(_path, "_CustomConditions"sv)) {
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
	_bCustomBlendTimeOnInterrupt = false;
	_blendTimeOnInterrupt = Settings::fDefaultBlendTimeOnInterrupt;
	_bReplaceOnLoop = true;
	_bCustomBlendTimeOnLoop = false;
	_blendTimeOnLoop = Settings::fDefaultBlendTimeOnLoop;
	_bReplaceOnEcho = false;
	_bCustomBlendTimeOnEcho = false;
	_blendTimeOnEcho = Settings::fDefaultBlendTimeOnEcho;
	_bKeepRandomResultsOnLoop_DEPRECATED = Settings::bLegacyKeepRandomResultsByDefault;
	_bShareRandomResults_DEPRECATED = false;

	ResetReplacementAnimationsToLegacy();

	SetDirty(false);

	UpdateAnimations();
}

void SubMod::ResetReplacementAnimationsToLegacy()
{
	for (const auto& anim : _replacementAnimations) {
		anim->SetDisabled(false);
		anim->ResetVariants();
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

bool ReplacerMod::StateDataUpdate(float a_deltaTime)
{
	bool bActive = false;
	if (conditionStateData.UpdateData(a_deltaTime)) {
		bActive = true;
	}
	if (variantStateData.UpdateData(a_deltaTime)) {
		bActive = true;
	}
	return bActive;
}

bool ReplacerMod::StateDataOnLoopOrEcho(RE::ObjectRefHandle a_refHandle, ActiveClip* a_activeClip, bool a_bIsEcho)
{
	bool bActive = false;
	if (conditionStateData.OnLoopOrEcho(a_refHandle, a_activeClip, a_bIsEcho)) {
		bActive = true;
	}
	if (variantStateData.OnLoopOrEcho(a_refHandle, a_activeClip, a_bIsEcho)) {
		bActive = true;
	}
	return bActive;
}

bool ReplacerMod::StateDataClearRefrData(RE::ObjectRefHandle a_refHandle)
{
	bool bActive = false;
	if (conditionStateData.ClearRefrData(a_refHandle)) {
		bActive = true;
	}
	if (variantStateData.ClearRefrData(a_refHandle)) {
		bActive = true;
	}
	return bActive;
}

void ReplacerMod::StateDataClearData()
{
	conditionStateData.Clear();
	variantStateData.Clear();
}

void ReplacerMod::LoadParseResult(Parsing::ModParseResult& a_parseResult)
{
	_name = a_parseResult.name;
	_author = a_parseResult.author;
	_description = a_parseResult.description;
	_path = a_parseResult.path;

	LoadConditionPresets(a_parseResult.conditionPresets);

	SetDirty(false);
}

void ReplacerMod::SetName(std::string_view a_name)
{
	const auto previousName = _name;
	_name = a_name;

	OpenAnimationReplacer::GetSingleton().OnReplacerModNameChanged(previousName, this);
}

bool ReplacerMod::ReloadConfig()
{
	Parsing::ModParseResult parseResult;

	std::filesystem::path directoryPath(_path);

	const auto configPath = directoryPath / "config.json"sv;
	const auto userPath = directoryPath / "user.json"sv;
	const bool bConfigJsonFound = Utils::IsRegularFile(configPath);
	const bool bUserJsonFound = Utils::IsRegularFile(userPath);

	if (bConfigJsonFound || bUserJsonFound) {
		bool bDeserializeSuccess = false;
		if (bUserJsonFound) {
			parseResult.configSource = Parsing::ConfigSource::kUser;
			if (!DeserializeMod(configPath, Parsing::DeserializeMode::kInfoOnly, parseResult)) {
				return false;
			}
			bDeserializeSuccess = DeserializeMod(userPath, Parsing::DeserializeMode::kDataOnly, parseResult);
		} else {
			parseResult.configSource = Parsing::ConfigSource::kAuthor;
			bDeserializeSuccess = DeserializeMod(configPath, Parsing::DeserializeMode::kFull, parseResult);
		}

		if (bDeserializeSuccess) {
			LoadParseResult(parseResult);
			RestorePresetReferences();
			auto& detectedProblems = DetectedProblems::GetSingleton();
			detectedProblems.CheckForReplacerModsWithInvalidConditions();
			return true;
		}
	}

	return false;
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

	// write condition presets
	rapidjson::Value presetArrayValue(rapidjson::kArrayType);
	ForEachConditionPreset([&](Conditions::ConditionPreset* a_preset) {
		rapidjson::Value conditionPresetValue(rapidjson::kObjectType);
		conditionPresetValue.AddMember("name", rapidjson::StringRef(a_preset->GetName().data(), a_preset->GetName().length()), allocator);
		const auto conditionPresetDescription = a_preset->GetDescription();
		if (!conditionPresetDescription.empty()) {
			conditionPresetValue.AddMember("description", rapidjson::StringRef(conditionPresetDescription.data(), conditionPresetDescription.length()), allocator);
		}
		rapidjson::Value conditionsValue = a_preset->Serialize(allocator);
		conditionPresetValue.AddMember("conditions", conditionsValue, allocator);

		presetArrayValue.PushBack(conditionPresetValue, allocator);

		return RE::BSVisit::BSVisitControl::kContinue;
	});

	if (!presetArrayValue.Empty()) {
		a_doc.AddMember("conditionPresets", presetArrayValue, allocator);
	}
}

void ReplacerMod::AddSubMod(std::unique_ptr<SubMod>& a_subMod)
{
	WriteLocker locker(_dataLock);

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
		return it->get();
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

void ReplacerMod::AddConditionPreset(std::unique_ptr<Conditions::ConditionPreset>& a_conditionPreset)
{
	WriteLocker locker(_presetsLock);

	const auto it = std::ranges::lower_bound(_conditionPresets, a_conditionPreset, [](const auto& a_lhs, const auto& a_rhs) {
		return a_lhs->GetName() < a_rhs->GetName();
	});

	_conditionPresets.insert(it, std::move(a_conditionPreset));
}

void ReplacerMod::RemoveConditionPreset(std::string_view a_name)
{
	WriteLocker locker(_presetsLock);

	const auto search = std::ranges::find_if(_conditionPresets, [&](const auto& a_conditionPreset) {
		return a_conditionPreset->GetName() == a_name;
	});

	if (search == _conditionPresets.end()) {
		return;
	}

	// remove it from all conditions. rather do this once than have the overhead of a weak pointer during gameplay
	ForEachSubMod([&](SubMod* a_subMod) {
		if (const auto conditionSet = a_subMod->GetConditionSet()) {
			conditionSet->ForEachCondition([&](std::unique_ptr<Conditions::ICondition>& a_condition) {
				if (a_condition->GetConditionType() == Conditions::ConditionType::kPreset) {
					const auto presetCondition = static_cast<Conditions::PRESETCondition*>(a_condition.get());
					if (presetCondition->conditionsComponent->conditionPreset == search->get()) {
						presetCondition->conditionsComponent->conditionPreset = nullptr;
					}
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
		}
		return RE::BSVisit::BSVisitControl::kContinue;
	});

	_conditionPresets.erase(search);
}

void ReplacerMod::LoadConditionPresets(std::vector<std::unique_ptr<Conditions::ConditionPreset>>& a_conditionPresets)
{
	// remove references from all conditions
	ForEachSubMod([&](SubMod* a_subMod) {
		if (const auto conditionSet = a_subMod->GetConditionSet()) {
			conditionSet->ForEachCondition([&](std::unique_ptr<Conditions::ICondition>& a_condition) {
				if (a_condition->GetConditionType() == Conditions::ConditionType::kPreset) {
					const auto presetCondition = static_cast<Conditions::PRESETCondition*>(a_condition.get());
					presetCondition->conditionsComponent->conditionPreset = nullptr;
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
		}
		return RE::BSVisit::BSVisitControl::kContinue;
	});

	{
		WriteLocker locker(_presetsLock);

		_conditionPresets.clear();
	}

	for (auto& conditionPreset : a_conditionPresets) {
		AddConditionPreset(conditionPreset);
	}
}

void ReplacerMod::RestorePresetReferences()
{
	// try to restore references in conditions
	ForEachSubMod([&](SubMod* a_subMod) {
		a_subMod->RestorePresetReferences();
		return RE::BSVisit::BSVisitControl::kContinue;
	});
}

bool ReplacerMod::HasConditionPresets() const
{
	ReadLocker locker(_presetsLock);

	return _conditionPresets.size() > 0;
}

bool ReplacerMod::HasConditionPreset(std::string_view a_name) const
{
	ReadLocker locker(_presetsLock);

	return GetConditionPreset(a_name) != nullptr;
}

Conditions::ConditionPreset* ReplacerMod::GetConditionPreset(std::string_view a_name) const
{
	ReadLocker locker(_presetsLock);

	const auto it = std::ranges::find_if(_conditionPresets, [&](const auto& a_conditionPreset) {
		return a_conditionPreset->GetName() == a_name;
	});

	if (it != _conditionPresets.end()) {
		return it->get();
	}

	return nullptr;
}

RE::BSVisit::BSVisitControl ReplacerMod::ForEachConditionPreset(const std::function<RE::BSVisit::BSVisitControl(Conditions::ConditionPreset*)>& a_func) const
{
	using Result = RE::BSVisit::BSVisitControl;

	ReadLocker locker(_presetsLock);

	for (auto& preset : _conditionPresets) {
		const auto result = a_func(preset.get());
		if (result == Result::kStop) {
			return result;
		}
	}

	return Result::kContinue;
}

void ReplacerMod::SortConditionPresets()
{
	WriteLocker locker(_presetsLock);

	std::ranges::sort(_conditionPresets, [](const auto& a_lhs, const auto& a_rhs) {
		return a_lhs->GetName() < a_rhs->GetName();
	});
}

bool ReplacerMod::HasInvalidConditions() const
{
	using Result = RE::BSVisit::BSVisitControl;

	const auto result = ForEachConditionPreset([&](const Conditions::ConditionPreset* a_preset) {
		if (a_preset->IsEmpty() || a_preset->HasInvalidConditions()) {
			return Result::kStop;
		}
		return Result::kContinue;
	});

	return result == Result::kStop;
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
		a_replacementAnimation->ForEachVariant([&](const Variant& a_variant) {
			addReplacementIndex(a_variant.GetIndex());
			return RE::BSVisit::BSVisitControl::kContinue;
		});
	} else {
		Variant* dummy = nullptr;
		const uint16_t replacementIndex = a_replacementAnimation->GetIndex(dummy);
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
