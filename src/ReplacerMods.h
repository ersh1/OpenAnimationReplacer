#pragma once

#include "ActiveClip.h"
#include "Havok/Havok.h"
#include "Parsing.h"
#include "ReplacementAnimation.h"

namespace Jobs
{
	struct RemoveSharedRandomFloatJob;
}

enum class EditMode : int
{
	kNone = 0,
	kUser = 1,
	kAuthor = 2
};

class SubMod : public IStateDataContainerHolder
{
public:
	SubMod(ReplacerMod* a_parentMod) :
		_parentMod(a_parentMod)
	{
		_conditionSet = std::make_unique<Conditions::ConditionSet>(this);
	}

	bool StateDataUpdate(float a_deltaTime) override;
	bool StateDataOnLoopOrEcho(RE::ObjectRefHandle a_refHandle, ActiveClip* a_activeClip, bool a_bIsEcho) override;
	bool StateDataClearRefrData(RE::ObjectRefHandle a_refHandle) override;
	void StateDataClearData() override;

	bool AddReplacementAnimation(std::string_view a_animPath, uint16_t a_originalIndex, class ReplacerProjectData* a_replacerProjectData, RE::hkbCharacterStringData* a_stringData);

	void SetAnimationFiles(const std::vector<ReplacementAnimationFile>& a_animationFiles);
	void LoadParseResult(const Parsing::SubModParseResult& a_parseResult);
	void LoadReplacementAnimationDatas(const std::vector<ReplacementAnimData>& a_replacementAnimDatas);
	void HandleDeprecatedSettings() const;

	void ResetAnimations();
	void UpdateAnimations() const;
	void RestorePresetReferences();

	Conditions::ConditionSet* GetConditionSet() const { return _conditionSet.get(); }
	Conditions::ConditionSet* GetSynchronizedConditionSet() const { return _synchronizedConditionSet.get(); }

	Functions::FunctionSet* GetFunctionSet(Functions::FunctionSetType a_setType) const;
	bool HasFunctionSet(Functions::FunctionSetType a_setType) const;
	bool HasValidFunctionSet(Functions::FunctionSetType a_setType) const;
	bool HasAnyFunctionSet() const;
	Functions::FunctionSet* CreateOrGetFunctionSet(Functions::FunctionSetType a_setType);

	std::string_view GetName() const { return _name; }
	void SetName(std::string_view a_name) { _name = a_name; }

	std::string_view GetDescription() const { return _description; }
	void SetDescription(std::string_view a_description) { _description = a_description; }

	int32_t GetPriority() const { return _priority; }
	void SetPriority(int32_t a_priority) { _priority = a_priority; }

	std::string_view GetPath() const { return _path; }
	Parsing::ConfigSource GetConfigSource() const { return _configSource; }
	//void SetConfigSource(Parsing::ConfigSource a_configSource) { _configSource = a_configSource; }

	bool IsDisabled() const { return _bDisabled; }
	void SetDisabled(bool a_bDisabled) { _bDisabled = a_bDisabled; }

	std::string_view GetOverrideAnimationsFolder() const { return _overrideAnimationsFolder; }
	void SetOverrideAnimationsFolder(std::string_view a_overrideAnimationsFolder) { _overrideAnimationsFolder = a_overrideAnimationsFolder; }

	std::string_view GetRequiredProjectName() const { return _requiredProjectName; }
	void SetRequiredProjectName(std::string_view a_requiredProjectName) { _requiredProjectName = a_requiredProjectName; }

	bool IsIgnoringDontConvertAnnotationsToTriggersFlag() const { return _bIgnoreDontConvertAnnotationsToTriggersFlag; }
	void SetIgnoringDontConvertAnnotationsToTriggersFlag(bool a_bIgnoreDontConvertAnnotationsToTriggersFlag) { _bIgnoreDontConvertAnnotationsToTriggersFlag = a_bIgnoreDontConvertAnnotationsToTriggersFlag; }

	bool IsOnlyUsingTriggersFromAnnotations() const { return _bTriggersFromAnnotationsOnly; }
	void SetOnlyUsingTriggersFromAnnotations(bool a_bTriggersFromAnnotationsOnly) { _bTriggersFromAnnotationsOnly = a_bTriggersFromAnnotationsOnly; }

	bool IsInterruptible() const { return _bInterruptible; }
	void SetInterruptible(bool a_bInterruptible) { _bInterruptible = a_bInterruptible; }

