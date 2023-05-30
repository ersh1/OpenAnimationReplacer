#pragma once

#include "Conditions.h"

class ReplacementAnimation
{
public:
    ReplacementAnimation(uint16_t a_index, uint16_t a_originalIndex, int32_t a_priority, std::string_view a_path, std::string_view a_projectName, Conditions::ConditionSet* a_conditionSet) :
        _index(a_index),
        _originalIndex(a_originalIndex),
        _priority(a_priority),
        _path(a_path),
        _projectName(a_projectName),
        _conditionSet(a_conditionSet) {}

    uint16_t GetIndex() const { return _index; }
    uint16_t GetOriginalIndex() const { return _originalIndex; }
    int32_t GetPriority() const { return _priority; }
    bool GetDisabled() const { return _bDisabled; }
    bool GetIgnoreNoTriggersFlag() const { return _bIgnoreNoTriggersFlag; }
    bool GetInterruptible() const { return _bInterruptible; }
    bool GetKeepRandomResultsOnLoop() const { return _bKeepRandomResultsOnLoop; }
    void SetPriority(int32_t a_priority) { _priority = a_priority; }
    void SetDisabled(bool a_bDisable) { _bDisabled = a_bDisable; }
    void SetIgnoreNoTriggersFlag(bool a_enable) { _bIgnoreNoTriggersFlag = a_enable; }
    void SetInterruptible(bool a_bEnable) { _bInterruptible = a_bEnable; }
    void SetKeepRandomResultsOnLoop(bool a_bEnable) { _bKeepRandomResultsOnLoop = a_bEnable; }
    std::string_view GetAnimPath() const { return _path; }
    std::string_view GetProjectName() const { return _projectName; }
	Conditions::ConditionSet* GetConditionSet() const { return _conditionSet; }
    class SubMod* GetParentSubMod() const;

    bool EvaluateConditions(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const;

protected:
    uint16_t _index;
    uint16_t _originalIndex;
    int32_t _priority;
    bool _bDisabled = false;
    bool _bIgnoreNoTriggersFlag = false;
    bool _bInterruptible = false;
    bool _bKeepRandomResultsOnLoop = false;
    std::string _path;
    std::string _projectName;

    Conditions::ConditionSet* _conditionSet;

    friend class SubMod;
    SubMod* _parentSubMod = nullptr;
};
