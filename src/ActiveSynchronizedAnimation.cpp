#include "ActiveSynchronizedAnimation.h"

#include "OpenAnimationReplacer.h"

#include <ranges>

ActiveSynchronizedAnimation::ActiveSynchronizedAnimation(RE::BGSSynchronizedAnimationInstance* a_synchronizedAnimationInstance) :
	_synchronizedAnimationInstance(a_synchronizedAnimationInstance)
{}

ActiveSynchronizedAnimation::~ActiveSynchronizedAnimation()
{
	ReadLocker locker(_clipDataLock);

	for (auto& [synchronizedClipGenerator, replacementInfo] : _clipData) {
		synchronizedClipGenerator->animationBindingIndex = replacementInfo->originalSynchronizedIndex;
		if (synchronizedClipGenerator->clipGenerator) {
			synchronizedClipGenerator->clipGenerator->animationBindingIndex = replacementInfo->originalInternalClipIndex;
		}
	}
}

void ActiveSynchronizedAnimation::OnSynchronizedClipPreActivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context)
{
	if (_bTransitioning) {
		return;
	}

	if (!_bInitialized) {
		Initialize();
	}

	bool bAdded;
	const auto activeClip = OpenAnimationReplacer::GetSingleton().AddOrGetActiveClip(a_synchronizedClipGenerator->clipGenerator, a_context, bAdded);
	activeClip->SetSynchronizedParent(a_synchronizedClipGenerator);

	if (const auto synchronizedClipData = GetSynchronizedClipData(a_synchronizedClipGenerator)) {
		activeClip->OnActivateSynchronized(a_synchronizedClipGenerator, synchronizedClipData->replacements, _bIsReplacementActive ? synchronizedClipData->replacementAnimation : nullptr, _variant);

		auto& animationLog = AnimationLog::GetSingleton();
		const auto event = (activeClip->IsOriginal() || !_bIsReplacementActive) ? AnimationLogEntry::Event::kActivateSynchronized : AnimationLogEntry::Event::kActivateReplaceSynchronized;
		if (bAdded && animationLog.ShouldLogAnimations() && !activeClip->IsTransitioning() && animationLog.ShouldLogAnimationsForActiveClip(activeClip, event)) {
			animationLog.LogAnimation(event, activeClip, a_context.character);
		}
	}
}

void ActiveSynchronizedAnimation::OnSynchronizedClipPostActivate([[maybe_unused]] RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, [[maybe_unused]] const RE::hkbContext& a_context)
{
	// check if the active replacement info requires removing non-annotation triggers, if so, remove them from this one
	// solves the issue with vanilla triggers from first person animation playing while we have a third person animation replacement etc
	if (const auto actor = Utils::GetActorFromHkbCharacter(a_context.character)) {
		const auto actorHandle = actor->GetHandle();
		if (_bIsReplacementActive && IsFromInactiveGraph(a_synchronizedClipGenerator)) {
			if (const auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_synchronizedClipGenerator->clipGenerator)) {
				if (!activeClip->HasRemovedNonAnnotationTriggers()) {
					activeClip->RemoveNonAnnotationTriggers(a_synchronizedClipGenerator->clipGenerator);
					if (const auto other = GetOtherSynchronizedClipData(a_synchronizedClipGenerator, true)) {
						_notifyOnDeactivate = other->syncInfo->synchronizedClipGenerator;
					}
				}
			}
		}
	}
}

void ActiveSynchronizedAnimation::OnSynchronizedClipPreUpdate([[maybe_unused]] RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, [[maybe_unused]] const RE::hkbContext& a_context, [[maybe_unused]] float a_timestep)
{
	if (_bIsAtEnd) {
		return;
	}

	if (_trackedClipGenerator && IsFromActiveGraph(_trackedClipGenerator)) {
		const bool bShouldSynchronizedClipReplacementBeActive = ShouldSynchronizedClipReplacementBeActive();
		if (_bIsReplacementActive != bShouldSynchronizedClipReplacementBeActive) {
			ToggleReplacement(bShouldSynchronizedClipReplacementBeActive);
		}
	}

	if (const auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_synchronizedClipGenerator->clipGenerator)) {
		if (activeClip->HasQueuedReplacement()) {
			activeClip->ReplaceActiveAnimation(a_synchronizedClipGenerator->clipGenerator, a_context);
		}
	}
}