	bool HasCustomBlendTime(CustomBlendType a_type) const;
	float GetCustomBlendTime(CustomBlendType a_type) const;
	void ToggleCustomBlendTime(CustomBlendType a_type, bool a_bEnable);
	void SetCustomBlendTime(CustomBlendType a_type, float a_value);

	bool IsReevaluatingOnLoop() const { return _bReplaceOnLoop; }
	void SetReevaluatingOnLoop(bool a_bReplaceOnLoop) { _bReplaceOnLoop = a_bReplaceOnLoop; }

	bool IsReevaluatingOnEcho() const { return _bReplaceOnEcho; }
	void SetReevaluatingOnEcho(bool a_bReplaceOnEcho) { _bReplaceOnEcho = a_bReplaceOnEcho; }

	bool IsRunningFunctionsOnLoop() const { return _bRunFunctionsOnLoop; }
	void SetRunningFunctionsOnLoop(bool a_bRunFunctionsOnLoop) { _bRunFunctionsOnLoop = a_bRunFunctionsOnLoop; }

	bool IsRunningFunctionsOnEcho() const { return _bRunFunctionsOnEcho; }
	void SetRunningFunctionsOnEcho(bool a_bRunFunctionsOnEcho) { _bRunFunctionsOnEcho = a_bRunFunctionsOnEcho; }

	bool IsDirty() const;
	bool IsFromUserConfig() const { return _configSource == Parsing::ConfigSource::kUser; }
	bool IsFromLegacyConfig() const { return _configSource >= Parsing::ConfigSource::kLegacy; }
	bool IsFromLegacyActorBase() const { return _configSource == Parsing::ConfigSource::kLegacyActorBase; }
	void SetDirty(bool a_bDirty) { _bDirty = a_bDirty; }

	void SetDirtyRecursive(bool a_bDirty)
	{
		_bDirty = a_bDirty;
		_conditionSet->SetDirtyRecursive(a_bDirty);
		if (_synchronizedConditionSet) {
			_synchronizedConditionSet->SetDirtyRecursive(a_bDirty);
		}
		if (_functionSetOnActivate) {
			_functionSetOnActivate->SetDirtyRecursive(a_bDirty);
		}
		if (_functionSetOnDeactivate) {
			_functionSetOnDeactivate->SetDirtyRecursive(a_bDirty);
		}
		if (_functionSetOnTrigger) {
			_functionSetOnTrigger->SetDirtyRecursive(a_bDirty);
		}
	}

	bool HasInvalidConditions() const;
	bool HasInvalidFunctions() const;

	bool HasSynchronizedAnimations() const { return _synchronizedConditionSet != nullptr; }
	void SetHasSynchronizedAnimations();

	bool ReloadConfig();
	void SaveConfig(EditMode a_editMode, bool a_bResetDirty = true);
	void Serialize(rapidjson::Document& a_doc, EditMode a_editMode) const;
	std::string SerializeToString() const;

	class ReplacerMod* GetParentMod() const;

	void ResetToLegacy();
	void ResetReplacementAnimationsToLegacy();

	void AddReplacerProject(ReplacerProjectData* a_replacerProject);
	void ForEachReplacerProject(const std::function<void(ReplacerProjectData*)>& a_func) const;
	void ForEachReplacementAnimation(const std::function<void(ReplacementAnimation*)>& a_func) const;
	void ForEachReplacementAnimationFile(const std::function<void(const ReplacementAnimationFile&)>& a_func) const;

	StateDataContainer<const Conditions::ICondition*> conditionStateData;

private:
	friend class ReplacerMod;
	ReplacerMod* _parentMod = nullptr;

	std::string _name;
	std::string _description;
	int32_t _priority = 0;
	std::string _path;
	Parsing::ConfigSource _configSource = Parsing::ConfigSource::kAuthor;
	bool _bDisabled = false;
	std::vector<ReplacementAnimData> _replacementAnimDatas{};
	std::string _overrideAnimationsFolder{};
	std::string _requiredProjectName{};
	bool _bIgnoreDontConvertAnnotationsToTriggersFlag = false;
	bool _bTriggersFromAnnotationsOnly = false;
	bool _bInterruptible = false;
	bool _bCustomBlendTimeOnInterrupt = false;
	float _blendTimeOnInterrupt = Settings::fDefaultBlendTimeOnInterrupt;
	bool _bReplaceOnLoop = true;
	bool _bCustomBlendTimeOnLoop = false;
	float _blendTimeOnLoop = Settings::fDefaultBlendTimeOnLoop;
	bool _bReplaceOnEcho = false;
	bool _bCustomBlendTimeOnEcho = false;
	float _blendTimeOnEcho = Settings::fDefaultBlendTimeOnEcho;
	bool _bRunFunctionsOnLoop = true;
	bool _bRunFunctionsOnEcho = true;
	bool _bKeepRandomResultsOnLoop_DEPRECATED = false;
	bool _bShareRandomResults_DEPRECATED = false;

