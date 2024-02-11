#include "ReplacementAnimation.h"

#include "ActiveClip.h"
#include "AnimationFileHashCache.h"
#include "Parsing.h"
#include "ReplacerMods.h"
#include "Settings.h"

ReplacementAnimationFile::ReplacementAnimationFile(std::string_view a_fullPath) :
	fullPath(a_fullPath)
{
	if (Settings::bFilterOutDuplicateAnimations) {
		hash = AnimationFileHashCache::CalculateHash(fullPath);
	}
}

ReplacementAnimationFile::ReplacementAnimationFile(std::string_view a_fullPath, std::vector<Variant>& a_variants) :
	fullPath(a_fullPath),
	variants(std::move(a_variants))
{
	if (Settings::bFilterOutDuplicateAnimations) {
		for (auto& variant : *variants) {
			variant.hash = (AnimationFileHashCache::CalculateHash(variant.fullPath));
		}
	}
}

std::string ReplacementAnimationFile::GetOriginalPath() const
{
	return Parsing::ConvertVariantsPath(Parsing::StripReplacerPath(fullPath));
}

bool ReplacementAnimation::Variant::ShouldSaveToJson() const
{
    return _weight != 1.f || _bDisabled != false;
}

void ReplacementAnimation::Variant::ResetSettings()
{
    _weight = 1.f;
	_bDisabled = false;
}

uint16_t ReplacementAnimation::Variants::GetVariantIndex() const
{
	const float randomWeight = Utils::GetRandomFloat(0.f, 1.f);

	ReadLocker locker(_lock);

	const auto it = std::ranges::lower_bound(_cumulativeWeights, randomWeight);
	const auto i = std::distance(_cumulativeWeights.begin(), it);
	return _activeVariants[i]->GetIndex();
}

uint16_t ReplacementAnimation::Variants::GetVariantIndex(ActiveClip* a_activeClip) const
{
	const float randomWeight = a_activeClip->GetVariantRandom(_parentReplacementAnimation);

	ReadLocker locker(_lock);

	const auto it = std::ranges::lower_bound(_cumulativeWeights, randomWeight);
	const auto i = std::distance(_cumulativeWeights.begin(), it);
	return _activeVariants[i]->GetIndex();
}

void ReplacementAnimation::Variants::UpdateVariantCache()
{
	WriteLocker locker(_lock);

	_activeVariants.clear();
	_totalWeight = 0.f;
	_cumulativeWeights.clear();
	float maxWeight = 0.f;

	std::ranges::sort(_variants, [](const Variant& a, const Variant& b) {
		return a.GetWeight() > b.GetWeight();
	});

	for (auto& variant : _variants) {
		if (!variant.IsDisabled()) {
			_totalWeight += variant.GetWeight();
			maxWeight = std::max(maxWeight, variant.GetWeight());
			_activeVariants.emplace_back(&variant);
		}
	}

	// cache
	float weightSum = 0.f;

	for (const auto activeVariant : _activeVariants) {
		const float normalizedWeight = activeVariant->GetWeight();
		weightSum += normalizedWeight;
		_cumulativeWeights.emplace_back(weightSum / _totalWeight);
	}
}

RE::BSVisit::BSVisitControl ReplacementAnimation::Variants::ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(Variant&)>& a_func)
{
	using Result = RE::BSVisit::BSVisitControl;

	for (auto& variant : _variants) {
		const auto result = a_func(variant);
		if (result == Result::kStop) {
			return result;
		}
	}

	return Result::kContinue;
}

RE::BSVisit::BSVisitControl ReplacementAnimation::Variants::ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(const Variant&)>& a_func) const
{
	using Result = RE::BSVisit::BSVisitControl;

	for (auto& variant : _variants) {
		const auto result = a_func(variant);
		if (result == Result::kStop) {
			return result;
		}
	}

	return Result::kContinue;
}

ReplacementAnimation::ReplacementAnimation(uint16_t a_index, uint16_t a_originalIndex, int32_t a_priority, std::string_view a_path, std::string_view a_projectName, Conditions::ConditionSet* a_conditionSet) :
	_index(a_index),
	_originalIndex(a_originalIndex),
	_priority(a_priority),
	_path(a_path),
	_projectName(a_projectName),
	_conditionSet(a_conditionSet)
{}

ReplacementAnimation::ReplacementAnimation(std::vector<Variant>& a_variants, uint16_t a_originalIndex, int32_t a_priority, std::string_view a_path, std::string_view a_projectName, Conditions::ConditionSet* a_conditionSet) :
	_originalIndex(a_originalIndex),
	_priority(a_priority),
	_path(a_path),
	_projectName(a_projectName),
	_conditionSet(a_conditionSet)
{
    _index.emplace<Variants>(a_variants);
	std::get<Variants>(_index)._parentReplacementAnimation = this;
}

