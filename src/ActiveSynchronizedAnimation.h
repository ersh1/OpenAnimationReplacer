#pragma once
#include "Havok/Havok.h"
#include "ReplacementAnimation.h"
#include "ReplacerMods.h"

class ActiveSynchronizedAnimation
{
public:
	[[nodiscard]] ActiveSynchronizedAnimation(RE::BGSSynchronizedAnimationInstance* a_synchronizedAnimationInstance);
	~ActiveSynchronizedAnimation();

	void OnSynchronizedClipPreActivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context);
	void OnSynchronizedClipPostActivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context);
	void OnSynchronizedClipPreUpdate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context, float a_timestep);
	void OnSynchronizedClipPostUpdate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context, float a_timestep);
	void OnSynchronizedClipDeactivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context);

	[[nodiscard]] bool ShouldSynchronizedClipReplacementBeActive() const;
	[[nodiscard]] bool IsReplacementActive() const { return _bIsReplacementActive; }
	[[nodiscard]] bool IsFromActiveGraph(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator) const;
	[[nodiscard]] bool IsFromInactiveGraph(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator) const;
	[[nodiscard]] bool IsTransitioning() const { return _bTransitioning; }
	void SetTransitioning(bool a_bValue) { _bTransitioning = a_bValue; }

	[[nodiscard]] bool HasRef(RE::TESObjectREFR* a_refr) const;

	[[nodiscard]] ReplacementTrace* GetTrace();

protected:
	struct SynchronizedClipData
	{
		SynchronizedClipData() = default;
		SynchronizedClipData(uint16_t a_originalSynchronizedIndex, uint16_t a_originalInternalClipIndex, RE::BGSSynchronizedAnimationInstance::ActorSyncInfo const* a_syncInfo, AnimationReplacements* a_replacements, ReplacementAnimation* a_replacementAnimation, Variant* a_variant) :
			originalSynchronizedIndex(a_originalSynchronizedIndex), originalInternalClipIndex(a_originalInternalClipIndex), syncInfo(a_syncInfo), replacements(a_replacements), replacementAnimation(a_replacementAnimation), variant(a_variant)
		{}
			
		[[nodiscard]] constexpr bool Matches(const RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator) const { return syncInfo->synchronizedClipGenerator == a_synchronizedClipGenerator; }

		const uint16_t originalSynchronizedIndex;
		const uint16_t originalInternalClipIndex;
		RE::BGSSynchronizedAnimationInstance::ActorSyncInfo const* syncInfo = nullptr;
		AnimationReplacements* replacements = nullptr;
		ReplacementAnimation* replacementAnimation = nullptr;
		Variant* variant = nullptr;
	};

	void Initialize();
	void ToggleReplacement(bool a_bEnable);

	[[nodiscard]] std::shared_ptr<SynchronizedClipData> GetSynchronizedClipData(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator) const;
	[[nodiscard]] std::shared_ptr<SynchronizedClipData> GetOtherSynchronizedClipData(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, bool a_bFromActiveGraph) const;

	[[nodiscard]] bool CheckRequiresActiveGraphTracking() const;
	[[nodiscard]] RE::BGSSynchronizedAnimationInstance::ActorSyncInfo const* GetUniqueSyncInfo() const;

	bool _bInitialized = false;

	RE::BGSSynchronizedAnimationInstance* _synchronizedAnimationInstance;

	mutable SharedLock _clipDataLock;
	std::unordered_map<RE::BSSynchronizedClipGenerator*, std::shared_ptr<SynchronizedClipData>> _clipData;

	RE::BSSynchronizedClipGenerator* _notifyOnDeactivate = nullptr;

	RE::BSSynchronizedClipGenerator* _trackedClipGenerator = nullptr;
	bool _bIsReplacementActive = false;
	bool _bIsAtEnd = false;
	bool _bTransitioning = false;

	std::optional<float> _variantRandomWeight = std::nullopt;

	ReplacementTrace _trace;
};

// a special case where some mod uses SendAnimationEvent to trigger a synchronized clip. This doesn't create a BGSSynchronizedAnimationInstance at all.
// we still need to run some code here
class ActiveScenelessSynchronizedClip
{
public:
	[[nodiscard]] ActiveScenelessSynchronizedClip(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator);
	~ActiveScenelessSynchronizedClip() = default;

	void OnSynchronizedClipActivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context);
	void OnSynchronizedClipDeactivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context);

protected:
	RE::BSSynchronizedClipGenerator* _parentSynchronizedClipGenerator = nullptr;
	std::optional<uint16_t> _originalSynchronizedIndex;
};