void ActiveSynchronizedAnimation::OnSynchronizedClipPostUpdate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, [[maybe_unused]] const RE::hkbContext& a_context, [[maybe_unused]] float a_timestep)
{
	if (_trackedClipGenerator) {
		// If the clip from the active graph has just finished, this means that we've reached the end of the synchronized scene
		if (IsFromActiveGraph(a_synchronizedClipGenerator)) {
			if (a_synchronizedClipGenerator->clipGenerator->atEnd) {
				_bIsAtEnd = true;
			}
		}
	}
}

void ActiveSynchronizedAnimation::OnSynchronizedClipDeactivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, [[maybe_unused]] const RE::hkbContext& a_context)
{
	if (_bTransitioning) {
		if (a_synchronizedClipGenerator->animationBindingIndex == static_cast<uint16_t>(-1) && a_synchronizedClipGenerator->localSyncBinding) {
			a_synchronizedClipGenerator->localSyncBinding->RemoveReference();
			a_synchronizedClipGenerator->localSyncBinding = nullptr;
		}

		a_synchronizedClipGenerator->synchronizedScene->numActivated--;

		a_synchronizedClipGenerator->currentLerp = 0.f;
		a_synchronizedClipGenerator->atMark = false;
		a_synchronizedClipGenerator->allCharactersInScene = false;
		a_synchronizedClipGenerator->allCharactersAtMarks = false;

		return;
	}

	{
		WriteLocker locker(_clipDataLock);

		auto it = _clipData.find(a_synchronizedClipGenerator);
		if (it != _clipData.end()) {
			a_synchronizedClipGenerator->animationBindingIndex = it->second->originalSynchronizedIndex;
			a_synchronizedClipGenerator->clipGenerator->animationBindingIndex = it->second->originalInternalClipIndex;

			_clipData.erase(it);
		}
	}

	if (_notifyOnDeactivate == a_synchronizedClipGenerator) {
		const auto actor = Utils::GetActorFromHkbCharacter(a_context.character);
		if (!actor->NotifyAnimationGraph("PairEnd"sv)) {
			actor->NotifyAnimationGraph("PairedStop"sv);
		}
	}
}

bool ActiveSynchronizedAnimation::ShouldSynchronizedClipReplacementBeActive() const
{
	if (_trackedClipGenerator) {
		// get the other sync data from active graph (player first/third person) and check if it has a replacement
		if (const auto otherSyncData = GetOtherSynchronizedClipData(_trackedClipGenerator, true)) {
			return otherSyncData->replacementAnimation != nullptr;
		}
	}

	return true;
}

bool ActiveSynchronizedAnimation::IsFromActiveGraph(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator) const
{
	// search sync infos and compare with active graph
	for (const auto& actorSyncInfo : _synchronizedAnimationInstance->actorSyncInfos) {
		if (actorSyncInfo.synchronizedClipGenerator == a_synchronizedClipGenerator) {
			RE::BSAnimationGraphManagerPtr graphManager = nullptr;
			if (actorSyncInfo.refHandle.get()->GetAnimationGraphManager(graphManager)) {
				const auto activeGraph = graphManager->graphs[graphManager->GetRuntimeData().activeGraph];
				if (actorSyncInfo.character == &activeGraph->characterInstance) {
					return true;
				}
			}
		}
	}

	return false;
}

