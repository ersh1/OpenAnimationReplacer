#include "ReplacerMods.h"

#include <ranges>

#include "DetectedProblems.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"

void SubMod::AddReplacementAnimations(RE::hkbCharacterStringData* a_stringData, RE::BShkbHkxDB::ProjectDBData* a_projectDBData, const std::vector<Parsing::ReplacementAnimationToAdd>& a_animationsToAdd)
{
    const auto replacerProjectData = OpenAnimationReplacer::GetSingleton().GetOrAddReplacerProjectData(a_stringData, a_projectDBData);

    for (auto& replacementAnimToAdd : a_animationsToAdd) {
        if (uint16_t newIndex = replacerProjectData->TryAddAnimationToAnimationBundleNames(replacementAnimToAdd); newIndex != static_cast<uint16_t>(-1)) {
            auto newReplacementAnimation = std::make_unique<ReplacementAnimation>(newIndex, replacementAnimToAdd.originalBindingIndex, _priority, replacementAnimToAdd.animationPath, a_stringData->name.data(), _conditionSet.get());
			newReplacementAnimation->_parentSubMod = this;
            {
                WriteLocker locker(_dataLock);
				_replacementAnimations.emplace_back(newReplacementAnimation.get());

                // sort replacement animations by path
				std::ranges::sort(_replacementAnimations, [](const auto& a_lhs, const auto& a_rhs) {
					return a_lhs->_path < a_rhs->_path;
				});
            }

            replacerProjectData->AddReplacementAnimation(a_stringData, replacementAnimToAdd.originalBindingIndex, newReplacementAnimation);
        }
    }

    UpdateAnimations();
}

void SubMod::LoadParseResult(const Parsing::SubModParseResult& a_parseResult)
{
    _path = a_parseResult.path;
    _configSource = a_parseResult.configSource;
    _name = a_parseResult.name;
    _description = a_parseResult.description;
    _priority = a_parseResult.priority;
    _bDisabled = a_parseResult.bDisabled;
    _disabledAnimations = a_parseResult.disabledAnimations;
    _overrideAnimationsFolder = a_parseResult.overrideAnimationsFolder;
    _requiredProjectName = a_parseResult.requiredProjectName;
    _bIgnoreNoTriggersFlag = a_parseResult.bIgnoreNoTriggersFlag;
    _bInterruptible = a_parseResult.bInterruptible;
    _bKeepRandomResultsOnLoop = a_parseResult.bKeepRandomResultsOnLoop;
    _conditionSet->MoveConditions(a_parseResult.conditionSet.get());

    SetDirtyRecursive(false);
}

