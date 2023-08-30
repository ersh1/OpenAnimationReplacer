#pragma once
#include "FakeClipGenerator.h"
#include "ReplacementAnimation.h"

class ActiveAnimationPreview
{
public:
	[[nodiscard]] ActiveAnimationPreview(RE::hkbBehaviorGraph* a_behaviorGraph, const ReplacementAnimation* a_replacementAnimation, std::string_view a_syncAnimationPrefix, std::optional<uint16_t> a_variantIndex = std::nullopt);

	void OnUpdate(RE::hkbBehaviorGraph* a_behaviorGraph, const RE::hkbContext& a_context, float a_timestep);
	void OnGenerate(RE::hkbBehaviorGraph* a_behaviorGraph, const RE::hkbContext& a_context, RE::hkbGeneratorOutput& a_output);

	RE::hkbBehaviorGraph* GetBehaviorGraph() const { return _behaviorGraph; }
	const ReplacementAnimation* GetReplacementAnimation() const { return _replacementAnimation; }
	uint16_t GetCurrentBindingIndex() const { return _previewClipGenerator->animationBindingIndex; }

protected:
	RE::hkbBehaviorGraph* _behaviorGraph;
	const ReplacementAnimation* _replacementAnimation;

	std::unique_ptr<FakeClipGenerator> _previewClipGenerator = nullptr;
};