	std::unordered_map<std::filesystem::path, ReplacementAnimationFile, CaseInsensitivePathHash, CaseInsensitivePathEqual> _replacementAnimationFiles;

	std::unique_ptr<Conditions::ConditionSet> _conditionSet;
	std::unique_ptr<Conditions::ConditionSet> _synchronizedConditionSet = nullptr;
	std::unique_ptr<Functions::FunctionSet> _functionSetOnActivate = nullptr;
	std::unique_ptr<Functions::FunctionSet> _functionSetOnDeactivate = nullptr;
	std::unique_ptr<Functions::FunctionSet> _functionSetOnTrigger = nullptr;
	bool _bDirty = false;

	mutable SharedLock _dataLock;
	std::vector<ReplacerProjectData*> _replacerProjects;
	std::vector<ReplacementAnimation*> _replacementAnimations;
};

// this is a replacer mod, contains submods
class ReplacerMod : public IStateDataContainerHolder
{
public:
	ReplacerMod(bool a_bIsLegacy) :
		_bIsLegacy(a_bIsLegacy)
	{}

	ReplacerMod(std::string_view a_path, std::string_view a_name, std::string_view a_author, std::string_view a_description, bool a_bIsLegacy) :
		_name(a_name),
		_author(a_author),
		_description(a_description),
		_bIsLegacy(a_bIsLegacy),
		_path(a_path)
	{}

	bool StateDataUpdate(float a_deltaTime) override;
	bool StateDataOnLoopOrEcho(RE::ObjectRefHandle a_refHandle, ActiveClip* a_activeClip, bool a_bIsEcho) override;
	bool StateDataClearRefrData(RE::ObjectRefHandle a_refHandle) override;
	void StateDataClearData() override;

	void LoadParseResult(Parsing::ModParseResult& a_parseResult);

	std::string_view GetName() const { return _name; }
	void SetName(std::string_view a_name);
	std::string_view GetAuthor() const { return _author; }
	void SetAuthor(std::string_view a_author) { _author = a_author; }
	std::string_view GetDescription() const { return _description; }
	void SetDescription(std::string_view a_description) { _description = a_description; }

	std::string_view GetPath() const { return _path; }
	bool IsLegacy() const { return _bIsLegacy; }

	Parsing::ConfigSource GetConfigSource() const { return _configSource; }

	bool IsDirty() const { return _bDirty; }
	void SetDirty(bool a_bDirty) { _bDirty = a_bDirty; }

	bool ReloadConfig();
	void SaveConfig(EditMode a_editMode);
	void Serialize(rapidjson::Document& a_doc) const;

	void AddSubMod(std::unique_ptr<SubMod>& a_subMod);
	bool HasSubMod(std::string_view a_path) const;
	SubMod* GetSubMod(std::string_view a_path) const;
	RE::BSVisit::BSVisitControl ForEachSubMod(const std::function<RE::BSVisit::BSVisitControl(SubMod*)>& a_func) const;
	void SortSubMods();

	void AddConditionPreset(std::unique_ptr<Conditions::ConditionPreset>& a_conditionPreset);
	void RemoveConditionPreset(std::string_view a_name);
	void LoadConditionPresets(std::vector<std::unique_ptr<Conditions::ConditionPreset>>& a_conditionPresets);
	void RestorePresetReferences();
	bool HasConditionPresets() const;
	bool HasConditionPreset(std::string_view a_name) const;
	Conditions::ConditionPreset* GetConditionPreset(std::string_view a_name) const;
	RE::BSVisit::BSVisitControl ForEachConditionPreset(const std::function<RE::BSVisit::BSVisitControl(Conditions::ConditionPreset*)>& a_func) const;
	void SortConditionPresets();

	bool HasInvalidConditions(bool a_bCheckPresetsOnly) const;
	bool HasInvalidFunctions() const;