bool ActiveSynchronizedAnimation::IsFromInactiveGraph(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator) const
{
	// search sync infos and compare with active graph
	for (const auto& actorSyncInfo : _synchronizedAnimationInstance->actorSyncInfos) {
		if (actorSyncInfo.synchronizedClipGenerator == a_synchronizedClipGenerator) {
			RE::BSAnimationGraphManagerPtr graphManager = nullptr;
			if (actorSyncInfo.refHandle.get()->GetAnimationGraphManager(graphManager)) {
				const auto activeGraph = graphManager->graphs[graphManager->GetRuntimeData().activeGraph];
				if (actorSyncInfo.character != &activeGraph->characterInstance) {
					return true;
				}
			}
		}
	}

	return false;
}

bool ActiveSynchronizedAnimation::HasRef(RE::TESObjectREFR* a_refr) const
{
	return _synchronizedAnimationInstance->HasRef(a_refr->GetHandle());
}

void ActiveSynchronizedAnimation::Initialize()
{
	if (_bInitialized) {
		return;
	}

	const auto sourceRefHandle = _synchronizedAnimationInstance->refHandles[0];  // source is always first
	const auto targetRefHandle = _synchronizedAnimationInstance->refHandles[1];

	{
		WriteLocker locker(_clipDataLock);
		for (const auto& actorSyncInfo : _synchronizedAnimationInstance->actorSyncInfos) {
			const uint16_t originalSynchronizedIndex = actorSyncInfo.synchronizedClipGenerator->animationBindingIndex;
			const uint16_t originalInternalClipIndex = actorSyncInfo.synchronizedClipGenerator->clipGenerator->animationBindingIndex;

			// fix ID if we aren't running Paired Animation Improvements
			if (actorSyncInfo.synchronizedClipGenerator->animationBindingIndex != static_cast<uint16_t>(-1)) {
				actorSyncInfo.synchronizedClipGenerator->animationBindingIndex += OpenAnimationReplacer::GetSingleton().GetSynchronizedClipsIDOffset(actorSyncInfo.character);
			}

			ReplacementAnimation* replacementAnimation = nullptr;
			AnimationReplacements* replacements = OpenAnimationReplacer::GetSingleton().GetReplacements(actorSyncInfo.character, actorSyncInfo.synchronizedClipGenerator->clipGenerator->animationBindingIndex);

			if (replacements) {
				replacementAnimation = replacements->EvaluateSynchronizedConditionsAndGetReplacementAnimation(sourceRefHandle.get().get(), targetRefHandle.get().get(), actorSyncInfo.synchronizedClipGenerator->clipGenerator);
				if (replacementAnimation) {
					// handle variants
					if (!_variant && replacementAnimation->HasVariants()) {
						replacementAnimation->GetIndex(_variant);
					}
				}
			}

			_clipData.emplace(actorSyncInfo.synchronizedClipGenerator, std::make_shared<SynchronizedClipData>(originalSynchronizedIndex, originalInternalClipIndex, &actorSyncInfo, replacements, replacementAnimation));
		}
	}

	// check if we need special handling
	if (CheckRequiresActiveGraphTracking()) {
		if (const auto syncInfo = GetUniqueSyncInfo()) {  // get sync info of the non-player character
			_trackedClipGenerator = syncInfo->synchronizedClipGenerator;
			_bIsReplacementActive = ShouldSynchronizedClipReplacementBeActive();
		}
	}

	_bInitialized = true;
}

void ActiveSynchronizedAnimation::ToggleReplacement(bool a_bEnable)
{
	if (_bIsReplacementActive != a_bEnable) {
		ReadLocker locker(_clipDataLock);
		for (const auto& clipData : _clipData | std::views::values) {
			if (const auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(clipData->syncInfo->synchronizedClipGenerator->clipGenerator)) {
				ReplacementAnimation* replacementAnimation = a_bEnable ? clipData->replacementAnimation : nullptr;
				activeClip->QueueReplacementAnimation(replacementAnimation, 0.f, ActiveClip::QueuedReplacement::Type::kContinue, AnimationLogEntry::Event::kPairedMismatch);
			}
		}

		_notifyOnDeactivate = nullptr;
		_bIsReplacementActive = a_bEnable;
	}
}

