#include "ActiveAnimationPreview.h"

#include "Havok/Havok.h"
#include "Offsets.h"
#include "OpenAnimationReplacer.h"

ActiveAnimationPreview::ActiveAnimationPreview(RE::hkbBehaviorGraph* a_behaviorGraph, ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex /* = std::nullopt*/) :
    _behaviorGraph(a_behaviorGraph),
    _replacementAnimation(a_replacementAnimation)
{
	_previewClipGenerator = std::make_unique<FakeClipGenerator>(a_behaviorGraph, a_replacementAnimation, a_variantIndex);
}

void ActiveAnimationPreview::OnUpdate([[maybe_unused]] RE::hkbBehaviorGraph* a_behaviorGraph, const RE::hkbContext& a_context, float a_timestep)
{
	_previewClipGenerator->Update(a_context, a_timestep);
}

void ActiveAnimationPreview::OnGenerate([[maybe_unused]] RE::hkbBehaviorGraph* a_behaviorGraph, [[maybe_unused]] const RE::hkbContext& a_context, RE::hkbGeneratorOutput& a_output)
{
	if (a_output.IsValid(RE::hkbGeneratorOutput::StandardTracks::TRACK_POSE)) {
		RE::hkbGeneratorOutput::Track poseTrack = a_output.GetTrack(RE::hkbGeneratorOutput::StandardTracks::TRACK_POSE);

		auto poseOut = poseTrack.GetDataQsTransform();

		if (const auto binding = _previewClipGenerator->animationControl->binding) {
			if (const auto& previewAnimation = binding->animation) {
				std::vector<RE::hkQsTransform> sampledTransformTracks{};
				sampledTransformTracks.resize(previewAnimation->numberOfTransformTracks);
				std::vector<float> sampledFloatTracks{};
				sampledFloatTracks.resize(previewAnimation->numberOfFloatTracks);

				previewAnimation->SamplePartialTracks(_previewClipGenerator->localTime, previewAnimation->numberOfTransformTracks, sampledTransformTracks.data(), previewAnimation->numberOfFloatTracks, sampledFloatTracks.data(), nullptr);

				auto numBlend = std::min(poseTrack.GetNumData(), previewAnimation->numberOfTransformTracks);
				hkbBlendPoses(numBlend, sampledTransformTracks.data(), poseOut, 0.f, poseOut);
			}
		}
	}
}
