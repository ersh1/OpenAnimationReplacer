#include "ActiveAnimationPreview.h"

#include "Havok/Havok.h"
#include "Offsets.h"
#include "OpenAnimationReplacer.h"

ActiveAnimationPreview::ActiveAnimationPreview(RE::hkbBehaviorGraph* a_behaviorGraph, const ReplacementAnimation* a_replacementAnimation, std::string_view a_syncAnimationPrefix, Variant* a_variant) :
	_behaviorGraph(a_behaviorGraph),
	_replacementAnimation(a_replacementAnimation)
{
	_previewClipGenerator = std::make_unique<FakeClipGenerator>(a_behaviorGraph, a_replacementAnimation, a_syncAnimationPrefix, a_variant);
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
				auto numTransformTracks = GetNumTransformTracks(binding->transformTrackToBoneIndices, nullptr, poseTrack.GetNumData(), previewAnimation->numberOfTransformTracks) + 1;
				if (a_context.character->numTracksInLOD > -1 && a_context.character->numTracksInLOD < numTransformTracks) {
					numTransformTracks = a_context.character->numTracksInLOD;
				}

				std::vector<RE::hkQsTransform> sampledTransformTracks{};
				sampledTransformTracks.resize(numTransformTracks);
				std::vector<float> sampledFloatTracks{};
				sampledFloatTracks.resize(previewAnimation->numberOfFloatTracks);

				previewAnimation->SamplePartialTracks(_previewClipGenerator->localTime, numTransformTracks, sampledTransformTracks.data(), previewAnimation->numberOfFloatTracks, sampledFloatTracks.data(), nullptr);

				RE::hkaMirroredSkeleton* mirroredSkeleton = nullptr;
				if ((_previewClipGenerator->flags & 4) != 0) {
					mirroredSkeleton = hkbCharacterSetup_GetMirroredSkeleton(a_context.character->setup.get());
				}

				int16_t poseTrackNumData = poseTrack.GetNumData();
				float* poseTrackData = poseTrack.GetDataReal();

				// ???
				RE::hkArray<float> array;
				array._data = &poseTrackData[0x30 * poseTrackNumData];
				array._size = (poseTrackNumData + 0x20) >> 5;
				array._capacityAndFlags = array._size | (std::uint32_t)1 << 31;

				if (array.size() > 0) {
					for (auto& elem : array) {
						elem = 0.f;
					}
				}

				std::vector<RE::hkQsTransform> convertedTransformTracks{};
				convertedTransformTracks.resize(poseTrackNumData);

				ConvertTrackPoseToBonePose(numTransformTracks, sampledTransformTracks.data(), poseTrackNumData, convertedTransformTracks.data(), &array, binding->transformTrackToBoneIndices, binding->blendHint == RE::hkaAnimationBinding::BlendHint::kAdditive, mirroredSkeleton, true);

				auto numBlend = std::min(poseTrackNumData, static_cast<int16_t>(numTransformTracks));
				hkbBlendPoses(numBlend, convertedTransformTracks.data(), poseOut, 0.f, poseOut);
			}
		}
	}
}
