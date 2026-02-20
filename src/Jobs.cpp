#include "Jobs.h"

#include "API/OpenAnimationReplacer-ConditionTypes.h"
#include "Conditions.h"
#include "DetectedProblems.h"
#include "OpenAnimationReplacer.h"
#include "ReplacerMods.h"

namespace Jobs
{
	void InsertConditionJob::Run()
	{
		conditionSet->Insert(conditionToInsert, insertAfterThisCondition, true);
	}

	void InsertFunctionJob::Run()
	{
		functionSet->Insert(functionToInsert, insertAfterThisFunction, true);
	}

	void RemoveConditionJob::Run()
	{
		conditionSet->Remove(conditionToRemove);
	}

	void RemoveFunctionJob::Run()
	{
		functionSet->Remove(functionToRemove);
	}

	void ReplaceConditionJob::Run()
	{
		auto newCondition = Conditions::CreateCondition(newConditionName);
		conditionSet->Replace(conditionToReplace, newCondition);
	}

	void ReplaceFunctionJob::Run()
	{
		auto newFunction = Functions::CreateFunction(newFunctionName);
		functionSet->Replace(functionToReplace, newFunction);
	}

	void MoveConditionJob::Run()
	{
		targetSet->Move(sourceCondition, sourceSet, targetCondition, bInsertAfter);
	}

	void MoveFunctionJob::Run()
	{
		targetSet->Move(sourceFunction, sourceSet, targetFunction, bInsertAfter);
	}

	void ClearConditionSetJob::Run()
	{
		conditionSet->Clear();
	}

	void ClearFunctionSetJob::Run()
	{
		functionSet->Clear();
	}

	void UpdateSubModJob::Run()
	{
		subMod->UpdateAnimations();
		if (bCheckProblems) {
			auto& detectedProblems = DetectedProblems::GetSingleton();
			detectedProblems.CheckForSubModsSharingPriority();
			detectedProblems.CheckForSubModsWithInvalidEntries();
		}
	}

	void ReloadSubModConfigJob::Run()
	{
		subMod->ReloadConfig();
	}

	void ReloadReplacerModConfigJob::Run()
	{
		replacerMod->ReloadConfig();
	}

	void BeginPreviewAnimationJob::Run()
	{
		RE::BSAnimationGraphManagerPtr graphManager = nullptr;
		refr->GetAnimationGraphManager(graphManager);
		if (graphManager) {
			const auto& activeGraph = graphManager->graphs[graphManager->GetRuntimeData().activeGraph];
			if (activeGraph->behaviorGraph) {
				OpenAnimationReplacer::GetSingleton().AddActiveAnimationPreview(activeGraph->behaviorGraph, replacementAnimation, syncAnimationPrefix, variant);
			}
		}
	}

	void StopPreviewAnimationJob::Run()
	{
		RE::BSAnimationGraphManagerPtr graphManager = nullptr;
		refr->GetAnimationGraphManager(graphManager);
		if (graphManager) {
			const auto& activeGraph = graphManager->graphs[graphManager->GetRuntimeData().activeGraph];
			if (activeGraph->behaviorGraph) {
				OpenAnimationReplacer::GetSingleton().RemoveActiveAnimationPreview(activeGraph->behaviorGraph);
			}
		}
	}

	void RemoveConditionPresetJob::Run()
	{
		replacerMod->RemoveConditionPreset(conditionPresetName);
	}

	void NotifyAnimationGraphJob::Run()
	{
		refr->NotifyAnimationGraph(eventName);
	}

	void AddTriggerJob::Run()
	{
		func->AddTrigger(trigger.event, trigger.payload);
	}

	void RemoveTriggerJob::Run()
	{
		func->RemoveTrigger(trigger.event, trigger.payload);
	}

}
