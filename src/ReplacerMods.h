#pragma once

#include "Havok/Havok.h"
#include "Parsing.h"
#include "ReplacementAnimation.h"

enum class ConditionEditMode : int
{
    kNone = 0,
    kUser = 1,
    kAuthor = 2
};

class SubMod
{
public:
    SubMod()
    {
        _conditionSet = std::make_unique<Conditions::ConditionSet>();
    }

    void AddReplacementAnimations(RE::hkbCharacterStringData* a_stringData, RE::BShkbHkxDB::ProjectDBData* a_projectDBData, const std::vector<Parsing::ReplacementAnimationToAdd>& a_animationsToAdd);

    void LoadParseResult(const Parsing::SubModParseResult& a_parseResult);

    void UpdateAnimations() const;

    Conditions::ConditionSet* GetConditionSet() const { return _conditionSet.get(); }

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

    std::set<DisabledAnimation>& GetDisabledAnimations() { return _disabledAnimations; }

    std::string_view GetOverrideAnimationsFolder() const { return _overrideAnimationsFolder; }
    void SetOverrideAnimationsFolder(std::string_view a_overrideAnimationsFolder) { _overrideAnimationsFolder = a_overrideAnimationsFolder; }

    std::string_view GetRequiredProjectName() const { return _requiredProjectName; }
    void SetRequiredProjectName(std::string_view a_requiredProjectName) { _requiredProjectName = a_requiredProjectName; }

    bool IsIgnoringNoTriggersFlag() const { return _bIgnoreNoTriggersFlag; }
    void SetIgnoringNoTriggersFlag(bool a_bIgnoreNoTriggersFlag) { _bIgnoreNoTriggersFlag = a_bIgnoreNoTriggersFlag; }

    bool IsInterruptible() const { return _bInterruptible; }
    void SetInterruptible(bool a_bInterruptible) { _bInterruptible = a_bInterruptible; }

	bool IsReevaluatingOnLoop() const { return _bReplaceOnLoop; }
	void SetReevaluatingOnLoop(bool a_bReplaceOnLoop) { _bReplaceOnLoop = a_bReplaceOnLoop; }

	bool IsReevaluatingOnEcho() const { return _bReplaceOnEcho; }
	void SetReevaluatingOnEcho(bool a_bReplaceOnEcho) { _bReplaceOnEcho = a_bReplaceOnEcho; }

    bool IsKeepingRandomResultsOnLoop() const { return _bKeepRandomResultsOnLoop; }
    void SetKeepRandomResultsOnLoop(bool a_bKeepRandomResultsOnLoop) { _bKeepRandomResultsOnLoop = a_bKeepRandomResultsOnLoop; }

    bool IsDirty() const { return _bDirty || _conditionSet->IsDirtyRecursive(); }
    bool IsFromUserConfig() const { return _configSource == Parsing::ConfigSource::kUser; }
    bool IsFromLegacyConfig() const { return _configSource >= Parsing::ConfigSource::kLegacy; }
    bool IsFromLegacyActorBase() const { return _configSource == Parsing::ConfigSource::kLegacyActorBase; }
    bool DoesUserConfigExist() const;
    void DeleteUserConfig() const;
    void SetDirty(bool a_bDirty) { _bDirty = a_bDirty; }

    void SetDirtyRecursive(bool a_bDirty)
    {
        _bDirty = a_bDirty;
        _conditionSet->SetDirtyRecursive(a_bDirty);
    }

    bool ReloadConfig();
    void SaveConfig(ConditionEditMode a_editMode, bool a_bResetDirty = true);
    void Serialize(rapidjson::Document& a_doc, ConditionEditMode a_editMode) const;
    std::string SerializeToString() const;

    class ReplacerMod* GetParentMod() const;

    void ResetToLegacy();

    void AddReplacerProject(class ReplacerProjectData* a_replacerProject);
    void ForEachReplacerProject(const std::function<void(ReplacerProjectData*)>& a_func) const;
    void ForEachReplacementAnimation(const std::function<void(ReplacementAnimation*)>& a_func) const;

