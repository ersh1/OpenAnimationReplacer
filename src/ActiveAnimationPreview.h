#pragma once
#include "FakeClipGenerator.h"
#include "ReplacementAnimation.h"

class ActiveAnimationPreview
{
public:
	[[nodiscard]] ActiveAnimationPreview(RE::hkbBehaviorGraph* a_behaviorGraph, ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex = std::nullopt);

	void OnUpdate(RE::hkbBehaviorGraph* a_behaviorGraph, const RE::hkbContext& a_context, float a_timestep);
	void OnGenerate(RE::hkbBehaviorGraph* a_behaviorGraph, const RE::hkbContext& a_context, RE::hkbGeneratorOutput& a_output);

	RE::hkbBehaviorGraph* GetBehaviorGraph() const { return _behaviorGraph; }
	ReplacementAnimation* GetReplacementAnimation() const { return _replacementAnimation; }
	uint16_t GetCurrentBindingIndex() const { return _previewClipGenerator->animationBindingIndex; }

protected:
	RE::hkbBehaviorGraph* _behaviorGraph;
	ReplacementAnimation* _replacementAnimation;

	std::unique_ptr<FakeClipGenerator> _previewClipGenerator = nullptr;
};