void SubMod::UpdateAnimations() const
{
    // Update stuff in each anim
    for (const auto& anim : _replacementAnimations) {
        anim->SetPriority(_priority);
        anim->SetDisabled(_bDisabled);
        if (_disabledAnimations.contains(anim)) {
            anim->SetDisabled(true);
        }
        anim->SetIgnoreNoTriggersFlag(_bIgnoreNoTriggersFlag);
        anim->SetInterruptible(_bInterruptible);
        anim->SetKeepRandomResultsOnLoop(_bKeepRandomResultsOnLoop);
    }

    // Update stuff in each anim replacements struct
    if (_parentMod) {
        _parentMod->SortSubMods();
    }

    ForEachReplacerProject([](const auto a_replacerProject) {
        a_replacerProject->ForEach([](auto a_animReplacements) {
            a_animReplacements->TestInterruptible();
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

bool SubMod::ReloadConfig()
{
    Parsing::SubModParseResult parseResult;

    std::filesystem::path directoryPath(_path);

    auto configPath = directoryPath / "user.json"sv;
    if (is_regular_file(configPath)) {
        parseResult.configSource = Parsing::ConfigSource::kUser;
    } else {
        configPath = directoryPath / "config.json"sv;
        parseResult.configSource = Parsing::ConfigSource::kAuthor;
    }

    if (is_regular_file(configPath)) {
        if (DeserializeSubMod(configPath, parseResult)) {
            LoadParseResult(parseResult);
            DetectedProblems::GetSingleton().CheckForSubModsSharingPriority();
            UpdateAnimations();
            return true;
        }
    } else {
        // check if this was originally a legacy mod
        if (auto directoryPathStr = directoryPath.string(); directoryPathStr.find("DynamicAnimationReplacer"sv) != std::string::npos) {
            ResetToLegacy();
            if (directoryPathStr.find("_CustomConditions"sv) != std::string::npos) {
                if (auto txtPath = directoryPath / "_conditions.txt"sv; exists(txtPath)) {
                    auto newConditionSet = Parsing::ParseConditionsTxt(txtPath);
                    _conditionSet->MoveConditions(newConditionSet.get());

                    SetDirtyRecursive(false);

                    DetectedProblems::GetSingleton().CheckForSubModsSharingPriority();

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

                DetectedProblems::GetSingleton().CheckForSubModsSharingPriority();

                return true;
            }
        }
    }

    return false;
}

void SubMod::SaveConfig(ConditionEditMode a_editMode)
{
    if (a_editMode == ConditionEditMode::kNone) {
        return;
    }

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

    Serialize(doc);

    Parsing::SerializeJson(jsonPath, doc);

    SetDirtyRecursive(false);
}

void SubMod::Serialize(rapidjson::Document& a_doc) const
{
    rapidjson::Value::AllocatorType& allocator = a_doc.GetAllocator();

    // write submod name
    rapidjson::Value nameValue(rapidjson::StringRef(_name.data(), _name.length()));
    a_doc.AddMember("name", nameValue, allocator);

    // write submod description
    if (!_description.empty()) {
        rapidjson::Value descriptionValue(rapidjson::StringRef(_description.data(), _description.length()));
        a_doc.AddMember("description", descriptionValue, allocator);
    }

    // write submod priority
    rapidjson::Value priorityValue(_priority);
    a_doc.AddMember("priority", priorityValue, allocator);

    // write submod disabled
    if (_bDisabled) {
        rapidjson::Value disabledValue(_bDisabled);
        a_doc.AddMember("disabled", disabledValue, allocator);
    }

    // write disabled animations
    if (!_disabledAnimations.empty()) {
        rapidjson::Value disabledAnimationsValue(rapidjson::kArrayType);
        for (const auto& disabledAnim : _disabledAnimations) {
            rapidjson::Value animValue(rapidjson::kObjectType);

            animValue.AddMember("projectName", rapidjson::StringRef(disabledAnim.projectName.data(), disabledAnim.projectName.length()), allocator);
            animValue.AddMember("path", rapidjson::StringRef(disabledAnim.path.data(), disabledAnim.path.length()), allocator);

            disabledAnimationsValue.PushBack(animValue, allocator);
        }
        a_doc.AddMember("disabledAnimations", disabledAnimationsValue, allocator);
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

    // write ignore no triggers flag
    if (_bIgnoreNoTriggersFlag) {
        rapidjson::Value ignoreNoTriggersValue(_bIgnoreNoTriggersFlag);
        a_doc.AddMember("ignoreNoTriggersFlag", ignoreNoTriggersValue, allocator);
    }

    // write interruptible
    if (_bInterruptible) {
        rapidjson::Value interruptibleValue(_bInterruptible);
        a_doc.AddMember("interruptible", interruptibleValue, allocator);
    }

    // write keep random result on loop
    if (_bKeepRandomResultsOnLoop) {
        rapidjson::Value keepRandomResultsOnLoopValue(_bKeepRandomResultsOnLoop);
        a_doc.AddMember("keepRandomResultsOnLoop", keepRandomResultsOnLoopValue, allocator);
    }

    // write conditions
    rapidjson::Value conditionArrayValue = _conditionSet->Serialize(allocator);

    a_doc.AddMember("conditions", conditionArrayValue, allocator);
}

std::string SubMod::SerializeToString() const
{
    rapidjson::Document doc(rapidjson::kObjectType);

    Serialize(doc);

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
    _disabledAnimations.clear();
    _overrideAnimationsFolder = "";
    _requiredProjectName = "";
    _bIgnoreNoTriggersFlag = false;
    _bInterruptible = false;
    _bKeepRandomResultsOnLoop = false;

    SetDirty(false);

    UpdateAnimations();
}

void SubMod::AddReplacerProject(ReplacerProjectData* a_replacerProject)
{
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

bool SubMod::HasDisabledAnimation(ReplacementAnimation* a_replacementAnimation) const
{
    return _disabledAnimations.contains(a_replacementAnimation);
}

void SubMod::AddDisabledAnimation(ReplacementAnimation* a_replacementAnimation)
{
    _disabledAnimations.emplace(a_replacementAnimation);
}

void SubMod::RemoveDisabledAnimation(ReplacementAnimation* a_replacementAnimation)
{
    _disabledAnimations.erase(a_replacementAnimation);
}

void ReplacerMod::SetName(std::string_view a_name)
{
	auto previousName = _name;
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

uint16_t ReplacerProjectData::TryAddAnimationToAnimationBundleNames(const Parsing::ReplacementAnimationToAdd& a_anim)
{
    std::optional<std::string> hash = std::nullopt;

    // Check hash
    if (Settings::bFilterOutDuplicateAnimations && a_anim.hash) {
        hash = a_anim.hash;

        if (const auto search = _fileHashToIndexMap.find(*hash); search != _fileHashToIndexMap.end()) {
            ++_filteredDuplicates;
            return search->second;
        }
    }

    // Check if the animation is already in the list and return the index if it is
	for (uint16_t i = 0; i < stringData->animationNames.size(); i++) {
		if (stringData->animationNames[i].data() == a_anim.animationPath) {
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
	stringData->animationNames.push_back(a_anim.animationPath.data());

    if (Settings::bFilterOutDuplicateAnimations && hash) {
        _fileHashToIndexMap[*hash] = newIndex;
    }

    return newIndex;
}

void ReplacerProjectData::AddReplacementAnimation(RE::hkbCharacterStringData* a_stringData, uint16_t a_originalIndex, std::unique_ptr<ReplacementAnimation>& a_replacementAnimation)
{
    uint16_t replacementIndex = a_replacementAnimation->GetIndex();
    if (const auto it = originalIndexToAnimationReplacementsMap.find(a_originalIndex); it != originalIndexToAnimationReplacementsMap.end()) {
        it->second->AddReplacementAnimation(a_replacementAnimation);
    } else {
        auto newReplacementAnimations = std::make_unique<AnimationReplacements>(Utils::GetOriginalAnimationName(a_stringData, a_originalIndex));
		newReplacementAnimations->AddReplacementAnimation(a_replacementAnimation);
		originalIndexToAnimationReplacementsMap.emplace(a_originalIndex, std::move(newReplacementAnimations));
    }
	
    replacementIndexToOriginalIndexMap.emplace(replacementIndex, a_originalIndex);
    animationsToQueue.emplace_back(replacementIndex);
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
