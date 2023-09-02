#include "ActiveSynchronizedAnimation.h"

#include "OpenAnimationReplacer.h"
#include "Offsets.h"

ActiveSynchronizedAnimation::ActiveSynchronizedAnimation(RE::BGSSynchronizedAnimationInstance* a_synchronizedAnimationInstance, [[maybe_unused]] const RE::hkbContext& a_context) :
    _synchronizedAnimationInstance(a_synchronizedAnimationInstance)
{}

ActiveSynchronizedAnimation::~ActiveSynchronizedAnimation()
{
	ReadLocker locker(_clipDataLock);

    for (auto& [synchronizedClipGenerator, originalClipData] : _originalClipDatas)
    {
        synchronizedClipGenerator->synchronizedAnimationBindingIndex = originalClipData.originalSynchronizedIndex;
        synchronizedClipGenerator->clipGenerator->animationBindingIndex = originalClipData.originalInternalClipIndex;
    }
}

void ActiveSynchronizedAnimation::OnSynchronizedClipActivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context)
{
    {
        WriteLocker locker(_clipDataLock);
        _originalClipDatas.emplace(a_synchronizedClipGenerator, OriginalClipData(a_synchronizedClipGenerator));
    }

	if (a_synchronizedClipGenerator->synchronizedAnimationBindingIndex != static_cast<uint16_t>(-1)) {
		a_synchronizedClipGenerator->synchronizedAnimationBindingIndex += OpenAnimationReplacer::GetSingleton().GetSynchronizedClipsIDOffset(a_context.character);
	}

	bool bAdded;
	const auto activeClip = OpenAnimationReplacer::GetSingleton().AddOrGetActiveClip(a_synchronizedClipGenerator->clipGenerator, a_context, bAdded);

	// consider replacement
	if (const auto replacements = OpenAnimationReplacer::GetSingleton().GetReplacements(a_context.character, a_synchronizedClipGenerator->clipGenerator->animationBindingIndex)) {
		if (const auto scene = a_synchronizedClipGenerator->synchronizedScene) {
			if (scene->refHandles.size() > 1) {
				const auto sourceRef = scene->refHandles[0].get();
				const auto targetRef = scene->refHandles[1].get();
				if (const auto replacementAnimation = replacements->EvaluateSynchronizedConditionsAndGetReplacementAnimation(sourceRef.get(), targetRef.get(), a_synchronizedClipGenerator->clipGenerator)) {
					if (!_variantIndex.has_value() && replacementAnimation->HasVariants()) {
						_variantIndex = replacementAnimation->GetIndex();
					}

					activeClip->OnActivateSynchronized(replacements, replacementAnimation, _variantIndex);
				}
			}
		}
	}

	auto& animationLog = AnimationLog::GetSingleton();
	const auto event = activeClip->IsOriginal() ? AnimationLogEntry::Event::kActivateSynchronized : AnimationLogEntry::Event::kActivateReplaceSynchronized;
	if (bAdded && animationLog.ShouldLogAnimations() && !activeClip->IsTransitioning() && animationLog.ShouldLogAnimationsForActiveClip(activeClip, event)) {
		animationLog.LogAnimation(event, activeClip, a_context.character);
	}
}

void ActiveSynchronizedAnimation::OnSynchronizedClipDeactivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, [[maybe_unused]] const RE::hkbContext& a_context)
{
	WriteLocker locker(_clipDataLock);

    auto it = _originalClipDatas.find(a_synchronizedClipGenerator);
    if (it != _originalClipDatas.end())
    {
		a_synchronizedClipGenerator->synchronizedAnimationBindingIndex = it->second.originalSynchronizedIndex;
		a_synchronizedClipGenerator->clipGenerator->animationBindingIndex = it->second.originalInternalClipIndex;
    }

	_originalClipDatas.erase(it);
}

bool ActiveSynchronizedAnimation::HasRef(RE::TESObjectREFR* a_refr) const
{
    return _synchronizedAnimationInstance->HasRef(a_refr->GetHandle());
}
