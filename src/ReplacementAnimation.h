#pragma once

#include "Conditions.h"

class ReplacementAnimation
{
public:
    class Variant
    {
    public:
		Variant(uint16_t a_index, std::string_view a_filename) :
			_index(a_index),
			_filename(a_filename)
		{}
		
		uint16_t GetIndex() const { return _index; }
		float GetWeight() const { return _weight; }
		bool IsDisabled() const { return _bDisabled; }
		std::string_view GetFilename() const { return _filename; }
		void SetWeight(float a_weight) { _weight = a_weight; }
		void SetDisabled(bool a_bDisable) { _bDisabled = a_bDisable; }

		bool ShouldSaveToJson() const;

		void ResetSettings();

    protected:
		uint16_t _index = static_cast<uint16_t>(-1);
		float _weight = 1.f;
		bool _bDisabled = false;
		std::string _filename;
    };

	class Variants
	{
	public:
		Variants(std::vector<Variant>& a_variants) :
			_variants(std::move(a_variants))
		{
			UpdateVariantCache();
		}

		uint16_t GetVariantIndex(class ActiveClip* a_activeClip) const;
		void UpdateVariantCache();

		RE::BSVisit::BSVisitControl ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(Variant&)>& a_func);
		RE::BSVisit::BSVisitControl ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(const Variant&)>& a_func) const;

	protected:
		friend class ReplacementAnimation;
		ReplacementAnimation* _parentReplacementAnimation = nullptr;
		std::vector<Variant> _variants;

		mutable SharedLock _lock;
		std::vector<Variant*> _activeVariants;
		float _totalWeight = 0.f;
		std::vector<float> _cumulativeWeights;
	};

	ReplacementAnimation(uint16_t a_index, uint16_t a_originalIndex, int32_t a_priority, std::string_view a_path, std::string_view a_projectName, Conditions::ConditionSet* a_conditionSet);
	ReplacementAnimation(std::vector<Variant>& a_variants, uint16_t a_originalIndex, int32_t a_priority, std::string_view a_path, std::string_view a_projectName, Conditions::ConditionSet* a_conditionSet);

	bool HasVariants() const { return std::holds_alternative<Variants>(_index); }

	bool ShouldSaveToJson() const;

	bool IsDisabled() const { return _bDisabled || _bDisabledByParent; }
	
    uint16_t GetIndex() const;
	uint16_t GetIndex(class ActiveClip* a_activeClip) const;
    uint16_t GetOriginalIndex() const { return _originalIndex; }
    int32_t GetPriority() const { return _priority; }
    bool GetDisabled() const { return _bDisabled; }
    bool GetIgnoreNoTriggersFlag() const { return _bIgnoreNoTriggersFlag; }
    bool GetInterruptible() const { return _bInterruptible; }
	bool GetReplaceOnLoop() const { return _bReplaceOnLoop; }
	bool GetReplaceOnEcho() const { return _bReplaceOnEcho; }
    bool GetKeepRandomResultsOnLoop() const { return _bKeepRandomResultsOnLoop; }
    bool GetShareRandomResults() const { return _bShareRandomResults; }
    void SetPriority(int32_t a_priority) { _priority = a_priority; }
    void SetDisabled(bool a_bDisable) { _bDisabled = a_bDisable; }
    void SetDisabledByParent(bool a_bDisable) { _bDisabledByParent = a_bDisable; }
    void SetIgnoreNoTriggersFlag(bool a_enable) { _bIgnoreNoTriggersFlag = a_enable; }
    void SetInterruptible(bool a_bEnable) { _bInterruptible = a_bEnable; }
    void SetReplaceOnLoop(bool a_bEnable) { _bReplaceOnLoop = a_bEnable; }
    void SetReplaceOnEcho(bool a_bEnable) { _bReplaceOnEcho = a_bEnable; }
    void SetKeepRandomResultsOnLoop(bool a_bEnable) { _bKeepRandomResultsOnLoop = a_bEnable; }
	void SetShareRandomResults(bool a_bEnable) { _bShareRandomResults = a_bEnable; }
    std::string_view GetAnimPath() const { return _path; }
    std::string_view GetProjectName() const { return _projectName; }
	Conditions::ConditionSet* GetConditionSet() const { return _conditionSet; }
    SubMod* GetParentSubMod() const;

	void LoadAnimData(const struct ReplacementAnimData& a_replacementAnimData);
	void ResetVariants();
	void UpdateVariantCache();
	std::string_view GetVariantFilename(uint16_t a_variantIndex) const;
	RE::BSVisit::BSVisitControl ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(Variant&)>& a_func);
	RE::BSVisit::BSVisitControl ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(const Variant&)>& a_func) const;

    bool EvaluateConditions(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const;

protected:
	std::variant<uint16_t, Variants> _index;
    uint16_t _originalIndex;
    int32_t _priority;
    bool _bDisabled = false;
    bool _bIgnoreNoTriggersFlag = false;
    bool _bInterruptible = false;
	bool _bReplaceOnLoop = true;
	bool _bReplaceOnEcho = false;
    bool _bKeepRandomResultsOnLoop = false;
	bool _bShareRandomResults = false;
    std::string _path;
    std::string _projectName;
    Conditions::ConditionSet* _conditionSet;

    friend class SubMod;
    SubMod* _parentSubMod = nullptr;

	bool _bDisabledByParent = false;
};
