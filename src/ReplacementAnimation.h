#pragma once

#include "Conditions.h"
#include "Settings.h"
#include "Variants.h"

struct ReplacementAnimationFile
{
	struct Variant
	{
		Variant(std::string_view a_fullPath) :
			fullPath(a_fullPath) {}

		std::string fullPath;
		std::optional<std::string> hash = std::nullopt;
	};

	ReplacementAnimationFile(std::string_view a_fullPath);
	ReplacementAnimationFile(std::string_view a_fullPath, std::vector<Variant>& a_variants);

	std::string GetOriginalPath() const;

	std::string fullPath;
	std::optional<std::string> hash = std::nullopt;
	std::optional<std::vector<Variant>> variants = std::nullopt;
};

enum class CustomBlendType : int
{
	kInterrupt = 0,
	kLoop = 1,
	kEcho = 2
};

class ReplacementAnimation
{
public:
	ReplacementAnimation(uint16_t a_index, uint16_t a_originalIndex, std::string_view a_path, std::string_view a_projectName, Conditions::ConditionSet* a_conditionSet);
	ReplacementAnimation(std::vector<Variant>& a_variants, uint16_t a_originalIndex, std::string_view a_path, std::string_view a_projectName, Conditions::ConditionSet* a_conditionSet);

	bool HasVariants() const { return std::holds_alternative<Variants>(_index); }
	Variants& GetVariants() { return std::get<Variants>(_index); }
	const Variants& GetVariants() const { return std::get<Variants>(_index); }

	bool ShouldSaveToJson() const;

	bool IsDisabled() const;

	uint16_t GetIndex(Variant*& a_outVariant) const;
	uint16_t GetIndex(Variant*& a_outVariant, float a_randomWeight) const;
	uint16_t GetIndex(ActiveClip* a_activeClip, Variant*& a_outVariant) const;
	uint16_t GetOriginalIndex() const { return _originalIndex; }
	int32_t GetPriority() const;
	bool GetDisabled() const { return _bDisabled; }
	bool GetIgnoreDontConvertAnnotationsToTriggersFlag() const;
	bool GetTriggersFromAnnotationsOnly() const;
	bool GetInterruptible() const;
	bool GetReplaceOnLoop() const;
	bool GetReplaceOnEcho() const;
	float GetCustomBlendTime(ActiveClip* a_activeClip, CustomBlendType a_type, bool a_bCheckWithinSequence = false, bool a_bIsBetweenVariants = false) const;
	void SetDisabled(bool a_bDisable) { _bDisabled = a_bDisable; }
	std::string_view GetAnimPath() const { return _path; }
	std::string_view GetProjectName() const { return _projectName; }
	Conditions::ConditionSet* GetConditionSet() const { return _conditionSet; }
	SubMod* GetParentSubMod() const;

	bool IsSynchronizedAnimation() const { return _bSynchronized; }
	void MarkAsSynchronizedAnimation(bool a_bSynchronized);
	void SetSynchronizedConditionSet(Conditions::ConditionSet* a_synchronizedConditionSet);

	void LoadAnimData(const struct ReplacementAnimData& a_replacementAnimData);
	void ResetVariants();
	void UpdateVariantCache();
	std::string_view GetVariantFilename(uint16_t a_variantIndex) const;
	RE::BSVisit::BSVisitControl ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(Variant&)>& a_func);
	RE::BSVisit::BSVisitControl ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(const Variant&)>& a_func) const;

	bool EvaluateConditions(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const;
	bool EvaluateSynchronizedConditions(RE::TESObjectREFR* a_sourceRefr, RE::TESObjectREFR* a_targetRefr, RE::hkbClipGenerator* a_clipGenerator) const;

protected:
	std::variant<uint16_t, Variants> _index;
	uint16_t _originalIndex;
	bool _bDisabled = false;
	std::string _path;
	std::string _projectName;
	Conditions::ConditionSet* _conditionSet;
	Conditions::ConditionSet* _synchronizedConditionSet = nullptr;

	bool _bSynchronized = false;

	friend class SubMod;
	SubMod* _parentSubMod = nullptr;
};
