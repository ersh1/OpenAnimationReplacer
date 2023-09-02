#pragma once
#include "ReplacementAnimation.h"
#include "Havok/Havok.h"

class ActiveSynchronizedAnimation
{
public:
	[[nodiscard]] ActiveSynchronizedAnimation(RE::BGSSynchronizedAnimationInstance* a_synchronizedAnimationInstance, const RE::hkbContext& a_context);
	~ActiveSynchronizedAnimation();

	void OnSynchronizedClipActivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context);
	void OnSynchronizedClipDeactivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context);

	bool HasRef(RE::TESObjectREFR* a_refr) const;

protected:
    struct OriginalClipData
    {
		OriginalClipData(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator) :
			originalSynchronizedIndex(a_synchronizedClipGenerator->synchronizedAnimationBindingIndex),
			originalInternalClipIndex(a_synchronizedClipGenerator->clipGenerator->animationBindingIndex)
        {}

		const uint16_t originalSynchronizedIndex;
		const uint16_t originalInternalClipIndex;
    };

	RE::BGSSynchronizedAnimationInstance* _synchronizedAnimationInstance;

	SharedLock _clipDataLock;
	std::unordered_map<RE::BSSynchronizedClipGenerator*, OriginalClipData> _originalClipDatas;

	std::optional<uint16_t> _variantIndex = std::nullopt;
};
