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

	struct BeginPreviewAnimationJob : GenericJob
	{
		BeginPreviewAnimationJob(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex = std::nullopt) :
			refr(a_refr),
			replacementAnimation(a_replacementAnimation),
			variantIndex(a_variantIndex) {}

		BeginPreviewAnimationJob(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, std::string_view a_syncAnimationPrefix, std::optional<uint16_t> a_variantIndex = std::nullopt) :
			refr(a_refr),
			replacementAnimation(a_replacementAnimation),
			syncAnimationPrefix(a_syncAnimationPrefix),
			variantIndex(a_variantIndex) {}

		RE::TESObjectREFR* refr;
		const ReplacementAnimation* replacementAnimation;
		std::optional<uint16_t> variantIndex;
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

	struct RemoveSharedRandomFloatJob : LatentJob
	{
		RemoveSharedRandomFloatJob(float a_delay, SubMod* a_subMod, RE::hkbBehaviorGraph* a_behaviorGraph) :
			LatentJob(a_delay),
			subMod(a_subMod),
			behaviorGraph(a_behaviorGraph) {}

		SubMod* subMod;
		RE::hkbBehaviorGraph* behaviorGraph;

		bool Run(float a_deltaTime) override
		{
			_timeRemaining -= a_deltaTime;

			if (_timeRemaining > 0.f) {
				return false;
			}

			subMod->ClearSharedRandom(behaviorGraph);
			return true;
		}
	};
}