	StateDataContainer<std::string> conditionStateData;
	VariantStateDataContainer variantStateData;

private:
	std::string _name;
	std::string _author;
	std::string _description;
	bool _bIsLegacy = false;
	std::string _path;
	Parsing::ConfigSource _configSource = Parsing::ConfigSource::kAuthor;

	mutable SharedLock _dataLock;
	std::vector<std::unique_ptr<SubMod>> _subMods;

	mutable SharedLock _presetsLock;
	std::vector<std::unique_ptr<Conditions::ConditionPreset>> _conditionPresets;

	bool _bDirty = false;
};

// this class contains all animation replacements for a particular animation
class AnimationReplacements
{
public:
	AnimationReplacements(std::string_view a_originalPath) :
		_originalPath(a_originalPath) {}

	bool IsEmpty() const { return _replacements.empty(); }
	std::string_view GetOriginalPath() const { return _originalPath; }
	bool IsOriginalInterruptible() const { return _bOriginalInterruptible; }
	bool ShouldOriginalReplaceOnEcho() const { return _bOriginalReplaceOnEcho; }

	[[nodiscard]] ReplacementAnimation* EvaluateConditionsAndGetReplacementAnimation(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const;
	[[nodiscard]] ReplacementAnimation* EvaluateSynchronizedConditionsAndGetReplacementAnimation(RE::TESObjectREFR* a_sourceRefr, RE::TESObjectREFR* a_targetRefr, RE::hkbClipGenerator* a_clipGenerator) const;

	void AddReplacementAnimation(std::unique_ptr<ReplacementAnimation>& a_replacementAnimation);
	void SortByPriority();

	void ForEachReplacementAnimation(const std::function<void(ReplacementAnimation*)>& a_func, bool a_bReverse = false) const;

	void TestInterruptible();
	void TestReplaceOnEcho();

	void MarkAsSynchronizedAnimation(bool a_bSynchronized);

protected:
	mutable SharedLock _lock;

	std::string _originalPath;
	std::vector<std::unique_ptr<ReplacementAnimation>> _replacements;

	bool _bSynchronized = false;

	bool _bOriginalInterruptible = false;
	bool _bOriginalReplaceOnEcho = false;
};

// this is a class holding our data per behavior project
class ReplacerProjectData
{
public:
	ReplacerProjectData(RE::hkbCharacterStringData* a_stringData, RE::BShkbHkxDB::ProjectDBData* a_projectDBData) :
		stringData(a_stringData),
		projectDBData(a_projectDBData) {}

	ReplacementAnimation* EvaluateConditionsAndGetReplacementAnimation(RE::hkbClipGenerator* a_clipGenerator, uint16_t a_originalIndex, RE::TESObjectREFR* a_refr) const;
	[[nodiscard]] uint16_t GetOriginalAnimationIndex(uint16_t a_currentIndex) const;

	uint16_t TryAddAnimationToAnimationBundleNames(std::string_view a_path, const std::optional<std::string>& a_hash);
	void AddReplacementAnimation(RE::hkbCharacterStringData* a_stringData, uint16_t a_originalIndex, std::unique_ptr<ReplacementAnimation>& a_replacementAnimation);
	void SortReplacementAnimationsByPriority(uint16_t a_originalIndex);
	void QueueReplacementAnimations(RE::hkbCharacter* a_character);
	void MarkSynchronizedReplacementAnimations(RE::hkbGenerator* a_rootGenerator);

	[[nodiscard]] uint32_t GetFilteredDuplicateCount() const { return _filteredDuplicates; }

	[[nodiscard]] AnimationReplacements* GetAnimationReplacements(uint16_t a_originalIndex) const;

	void ForEach(const std::function<void(AnimationReplacements*)>& a_func);

	std::unordered_map<uint16_t, std::unique_ptr<AnimationReplacements>> originalIndexToAnimationReplacementsMap;
	std::unordered_map<uint16_t, uint16_t> replacementIndexToOriginalIndexMap;
	std::vector<uint16_t> animationsToQueue;

	RE::hkRefPtr<RE::hkbCharacterStringData> stringData;
	RE::hkRefPtr<RE::BShkbHkxDB::ProjectDBData> projectDBData;

	uint16_t synchronizedClipIDOffset = 0;

protected:
	std::unordered_map<std::string, uint16_t> _fileHashToIndexMap;
	uint32_t _filteredDuplicates = 0;
};
