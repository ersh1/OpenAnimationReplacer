#include "Jobs.h"

#include "OpenAnimationReplacer.h"

namespace Jobs
{
    void BeginPreviewAnimationJob::Run()
    {
		RE::BSAnimationGraphManagerPtr graphManager = nullptr;
		refr->GetAnimationGraphManager(graphManager);
		if (graphManager) {
			const auto& activeGraph = graphManager->graphs[graphManager->GetRuntimeData().activeGraph];
			if (activeGraph->behaviorGraph) {
				OpenAnimationReplacer::GetSingleton().AddActiveAnimationPreview(activeGraph->behaviorGraph, replacementAnimation, syncAnimationPrefix, variantIndex);
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
}