bool ReplacementAnimation::ShouldSaveToJson() const
{
	if (GetDisabled()) {
	    return true;
	}

	if (HasVariants()) {
	    bool bShouldSave = false;
		const auto& variants = std::get<Variants>(_index);
		variants.ForEachVariant([&](const Variant& a_variant) {
		    if (a_variant.ShouldSaveToJson()) {
		        bShouldSave = true;
				return RE::BSVisit::BSVisitControl::kStop;
		    }

			return RE::BSVisit::BSVisitControl::kContinue;
		});

		if (bShouldSave) {
		    return true;
		}
	}

	return false;
}

uint16_t ReplacementAnimation::GetIndex() const
{
	if (HasVariants()) {
		return std::get<Variants>(_index).GetVariantIndex();
	}

	return std::get<uint16_t>(_index);
}

uint16_t ReplacementAnimation::GetIndex(ActiveClip* a_activeClip) const
{
	if (HasVariants()) {
		return std::get<Variants>(_index).GetVariantIndex(a_activeClip);
	}

	return std::get<uint16_t>(_index);
}

bool ReplacementAnimation::HasCustomBlendTime(CustomBlendType a_type) const
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

float ReplacementAnimation::GetCustomBlendTime(CustomBlendType a_type) const
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

void ReplacementAnimation::ToggleCustomBlendTime(CustomBlendType a_type, bool a_bEnable)
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

void ReplacementAnimation::SetCustomBlendTime(CustomBlendType a_type, float a_value)
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

SubMod* ReplacementAnimation::GetParentSubMod() const
{
    return _parentSubMod;
}

void ReplacementAnimation::MarkAsSynchronizedAnimation(bool a_bSynchronized)
{
    _bSynchronized = a_bSynchronized;

	if (_parentSubMod) {
		_parentSubMod->SetHasSynchronizedAnimations();
	}
}

void ReplacementAnimation::SetSynchronizedConditionSet(Conditions::ConditionSet* a_synchronizedConditionSet)
{
    _synchronizedConditionSet = a_synchronizedConditionSet;
}

void ReplacementAnimation::LoadAnimData(const ReplacementAnimData& a_replacementAnimData)
{
	SetDisabled(a_replacementAnimData.bDisabled);
	if (HasVariants() && a_replacementAnimData.variants.has_value()) {
		// try to find the matching variant for each variant data in replacement anim data, and load the settings if found
		for (const ReplacementAnimData::Variant& variantData : *a_replacementAnimData.variants) {
			auto& variants = std::get<Variants>(_index);

			auto search = std::ranges::find_if(variants._variants, [&](const Variant& a_variant) {
				return variantData.filename == a_variant.GetFilename();
			});

			if (search != variants._variants.end()) {
				search->SetDisabled(variantData.bDisabled);
				search->SetWeight(variantData.weight);
			}
		}
	}
}

void ReplacementAnimation::ResetVariants()
{
	if (HasVariants()) {
		auto& variants = std::get<Variants>(_index);
		for (Variant& variant : variants._variants) {
			variant.ResetSettings();
		}
	}
}

void ReplacementAnimation::UpdateVariantCache()
{
	if (HasVariants()) {
		std::get<Variants>(_index).UpdateVariantCache();
	}
}

std::string_view ReplacementAnimation::GetVariantFilename(uint16_t a_variantIndex) const
{
	std::string_view filename = ""sv;

	if (HasVariants()) {
		std::get<Variants>(_index).ForEachVariant([&](const Variant& a_variant) {
		    if (a_variant.GetIndex() == a_variantIndex) {
		        filename = a_variant.GetFilename();
				return RE::BSVisit::BSVisitControl::kStop;
		    }
			return RE::BSVisit::BSVisitControl::kContinue;
		});
	}

	return filename;
}

RE::BSVisit::BSVisitControl ReplacementAnimation::ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(Variant&)>& a_func)
{
	if (HasVariants()) {
		return std::get<Variants>(_index).ForEachVariant(a_func);
	}

	return RE::BSVisit::BSVisitControl::kContinue;
}

RE::BSVisit::BSVisitControl ReplacementAnimation::ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(const Variant&)>& a_func) const
{
	if (HasVariants()) {
		return std::get<Variants>(_index).ForEachVariant(a_func);
	}

	return RE::BSVisit::BSVisitControl::kContinue;
}

bool ReplacementAnimation::EvaluateConditions(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const
{
    if (IsDisabled()) {
        return false;
    }

    if (_conditionSet->IsEmpty()) {
        return true;
    }

    return _conditionSet->EvaluateAll(a_refr, a_clipGenerator);
}

bool ReplacementAnimation::EvaluateSynchronizedConditions(RE::TESObjectREFR* a_sourceRefr, RE::TESObjectREFR* a_targetRefr, RE::hkbClipGenerator* a_clipGenerator) const
{
    if (IsDisabled()) {
        return false;
    }

	const bool bPassingSourceConditions = _conditionSet->IsEmpty() || _conditionSet->EvaluateAll(a_sourceRefr, a_clipGenerator);
	const bool bPassingTargetConditions = !_synchronizedConditionSet || _synchronizedConditionSet->IsEmpty() || _synchronizedConditionSet->EvaluateAll(a_targetRefr, a_clipGenerator);

    return bPassingSourceConditions && bPassingTargetConditions;
}