std::shared_ptr<ActiveSynchronizedAnimation::SynchronizedClipData> ActiveSynchronizedAnimation::GetSynchronizedClipData(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator) const
{
	ReadLocker locker(_clipDataLock);

	if (const auto search = _clipData.find(a_synchronizedClipGenerator); search != _clipData.end()) {
		return search->second;
	}

	return nullptr;
}

std::shared_ptr<ActiveSynchronizedAnimation::SynchronizedClipData> ActiveSynchronizedAnimation::GetOtherSynchronizedClipData(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, bool a_bFromActiveGraph) const
{
	ReadLocker locker(_clipDataLock);

	for (const auto& [clipGenerator, clipData] : _clipData) {
		if (clipGenerator != a_synchronizedClipGenerator) {
			if (a_bFromActiveGraph ? IsFromActiveGraph(clipGenerator) : IsFromInactiveGraph(clipGenerator)) {
				return clipData;
			}
		}
	}

	return nullptr;
}

bool ActiveSynchronizedAnimation::CheckRequiresActiveGraphTracking() const
{
	if (_synchronizedAnimationInstance->actorSyncInfos.size() > 2) {
		// do all have replacements, or none

		// to simplify this mess, gonna assume that if all clips have replacements, they also match.
		// I can't realistically imagine a situation where there's a replacer that only replaces the first person animation,
		// as the killmove victim's third person animation is a paired animation, not a "normal" one - and therefore has to include both characters.

		bool bNoneHaveReplacements = true;
		bool bAllHaveReplacements = true;

		ReadLocker locker(_clipDataLock);
		for (const auto& clipData : _clipData | std::views::values) {
			if (clipData->replacementAnimation) {
				bNoneHaveReplacements = false;
			} else {
				bAllHaveReplacements = false;
			}
		}

		if (!bNoneHaveReplacements && !bAllHaveReplacements) {
			return true;
		}
	}

	return false;
}

RE::BGSSynchronizedAnimationInstance::ActorSyncInfo const* ActiveSynchronizedAnimation::GetUniqueSyncInfo() const
{
	// this returns the sync info of the actor that only has one clip data in this synchronized animation instance.
	// in practice this means it's going to be the victim of a killmove - as the player actor will have both first person and third person clips

	ReadLocker locker(_clipDataLock);

	std::unordered_map<RE::ActorHandle, std::vector<std::shared_ptr<SynchronizedClipData>>> clipDatasForActor;
	for (const auto& clipData : _clipData | std::views::values) {
		auto& vector = clipDatasForActor[clipData->syncInfo->refHandle];
		vector.emplace_back(clipData);
	}

	for (const auto& vec : clipDatasForActor | std::views::values) {
		if (vec.size() == 1) {
			return vec[0]->syncInfo;
		}
	}

	return nullptr;
}

ActiveScenelessSynchronizedClip::ActiveScenelessSynchronizedClip(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator) :
	_parentSynchronizedClipGenerator(a_synchronizedClipGenerator)
{}

void ActiveScenelessSynchronizedClip::OnSynchronizedClipActivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context)
{
	// fix ID if we aren't running Paired Animation Improvements
	if (a_synchronizedClipGenerator->animationBindingIndex != static_cast<uint16_t>(-1)) {
		_originalSynchronizedIndex = a_synchronizedClipGenerator->animationBindingIndex;
		a_synchronizedClipGenerator->animationBindingIndex += OpenAnimationReplacer::GetSingleton().GetSynchronizedClipsIDOffset(a_context.character);
	}

	bool bAdded;
	const auto activeClip = OpenAnimationReplacer::GetSingleton().AddOrGetActiveClip(a_synchronizedClipGenerator->clipGenerator, a_context, bAdded);
	activeClip->SetSynchronizedParent(a_synchronizedClipGenerator);
}

void ActiveScenelessSynchronizedClip::OnSynchronizedClipDeactivate(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, [[maybe_unused]] const RE::hkbContext& a_context)
{
	if (_originalSynchronizedIndex.has_value()) {
		a_synchronizedClipGenerator->animationBindingIndex = *_originalSynchronizedIndex;
	}
}
