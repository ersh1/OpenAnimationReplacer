#include "ReplacementAnimation.h"

#include "AnimationFileHashCache.h"
#include "OpenAnimationReplacer.h"
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

ReplacementAnimation::ReplacementAnimation(uint16_t a_index, uint16_t a_originalIndex, std::string_view a_path, std::string_view a_projectName, Conditions::ConditionSet* a_conditionSet) :
	_index(a_index),
	_originalIndex(a_originalIndex),
	_path(a_path),
	_projectName(a_projectName),
	_conditionSet(a_conditionSet)
{}

ReplacementAnimation::ReplacementAnimation(std::vector<Variant>& a_variants, uint16_t a_originalIndex, std::string_view a_path, std::string_view a_projectName, Conditions::ConditionSet* a_conditionSet) :
	_originalIndex(a_originalIndex),
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
		const auto variantMode = variants.GetVariantMode();

		if (variantMode != VariantMode::kRandom) {
			return true;
		}

		if (variants.GetVariantStateScope() != Conditions::StateDataScope::kLocal) {
			return true;
		}

		if (!variants.ShouldBlendBetweenVariants()) {
			return true;
		}

		if (!variants.ShouldResetRandomOnLoopOrEcho()) {
			return true;
		}

		if (variants.ShouldSharePlayedHistory()) {
			return true;
		}

		variants.ForEachVariant([&](const Variant& a_variant) {
			if (a_variant.ShouldSaveToJson(variantMode)) {
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

bool ReplacementAnimation::IsDisabled() const
{
	return _bDisabled || _parentSubMod->IsDisabled();
}

uint16_t ReplacementAnimation::GetIndex(Variant*& a_outVariant) const
{
	if (HasVariants()) {
		return std::get<Variants>(_index).GetVariantIndex(a_outVariant);
	}

	return std::get<uint16_t>(_index);
}

uint16_t ReplacementAnimation::GetIndex(Variant*& a_outVariant, float a_randomWeight) const
{
	if (HasVariants()) {
		return std::get<Variants>(_index).GetVariantIndex(a_outVariant, a_randomWeight);
	}

	return std::get<uint16_t>(_index);
}

uint16_t ReplacementAnimation::GetIndex(ActiveClip* a_activeClip, Variant*& a_outVariant) const
{
	if (HasVariants()) {
		return std::get<Variants>(_index).GetVariantIndex(a_activeClip, a_outVariant);
	}

	return std::get<uint16_t>(_index);
}

int32_t ReplacementAnimation::GetPriority() const
{
	return _parentSubMod->GetPriority();
}

bool ReplacementAnimation::GetIgnoreDontConvertAnnotationsToTriggersFlag() const
{
	return _parentSubMod->IsIgnoringDontConvertAnnotationsToTriggersFlag();
}

bool ReplacementAnimation::GetTriggersFromAnnotationsOnly() const
{
	return _parentSubMod->IsOnlyUsingTriggersFromAnnotations();
}

bool ReplacementAnimation::GetInterruptible() const
{
	return _parentSubMod->IsInterruptible();
}

bool ReplacementAnimation::GetReplaceOnLoop() const
{
	return _parentSubMod->IsReevaluatingOnLoop();
}

bool ReplacementAnimation::GetReplaceOnEcho() const
{
	return _parentSubMod->IsReevaluatingOnEcho();
}

float ReplacementAnimation::GetCustomBlendTime(ActiveClip* a_activeClip, CustomBlendType a_type, bool a_bCheckWithinSequence, bool a_bIsBetweenVariants) const
{
	if (a_bCheckWithinSequence && a_activeClip) {
		if (a_type != CustomBlendType::kInterrupt) {
			if (HasVariants()) {
				// figure out if we're in the middle of a variant sequence
				const auto& variants = GetVariants();
				if (!variants.ShouldBlendBetweenVariants()) {
					if (a_bIsBetweenVariants) {
						return 0.f;
					}
					if (const auto stateData = variants.GetVariantStateData(a_activeClip)) {
						if (!stateData->IsAtBeginningOfSequence(a_activeClip->GetClipGenerator(), &variants)) {
							return 0.f;
						}
					}
				}
			}
		}
	}

	return _parentSubMod->GetCustomBlendTime(a_type);
}

SubMod* ReplacementAnimation::GetParentSubMod() const
{
	return _parentSubMod;
}

Functions::FunctionSet* ReplacementAnimation::GetFunctionSet(Functions::FunctionSetType a_setType) const
{
	return _parentSubMod ? _parentSubMod->GetFunctionSet(a_setType) : nullptr;
}

bool ReplacementAnimation::HasValidFunctionSet(Functions::FunctionSetType a_setType) const
{
	return _parentSubMod ? _parentSubMod->HasValidFunctionSet(a_setType) : false;
}

bool ReplacementAnimation::GetRunFunctionsOnLoop() const
{
	return _parentSubMod ? _parentSubMod->IsRunningFunctionsOnLoop() : false;
}

bool ReplacementAnimation::GetRunFunctionsOnEcho() const
{
	return _parentSubMod ? _parentSubMod->IsRunningFunctionsOnEcho() : false;
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
	if (HasVariants()) {
		auto& variants = std::get<Variants>(_index);

		if (a_replacementAnimData.variants.has_value()) {
			// try to find the matching variant for each variant data in replacement anim data, and load the settings if found
			for (const ReplacementAnimData::Variant& variantData : *a_replacementAnimData.variants) {
				auto search = std::ranges::find_if(variants._variants, [&](const Variant& a_variant) {
					return variantData.filename == a_variant.GetFilename();
				});

				if (search != variants._variants.end()) {
					search->SetWeight(variantData.weight);
					search->SetDisabled(variantData.bDisabled);
					search->SetOrder(variantData.order);
					search->SetPlayOnce(variantData.bPlayOnce);
				}
			}
		}

		if (a_replacementAnimData.variantMode.has_value()) {
			variants.SetVariantMode(*a_replacementAnimData.variantMode);
		}

		if (a_replacementAnimData.variantStateScope.has_value()) {
			variants.SetVariantStateScope(*a_replacementAnimData.variantStateScope);
		}

		variants.SetShouldBlendBetweenVariants(a_replacementAnimData.bBlendBetweenVariants);
		variants.SetShouldResetRandomOnLoopOrEcho(a_replacementAnimData.bResetRandomOnLoopOrEcho);
		variants.SetShouldSharePlayedHistory(a_replacementAnimData.bSharePlayedHistory);
	}
}

void ReplacementAnimation::ResetVariants()
{
	if (HasVariants()) {
		auto& variants = std::get<Variants>(_index);
		variants.ResetSettings();
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
	auto& animationLog = AnimationLog::GetSingleton();
	ReplacementTrace* trace = animationLog.ShouldLogAnimationsForRefr(a_refr) ? OpenAnimationReplacer::GetSingleton().GetTrace(a_refr, a_clipGenerator) : nullptr;

	if (trace) {
		trace->TraceAnimation(this);
	}

	if (IsDisabled()) {
		if (trace) {
			trace->SetTraceAnimationResult(ReplacementTrace::Step::StepResult::kDisabled);
		}
		return false;
	}

	if (_conditionSet->IsEmpty()) {
		if (trace) {
			trace->SetTraceAnimationResult(ReplacementTrace::Step::StepResult::kNoConditions);
		}
		return true;
	}

	bool bResult = _conditionSet->EvaluateAll(a_refr, a_clipGenerator, _parentSubMod);
	if (trace) {
		trace->SetTraceAnimationResult(bResult ? ReplacementTrace::Step::StepResult::kSuccess : ReplacementTrace::Step::StepResult::kFail);
	}

	return bResult;
}

bool ReplacementAnimation::EvaluateSynchronizedConditions(RE::TESObjectREFR* a_sourceRefr, RE::TESObjectREFR* a_targetRefr, RE::hkbClipGenerator* a_clipGenerator) const
{
	auto& animationLog = AnimationLog::GetSingleton();
	bool bTrace = animationLog.ShouldLogAnimationsForRefr(a_sourceRefr) || animationLog.ShouldLogAnimationsForRefr(a_targetRefr);
	ReplacementTrace* trace = bTrace ? OpenAnimationReplacer::GetSingleton().GetTrace(a_sourceRefr, a_clipGenerator) : nullptr;

	if (trace) {
		trace->TraceAnimation(this);
	}

	if (IsDisabled()) {
		if (trace) {
			trace->SetTraceAnimationResult(ReplacementTrace::Step::StepResult::kDisabled);
		}
		return false;
	}

	if (trace) {
		trace->SetEvaluatingSynchronizedConditions(false);
	}
	const bool bPassingSourceConditions = _conditionSet->IsEmpty() || _conditionSet->EvaluateAll(a_sourceRefr, a_clipGenerator, _parentSubMod, bTrace);

	if (trace) {
		trace->SetEvaluatingSynchronizedConditions(true);
	}
	const bool bPassingTargetConditions = !_synchronizedConditionSet || _synchronizedConditionSet->IsEmpty() || _synchronizedConditionSet->EvaluateAll(a_targetRefr, a_clipGenerator, _parentSubMod, bTrace);

	if (trace) {
		if (_conditionSet->IsEmpty() && (!_synchronizedConditionSet || _synchronizedConditionSet->IsEmpty())) {
			trace->SetTraceAnimationResult(ReplacementTrace::Step::StepResult::kNoConditions);
		} else {
			trace->SetTraceAnimationResult(bPassingSourceConditions && bPassingTargetConditions ? ReplacementTrace::Step::StepResult::kSuccess : ReplacementTrace::Step::StepResult::kFail);
		}
	}

	return bPassingSourceConditions && bPassingTargetConditions;
}