    bool HasDisabledAnimation(ReplacementAnimation* a_replacementAnimation) const;
    void AddDisabledAnimation(ReplacementAnimation* a_replacementAnimation);
    void RemoveDisabledAnimation(ReplacementAnimation* a_replacementAnimation);

private:
    friend class ReplacerMod;
    ReplacerMod* _parentMod = nullptr;

    std::string _name;
    std::string _description;
    int32_t _priority = 0;
    std::string _path;
    Parsing::ConfigSource _configSource = Parsing::ConfigSource::kAuthor;
    bool _bDisabled = false;
    std::set<DisabledAnimation> _disabledAnimations{};
    std::string _overrideAnimationsFolder{};
    std::string _requiredProjectName{};
    bool _bIgnoreNoTriggersFlag = false;
    bool _bInterruptible = false;
	bool _bReplaceOnLoop = true;
	bool _bReplaceOnEcho = false;
    bool _bKeepRandomResultsOnLoop = false;

    std::unique_ptr<Conditions::ConditionSet> _conditionSet;
    bool _bDirty = false;

    mutable SharedLock _dataLock;
    std::vector<ReplacerProjectData*> _replacerProjects;
    std::vector<ReplacementAnimation*> _replacementAnimations;
};

// this is a replacer mod, contains submods
class ReplacerMod
{
public:
    ReplacerMod(std::string_view a_path, std::string_view a_name, std::string_view a_author, std::string_view a_description, bool a_bIsLegacy) :
        _name(a_name),
        _author(a_author),
        _description(a_description),
        _bIsLegacy(a_bIsLegacy),
        _path(a_path) {}

    std::string_view GetName() const { return _name; }
    void SetName(std::string_view a_name);
    std::string_view GetAuthor() const { return _author; }
    void SetAuthor(std::string_view a_author) { _author = a_author; }
    std::string_view GetDescription() const { return _description; }
    void SetDescription(std::string_view a_description) { _description = a_description; }

    std::string_view GetPath() const { return _path; }
    bool IsLegacy() const { return _bIsLegacy; }

    bool IsDirty() const { return _bDirty; }
    void SetDirty(bool a_bDirty) { _bDirty = a_bDirty; }

    void SaveConfig(ConditionEditMode a_editMode);
    void Serialize(rapidjson::Document& a_doc) const;

    void AddSubMod(std::unique_ptr<SubMod>& a_subMod);

    bool HasSubMod(std::string_view a_path) const;
    SubMod* GetSubMod(std::string_view a_path) const;
    RE::BSVisit::BSVisitControl ForEachSubMod(const std::function<RE::BSVisit::BSVisitControl(SubMod*)>& a_func) const;

    void SortSubMods();

private:
    std::string _name;
    std::string _author;
    std::string _description;
    bool _bIsLegacy = false;
    std::string _path;

    mutable SharedLock _dataLock;
    std::vector<std::unique_ptr<SubMod>> _subMods;

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
    bool ShouldOriginalKeepRandomResultsOnLoop() const { return _bOriginalKeepRandomResultsOnLoop; }

    ReplacementAnimation* EvaluateConditionsAndGetReplacementAnimation(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const;

    void AddReplacementAnimation(std::unique_ptr<ReplacementAnimation>& a_replacementAnimation);
    void SortByPriority();

    void ForEachReplacementAnimation(const std::function<void(ReplacementAnimation*)>& a_func, bool a_bReverse = false) const;

    void TestInterruptible();
	void TestReplaceOnEcho();
    void TestKeepRandomResultsOnLoop();

protected:
    mutable SharedLock _lock;

    std::string _originalPath;
    std::vector<std::unique_ptr<ReplacementAnimation>> _replacements;

    bool _bOriginalInterruptible = false;
    bool _bOriginalReplaceOnEcho = false;
    bool _bOriginalKeepRandomResultsOnLoop = false;
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

    uint16_t TryAddAnimationToAnimationBundleNames(const Parsing::ReplacementAnimationToAdd& a_anim);
    void AddReplacementAnimation(RE::hkbCharacterStringData* a_stringData, uint16_t a_originalIndex, std::unique_ptr<ReplacementAnimation>& a_replacementAnimation);
    void SortReplacementAnimationsByPriority(uint16_t a_originalIndex);
    void QueueReplacementAnimations(RE::hkbCharacter* a_character);

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
