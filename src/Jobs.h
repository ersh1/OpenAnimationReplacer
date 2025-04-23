#pragma once

#include "DetectedProblems.h"

namespace Jobs
{
	struct GenericJob
	{
		GenericJob() = default;
		virtual ~GenericJob() = default;

		virtual void Run() = 0;
	};

	struct LatentJob
	{
		LatentJob() = delete;
		LatentJob(float a_delay) :
			_timeRemaining(a_delay)
		{}

		virtual ~LatentJob() = default;

		virtual bool Run(float a_deltaTime) = 0;

		float _timeRemaining;
	};

	struct InsertConditionJob : GenericJob
	{
		InsertConditionJob(std::unique_ptr<Conditions::ICondition>& a_conditionToInsert, Conditions::ConditionSet* a_conditionSet, const std::unique_ptr<Conditions::ICondition>& a_insertAfterThisCondition) :
			conditionToInsert(std::move(a_conditionToInsert)),
			conditionSet(a_conditionSet),
			insertAfterThisCondition(a_insertAfterThisCondition) {}

		std::unique_ptr<Conditions::ICondition> conditionToInsert;
		Conditions::ConditionSet* conditionSet;
		const std::unique_ptr<Conditions::ICondition>& insertAfterThisCondition;

		void Run() override
		{
			conditionSet->InsertCondition(conditionToInsert, insertAfterThisCondition, true);
		}
	};

	struct RemoveConditionJob : GenericJob
	{
		RemoveConditionJob(std::unique_ptr<Conditions::ICondition>& a_conditionToRemove, Conditions::ConditionSet* a_conditionSet) :
			conditionToRemove(a_conditionToRemove),
			conditionSet(a_conditionSet) {}

		std::unique_ptr<Conditions::ICondition>& conditionToRemove;
		Conditions::ConditionSet* conditionSet;

		void Run() override
		{
			conditionSet->RemoveCondition(conditionToRemove);
		}
	};

	struct ReplaceConditionJob : GenericJob
	{
		ReplaceConditionJob(std::unique_ptr<Conditions::ICondition>& a_conditionToReplace, std::string_view a_newConditionName, Conditions::ConditionSet* a_conditionSet) :
			conditionToReplace(a_conditionToReplace),
			newConditionName(a_newConditionName),
			conditionSet(a_conditionSet) {}

		std::unique_ptr<Conditions::ICondition>& conditionToReplace;
		std::string newConditionName;
		Conditions::ConditionSet* conditionSet;

		void Run() override
		{
			auto newCondition = Conditions::CreateCondition(newConditionName);
			conditionSet->ReplaceCondition(conditionToReplace, newCondition);
		}
	};

	struct MoveConditionJob : GenericJob
	{
		MoveConditionJob(std::unique_ptr<Conditions::ICondition>& a_sourceCondition, Conditions::ConditionSet* a_sourceSet, const std::unique_ptr<Conditions::ICondition>& a_targetCondition, Conditions::ConditionSet* a_targetSet, bool a_bInsertAfter) :
			sourceCondition(a_sourceCondition),
			sourceSet(a_sourceSet),
			targetCondition(a_targetCondition),
			targetSet(a_targetSet),
			bInsertAfter(a_bInsertAfter) {}

		std::unique_ptr<Conditions::ICondition>& sourceCondition;
		Conditions::ConditionSet* sourceSet;
		const std::unique_ptr<Conditions::ICondition>& targetCondition;
		Conditions::ConditionSet* targetSet;
		bool bInsertAfter;

		void Run() override
		{
			targetSet->MoveCondition(sourceCondition, sourceSet, targetCondition, bInsertAfter);
		}
	};

	struct ClearConditionSetJob : GenericJob
	{
		ClearConditionSetJob(Conditions::ConditionSet* a_conditionSet) :
			conditionSet(a_conditionSet) {}

		Conditions::ConditionSet* conditionSet;

		void Run() override
		{
			conditionSet->ClearConditions();
		}
	};

	struct UpdateSubModJob : GenericJob
	{
		UpdateSubModJob(SubMod* a_subMod, bool a_bCheckProblems) :
			subMod(a_subMod),
			bCheckProblems(a_bCheckProblems) {}

		SubMod* subMod;
		bool bCheckProblems;

		void Run() override
		{
			subMod->UpdateAnimations();
			if (bCheckProblems) {
				auto& detectedProblems = DetectedProblems::GetSingleton();
				detectedProblems.CheckForSubModsSharingPriority();
				detectedProblems.CheckForSubModsWithInvalidConditions();
			}
		}
	};

	struct ReloadSubModConfigJob : GenericJob
	{
		ReloadSubModConfigJob(SubMod* a_subMod) :
			subMod(a_subMod) {}

		SubMod* subMod;

		void Run() override
		{
			subMod->ReloadConfig();
		}
	};

	struct ReloadReplacerModConfigJob : GenericJob
	{
		ReloadReplacerModConfigJob(ReplacerMod* a_replacerMod) :
			replacerMod(a_replacerMod) {}

		ReplacerMod* replacerMod;

		void Run() override
		{
			replacerMod->ReloadConfig();
		}
	};

	struct BeginPreviewAnimationJob : GenericJob
	{
		BeginPreviewAnimationJob(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, Variant* a_variant = nullptr) :
			refr(a_refr),
			replacementAnimation(a_replacementAnimation),
			variant(a_variant) {}

		BeginPreviewAnimationJob(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, std::string_view a_syncAnimationPrefix, Variant* a_variant = nullptr) :
			refr(a_refr),
			replacementAnimation(a_replacementAnimation),
			syncAnimationPrefix(a_syncAnimationPrefix),
			variant(a_variant) {}

		RE::TESObjectREFR* refr;
		const ReplacementAnimation* replacementAnimation;
		Variant* variant;
		std::string syncAnimationPrefix{};

		void Run() override;
	};

	struct StopPreviewAnimationJob : GenericJob
	{
		StopPreviewAnimationJob(RE::TESObjectREFR* a_refr) :
			refr(a_refr) {}

		RE::TESObjectREFR* refr;

		void Run() override;
	};

	struct RemoveConditionPresetJob : GenericJob
	{
		RemoveConditionPresetJob(ReplacerMod* a_replacerMod, std::string_view a_conditionPresetName) :
			replacerMod(a_replacerMod),
			conditionPresetName(a_conditionPresetName) {}

		ReplacerMod* replacerMod;
		std::string conditionPresetName;

		void Run() override
		{
			replacerMod->RemoveConditionPreset(conditionPresetName);
		}
	};
}
