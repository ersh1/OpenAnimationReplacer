#pragma once

#include "API/OpenAnimationReplacer-FunctionTypes.h"

namespace Conditions
{
	class ICondition;
	class ConditionSet;
}

namespace Functions
{
	class IFunction;
	class FunctionSet;
}

class SubMod;
class ReplacerMod;
class ReplacementAnimation;
class Variant;

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
			insertAfterThisCondition(a_insertAfterThisCondition)
		{}

		std::unique_ptr<Conditions::ICondition> conditionToInsert;
		Conditions::ConditionSet* conditionSet;
		const std::unique_ptr<Conditions::ICondition>& insertAfterThisCondition;

		void Run() override;
	};

	struct InsertFunctionJob : GenericJob
	{
		InsertFunctionJob(std::unique_ptr<Functions::IFunction>& a_functionToInsert, Functions::FunctionSet* a_functionSet, const std::unique_ptr<Functions::IFunction>& a_insertAfterThisFunction) :
			functionToInsert(std::move(a_functionToInsert)),
			functionSet(a_functionSet),
			insertAfterThisFunction(a_insertAfterThisFunction)
		{}

		std::unique_ptr<Functions::IFunction> functionToInsert;
		Functions::FunctionSet* functionSet;
		const std::unique_ptr<Functions::IFunction>& insertAfterThisFunction;

		void Run() override;
	};

	struct RemoveConditionJob : GenericJob
	{
		RemoveConditionJob(std::unique_ptr<Conditions::ICondition>& a_conditionToRemove, Conditions::ConditionSet* a_conditionSet) :
			conditionToRemove(a_conditionToRemove),
			conditionSet(a_conditionSet)
		{}

		std::unique_ptr<Conditions::ICondition>& conditionToRemove;
		Conditions::ConditionSet* conditionSet;

		void Run() override;
	};

	struct RemoveFunctionJob : GenericJob
	{
		RemoveFunctionJob(std::unique_ptr<Functions::IFunction>& a_functionToRemove, Functions::FunctionSet* a_functionSet) :
			functionToRemove(a_functionToRemove),
			functionSet(a_functionSet)
		{}

		std::unique_ptr<Functions::IFunction>& functionToRemove;
		Functions::FunctionSet* functionSet;

		void Run() override;
	};

	struct ReplaceConditionJob : GenericJob
	{
		ReplaceConditionJob(std::unique_ptr<Conditions::ICondition>& a_conditionToReplace, std::string_view a_newConditionName, Conditions::ConditionSet* a_conditionSet) :
			conditionToReplace(a_conditionToReplace),
			newConditionName(a_newConditionName),
			conditionSet(a_conditionSet)
		{}

		std::unique_ptr<Conditions::ICondition>& conditionToReplace;
		std::string newConditionName;
		Conditions::ConditionSet* conditionSet;

		void Run() override;
	};

	struct ReplaceFunctionJob : GenericJob
	{
		ReplaceFunctionJob(std::unique_ptr<Functions::IFunction>& a_functionToReplace, std::string_view a_newFunctionName, Functions::FunctionSet* a_functionSet) :
			functionToReplace(a_functionToReplace),
			newFunctionName(a_newFunctionName),
			functionSet(a_functionSet)
		{}

		std::unique_ptr<Functions::IFunction>& functionToReplace;
		std::string newFunctionName;
		Functions::FunctionSet* functionSet;

		void Run() override;
	};

	struct MoveConditionJob : GenericJob
	{
		MoveConditionJob(std::unique_ptr<Conditions::ICondition>& a_sourceCondition, Conditions::ConditionSet* a_sourceSet, const std::unique_ptr<Conditions::ICondition>& a_targetCondition, Conditions::ConditionSet* a_targetSet, bool a_bInsertAfter) :
			sourceCondition(a_sourceCondition),
			sourceSet(a_sourceSet),
			targetCondition(a_targetCondition),
			targetSet(a_targetSet),
			bInsertAfter(a_bInsertAfter)
		{}

		std::unique_ptr<Conditions::ICondition>& sourceCondition;
		Conditions::ConditionSet* sourceSet;
		const std::unique_ptr<Conditions::ICondition>& targetCondition;
		Conditions::ConditionSet* targetSet;
		bool bInsertAfter;

		void Run() override;
	};

	struct MoveFunctionJob : GenericJob
	{
		MoveFunctionJob(std::unique_ptr<Functions::IFunction>& a_sourceFunction, Functions::FunctionSet* a_sourceSet, const std::unique_ptr<Functions::IFunction>& a_targetFunction, Functions::FunctionSet* a_targetSet, bool a_bInsertAfter) :
			sourceFunction(a_sourceFunction),
			sourceSet(a_sourceSet),
			targetFunction(a_targetFunction),
			targetSet(a_targetSet),
			bInsertAfter(a_bInsertAfter)
		{}

		std::unique_ptr<Functions::IFunction>& sourceFunction;
		Functions::FunctionSet* sourceSet;
		const std::unique_ptr<Functions::IFunction>& targetFunction;
		Functions::FunctionSet* targetSet;
		bool bInsertAfter;

		void Run() override;
	};

	struct AddTriggerJob : GenericJob
	{
		AddTriggerJob(std::unique_ptr<Functions::IFunction>& a_function, Functions::FunctionSet* a_functionSet, const Functions::Trigger& a_trigger) :
			func(a_function),
			functionSet(a_functionSet),
			trigger(a_trigger)
		{}

		std::unique_ptr<Functions::IFunction>& func;
		Functions::FunctionSet* functionSet;
		Functions::Trigger trigger;

		void Run() override;
	};

	struct RemoveTriggerJob : GenericJob
	{
		RemoveTriggerJob(std::unique_ptr<Functions::IFunction>& a_function, Functions::FunctionSet* a_functionSet, const Functions::Trigger& a_trigger) :
			func(a_function),
			functionSet(a_functionSet),
			trigger(a_trigger)
		{}

		std::unique_ptr<Functions::IFunction>& func;
		Functions::FunctionSet* functionSet;
		Functions::Trigger trigger;

		void Run() override;
	};

	struct ClearConditionSetJob : GenericJob
	{
		ClearConditionSetJob(Conditions::ConditionSet* a_conditionSet) :
			conditionSet(a_conditionSet)
		{}

		Conditions::ConditionSet* conditionSet;

		void Run() override;
	};

	struct ClearFunctionSetJob : GenericJob
	{
		ClearFunctionSetJob(Functions::FunctionSet* a_functionSet) :
			functionSet(a_functionSet)
		{}

		Functions::FunctionSet* functionSet;

		void Run() override;
	};

	struct UpdateSubModJob : GenericJob
	{
		UpdateSubModJob(SubMod* a_subMod, bool a_bCheckProblems) :
			subMod(a_subMod),
			bCheckProblems(a_bCheckProblems)
		{}

		SubMod* subMod;
		bool bCheckProblems;

		void Run() override;
	};

	struct ReloadSubModConfigJob : GenericJob
	{
		ReloadSubModConfigJob(SubMod* a_subMod) :
			subMod(a_subMod)
		{}

		SubMod* subMod;

		void Run() override;
	};

	struct ReloadReplacerModConfigJob : GenericJob
	{
		ReloadReplacerModConfigJob(ReplacerMod* a_replacerMod) :
			replacerMod(a_replacerMod)
		{}

		ReplacerMod* replacerMod;

		void Run() override;
	};

	struct BeginPreviewAnimationJob : GenericJob
	{
		BeginPreviewAnimationJob(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, Variant* a_variant = nullptr) :
			refr(a_refr),
			replacementAnimation(a_replacementAnimation),
			variant(a_variant)
		{}

		BeginPreviewAnimationJob(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, std::string_view a_syncAnimationPrefix, Variant* a_variant = nullptr) :
			refr(a_refr),
			replacementAnimation(a_replacementAnimation),
			syncAnimationPrefix(a_syncAnimationPrefix),
			variant(a_variant)
		{}

		RE::TESObjectREFR* refr;
		const ReplacementAnimation* replacementAnimation;
		Variant* variant;
		std::string syncAnimationPrefix{};

		void Run() override;
	};

	struct StopPreviewAnimationJob : GenericJob
	{
		StopPreviewAnimationJob(RE::TESObjectREFR* a_refr) :
			refr(a_refr)
		{}

		RE::TESObjectREFR* refr;

		void Run() override;
	};

	struct RemoveConditionPresetJob : GenericJob
	{
		RemoveConditionPresetJob(ReplacerMod* a_replacerMod, std::string_view a_conditionPresetName) :
			replacerMod(a_replacerMod),
			conditionPresetName(a_conditionPresetName)
		{}

		ReplacerMod* replacerMod;
		std::string conditionPresetName;

		void Run() override;
	};

	struct NotifyAnimationGraphJob : GenericJob
	{
		NotifyAnimationGraphJob(RE::TESObjectREFR* a_refr, const RE::BSString& a_eventName) :
			refr(a_refr),
			eventName(a_eventName)
		{}

		RE::TESObjectREFR* refr;
		RE::BSString eventName;

		void Run() override;
	};
}
