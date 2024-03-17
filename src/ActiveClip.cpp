#include "ActiveClip.h"

#include "Offsets.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"

bool ActiveClip::BlendingClip::Update(const RE::hkbContext& a_context, float a_deltaTime)
{
	blendElapsedTime += a_deltaTime;

	if (blendElapsedTime >= blendDuration) {
		return false;
	}

	clipGenerator.Update(a_context, a_deltaTime);

	return true;
}

ActiveClip::ActiveClip(RE::hkbClipGenerator* a_clipGenerator, RE::hkbCharacter* a_character, RE::hkbBehaviorGraph* a_behaviorGraph) :
	_clipGenerator(a_clipGenerator),
	_character(a_character),
	_behaviorGraph(a_behaviorGraph),
	_originalIndex(a_clipGenerator->animationBindingIndex),
	_originalMode(*a_clipGenerator->mode),
	_originalFlags(a_clipGenerator->flags),
	_bOriginalInterruptible(OpenAnimationReplacer::GetSingleton().IsOriginalAnimationInterruptible(a_character, a_clipGenerator->animationBindingIndex)),
	_bOriginalReplaceOnEcho(OpenAnimationReplacer::GetSingleton().ShouldOriginalAnimationReplaceOnEcho(a_character, a_clipGenerator->animationBindingIndex)),
	_bOriginalKeepRandomResultsOnLoop(OpenAnimationReplacer::GetSingleton().ShouldOriginalAnimationKeepRandomResultsOnLoop(a_character, a_clipGenerator->animationBindingIndex))
{
	_refr = Utils::GetActorFromHkbCharacter(a_character);
}

ActiveClip::~ActiveClip()
{
	{
		Locker locker(_callbacksLock);

		for (const auto& callbackWeakPtr : _destroyedCallbacks) {
			if (auto callback = callbackWeakPtr.lock()) {
				(*callback)(this);
			}
		}
	}

	RestoreOriginalAnimation();
}

bool ActiveClip::ShouldReplaceAnimation(const ReplacementAnimation* a_newReplacementAnimation, bool a_bTryVariant, std::optional<uint16_t>& a_outVariantIndex)
{
	// if different
	if (_currentReplacementAnimation != a_newReplacementAnimation) {
		return true;
	}

	if (a_bTryVariant) {
		// if same but not null
		if (_currentReplacementAnimation && a_newReplacementAnimation) {
			// roll for new variant
			const uint16_t newIndex = _currentReplacementAnimation->GetIndex(this);
			if (_currentReplacementAnimation->HasVariants() && newIndex != GetCurrentIndex()) {
				a_outVariantIndex = newIndex;
				return true;
			}
		}
	}

	return false;
}

void ActiveClip::ReplaceAnimation(ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex /* = std::nullopt*/)
{
	auto previousReplacementAnimation = _currentReplacementAnimation;

	RestoreOriginalAnimation();

	_currentReplacementAnimation = a_replacementAnimation;

	if (_currentReplacementAnimation) {
		// alter variables for the lifetime of this object to the ones read from the replacement animation
		const uint16_t newBindingIndex = a_variantIndex ? *a_variantIndex : _currentReplacementAnimation->GetIndex(this);
		_clipGenerator->animationBindingIndex = newBindingIndex;  // this is the most important part - this is what actually replaces the animation
																  // the animation binding index is the index of an entry inside hkbCharacterStringData->animationNames, which contains the actual path to the animation file or one of the replacements
		if (_currentReplacementAnimation->GetIgnoreDontConvertAnnotationsToTriggersFlag()) {
			_clipGenerator->flags &= ~0x10;
		}
	}

	OnReplacedAnimation(previousReplacementAnimation, _currentReplacementAnimation);
}

void ActiveClip::RestoreOriginalAnimation()
{
	_clipGenerator->animationBindingIndex = _originalIndex;
	_clipGenerator->mode = _originalMode;
	_clipGenerator->flags = _originalFlags;
	RestoreBackupTriggers();
	if (_currentReplacementAnimation) {
		_currentReplacementAnimation = nullptr;
	}
}

void ActiveClip::QueueReplacementAnimation(ReplacementAnimation* a_replacementAnimation, float a_blendTime, QueuedReplacement::Type a_type, AnimationLogEntry::Event a_replacementEvent, std::optional<uint16_t> a_variantIndex /* = std::nullopt*/)
{
	_queuedReplacement = { a_replacementAnimation, a_blendTime, a_type, a_replacementEvent, a_variantIndex };
}

ReplacementAnimation* ActiveClip::PopQueuedReplacementAnimation()
{
	if (!HasQueuedReplacement()) {
		return nullptr;
	}

	const auto replacement = _queuedReplacement->replacementAnimation;

	_queuedReplacement = std::nullopt;

	return replacement;
}

void ActiveClip::ReplaceActiveAnimation(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context)
{
	const float blendTime = _queuedReplacement->blendTime;
	const auto replacementEvent = _queuedReplacement->replacementEvent;
	const auto type = _queuedReplacement->type;

	float startTime = 0.f;
	if (type == QueuedReplacement::Type::kContinue && a_clipGenerator->animationControl) {
		startTime = a_clipGenerator->animationControl->localTime;
	}

	if (blendTime > 0.f) {
		StartBlend(a_clipGenerator, a_context, blendTime);

		// set to null before deactivation so it isn't destroyed when hkbClipGenerator::Deactivate is called (we continue using this animation control object in the fake clip generator)
		a_clipGenerator->animationControl = nullptr;
	}

	SetTransitioning(true);
	if (_parentSynchronizedClipGenerator) {
		_parentSynchronizedClipGenerator->Deactivate(a_context);
	} else {
		a_clipGenerator->Deactivate(a_context);
	}

	const std::optional<uint16_t> newIndex = _queuedReplacement ? _queuedReplacement->variantIndex : std::nullopt;
	const auto replacement = PopQueuedReplacementAnimation();
	ReplaceAnimation(replacement, newIndex);

	if (_parentSynchronizedClipGenerator) {
		BackupTriggers(a_clipGenerator);
	}

	auto& animationLog = AnimationLog::GetSingleton();

	if (animationLog.ShouldLogAnimations() && animationLog.ShouldLogAnimationsForActiveClip(this, replacementEvent)) {
		animationLog.LogAnimation(replacementEvent, this, a_context.character);
	}

	if (_parentSynchronizedClipGenerator) {
		_parentSynchronizedClipGenerator->Activate(a_context);
	} else {
		a_clipGenerator->Activate(a_context);
	}
	SetTransitioning(false);

	if (type == QueuedReplacement::Type::kLoop && blendTime > 0.f) {
		// set start time to end of animation minus blend time, so we end blending right at the beginning of the anim after it loops
		if (a_clipGenerator->binding && a_clipGenerator->binding->animation) {
			startTime = std::max(a_clipGenerator->binding->animation->duration - blendTime, 0.f);
		}
	}

	if (startTime != 0.f) {
		a_clipGenerator->animationControl->localTime = startTime;
	}
}

void ActiveClip::OnReplacedAnimation(ReplacementAnimation* a_previousReplacementAnimation, ReplacementAnimation* a_newReplacementAnimation)
{
	// handle sequential variant data
	if (a_previousReplacementAnimation && a_previousReplacementAnimation != a_newReplacementAnimation) {
		if (a_previousReplacementAnimation->HasVariants()) {
			const auto& variants = a_previousReplacementAnimation->GetVariants();
			if (variants.GetVariantMode() == ReplacementAnimation::VariantMode::kSequential) {
				ReadLocker locker(_sequentialVariantLock);
				const auto search = _sequentialVariantData.find(a_previousReplacementAnimation);
				if (search != _sequentialVariantData.end()) {
					search->second.SetActive(false);
				}
			}
		}
	}

	if (a_newReplacementAnimation && a_newReplacementAnimation->HasVariants()) {
		const auto& variants = a_newReplacementAnimation->GetVariants();
		if (variants.GetVariantMode() == ReplacementAnimation::VariantMode::kSequential) {
			WriteLocker locker(_sequentialVariantLock);

			auto& sequentialVariantData = _sequentialVariantData[a_newReplacementAnimation];
			sequentialVariantData.SetActive(true);

			auto& currentVariantIndex = sequentialVariantData.nextSequentialVariant;
			std::vector<bool>& playOnceHistory = sequentialVariantData.playOnceVariantHistory;

			const auto activeVariantCount = variants.GetActiveVariantCount();
			if (playOnceHistory.empty()) {
				// initialize the vector
				playOnceHistory.resize(activeVariantCount, false);
			}

			const auto currentVariant = variants.GetActiveVariant(currentVariantIndex);
			if (!currentVariant) {
				// failsafe, just reset everything
				currentVariantIndex = 0;
				playOnceHistory.clear();
				return;
			}

			if (currentVariant->ShouldPlayOnce()) {
				playOnceHistory[currentVariantIndex] = true;
			}

			// iterate the index
			++currentVariantIndex;

			for (size_t i = 0; i < activeVariantCount; ++i) {
				currentVariantIndex = (currentVariantIndex + i) % activeVariantCount;  // wrap around if necessary

				if (!variants.GetActiveVariant(currentVariantIndex)->ShouldPlayOnce()) {
					return;
				}

				if (!playOnceHistory[currentVariantIndex]) {
					return;
				}
			}

			// if we're here, all active variants have been played and we should clear the history
			playOnceHistory.clear();
		}
	}
}

void ActiveClip::SetTransitioning(bool a_bValue)
{
	_bTransitioning = a_bValue;
	if (_parentSynchronizedClipGenerator) {
		if (const auto synchronizedAnim = OpenAnimationReplacer::GetSingleton().GetActiveSynchronizedAnimation(_parentSynchronizedClipGenerator->synchronizedScene)) {
			synchronizedAnim->SetTransitioning(a_bValue);
		}
	}
}

void ActiveClip::StartBlend(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, float a_blendTime)
{
	const auto& newBlendingClip = _blendingClipGenerators.emplace_back(std::make_unique<BlendingClip>(a_clipGenerator, a_blendTime));

	_lastGameTime = OpenAnimationReplacer::gameTimeCounter;

	newBlendingClip->clipGenerator.Activate(a_context);
}

void ActiveClip::PreUpdate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, [[maybe_unused]] float a_timestep)
{
	// remove expired sequential variant data
	{
		WriteLocker locker(_sequentialVariantLock);

		for (auto it = _sequentialVariantData.begin(); it != _sequentialVariantData.end();) {
			auto& sequentialVariantData = it->second;
			if (!sequentialVariantData.Update(a_timestep)) {
				it = _sequentialVariantData.erase(it);
			} else {
				++it;
			}
		}
	}

	if (IsSynchronizedClip()) {
		return;  // Analogous function already ran by ActiveSynchronizedAnimation
	}

	if (!HasQueuedReplacement()) {
		// check if the animation should be interrupted (queue a replacement if so)
		if (IsInterruptible()) {
			const auto newReplacementAnim = OpenAnimationReplacer::GetSingleton().GetReplacementAnimation(a_context.character, a_clipGenerator, _originalIndex);
			// do not try to replace with other variants here
			std::optional<uint16_t> dummy = std::nullopt;
			if (ShouldReplaceAnimation(newReplacementAnim, false, dummy)) {
				const float blendTime = HasReplacementAnimation() ? GetReplacementAnimation()->GetCustomBlendTime(CustomBlendType::kInterrupt) : Settings::fDefaultBlendTimeOnInterrupt;
				QueueReplacementAnimation(newReplacementAnim, blendTime, QueuedReplacement::Type::kRestart, AnimationLogEntry::Event::kInterrupt);
			}
		}
	}

	if (!HasQueuedReplacement()) {
		// check if the animation is going to loop in this update
		if (a_clipGenerator->mode == RE::hkbClipGenerator::PlaybackMode::kModeLooping) {
			// calculate numLoops in this update, with added blend time so we replace just before it loops
			float prevLocalTime = 0.f;
			float newLocalTime = 0.f;
			int32_t numLoops = 0;
			bool newAtEnd = false;
			float blendTime = HasReplacementAnimation() ? GetReplacementAnimation()->GetCustomBlendTime(CustomBlendType::kLoop) : Settings::fDefaultBlendTimeOnLoop;
			if (hkbClipGenerator_GetAnimDuration(a_clipGenerator) <= blendTime) {
				blendTime = 0.f;
			}

			bool bTryReplaceOnLoop = false;
			hkbClipGenerator_ComputeBeginAndEndLocalTime(a_clipGenerator, a_timestep + blendTime, &prevLocalTime, &newLocalTime, &numLoops, &newAtEnd);
			if (numLoops > 0) {
				if (!_bIsAtEndOfLoop) {
					bTryReplaceOnLoop = true;
					_bIsAtEndOfLoop = true;
				}
			} else {
				_bIsAtEndOfLoop = false;
			}

			bool bShouldTryLog = true;
			if (bTryReplaceOnLoop) {
				// recheck conditions to see if we should replace the animation
				if (OnLoop(a_clipGenerator)) {
					bShouldTryLog = false;
				}
			}

			if (bShouldTryLog) {
				// if we're not replacing on loop, only log if we're actually looping in this update (the previous check takes into account blend time)
				hkbClipGenerator_ComputeBeginAndEndLocalTime(a_clipGenerator, a_timestep, &prevLocalTime, &newLocalTime, &numLoops, &newAtEnd);  // same but without added blendTime
				if (numLoops > 0) {
					auto& animationLog = AnimationLog::GetSingleton();
					constexpr auto event = AnimationLogEntry::Event::kLoop;
					if (animationLog.ShouldLogAnimations() && animationLog.ShouldLogAnimationsForActiveClip(this, event)) {
						animationLog.LogAnimation(event, this, GetCharacter());
					}
				}
			}
		}
	}

	// replace anim if queued
	if (HasQueuedReplacement()) {
		ReplaceActiveAnimation(a_clipGenerator, a_context);
	}

	// update blending clips and remove those expired
	if (IsBlending()) {
		const float currentGameTime = OpenAnimationReplacer::gameTimeCounter;
		const float deltaTime = currentGameTime - _lastGameTime;
		_lastGameTime = currentGameTime;

		for (auto it = _blendingClipGenerators.begin(); it != _blendingClipGenerators.end();) {
			auto& blendingClip = *it;
			if (!blendingClip->Update(a_context, deltaTime)) {
				it = _blendingClipGenerators.erase(it);
			} else {
				++it;
			}
		}
	}
}

void ActiveClip::OnActivate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context)
{
	if (!IsTransitioning() && !IsSynchronizedClip()) {
		// don't try to replace animation while transitioning interruptible anims as we already replaced it, this should only run on the actual Activate called by the game
		// also don't run this for synchronized clips as they are handled by OnActivateSynchronized
		if (const auto replacements = OpenAnimationReplacer::GetSingleton().GetReplacements(a_context.character, a_clipGenerator->animationBindingIndex)) {
			SetReplacements(replacements);
			const RE::BShkbAnimationGraph* animGraph = SKSE::stl::adjust_pointer<RE::BShkbAnimationGraph>(a_context.character, -0xC0);
			RE::TESObjectREFR* refr = animGraph->holder;
			// try to get refr from root node if holder is null - e.g. animated weapons
			if (!refr) {
				refr = Utils::GetRefrFromObject(animGraph->rootNode);
			}
			if (const auto replacementAnimation = replacements->EvaluateConditionsAndGetReplacementAnimation(refr, a_clipGenerator)) {
				ReplaceAnimation(replacementAnimation);

				if (_currentReplacementAnimation && _currentReplacementAnimation->GetTriggersFromAnnotationsOnly()) {
					BackupTriggers(a_clipGenerator);
				}
			}
		}
	}
}

void ActiveClip::OnPostActivate(RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] const RE::hkbContext& a_context)
{
	if (_currentReplacementAnimation) {
		// handle "triggers from annotations only" setting
		if (_currentReplacementAnimation->GetTriggersFromAnnotationsOnly()) {
			RemoveNonAnnotationTriggers(a_clipGenerator);
		}
	}
}

void ActiveClip::BackupTriggers(RE::hkbClipGenerator* a_clipGenerator)
{
	_originalTriggersBackup = a_clipGenerator->originalTriggers;
	_triggersBackup = a_clipGenerator->triggers;
	_bTriggersBackedUp = true;
}

void ActiveClip::RestoreBackupTriggers()
{
	if (_bTriggersBackedUp) {
		_clipGenerator->triggers = _triggersBackup;
		_triggersBackup = nullptr;
		_clipGenerator->originalTriggers = _originalTriggersBackup;
		_originalTriggersBackup = nullptr;
		_bTriggersBackedUp = false;
	}
}

void ActiveClip::RemoveNonAnnotationTriggers(RE::hkbClipGenerator* a_clipGenerator)
{
	if (!_bRemovedNonAnnotationTriggers) {
		// remove non-annotation triggers from both arrays (originalTriggers and triggers)
		if (a_clipGenerator->originalTriggers && !a_clipGenerator->originalTriggers->triggers.empty()) {
			RemoveNonAnnotationTriggersFromClipTriggerArray(a_clipGenerator->originalTriggers);
		}

		if (a_clipGenerator->triggers && !a_clipGenerator->triggers->triggers.empty()) {
			RemoveNonAnnotationTriggersFromClipTriggerArray(a_clipGenerator->triggers);
		}
	}
}

void ActiveClip::OnActivateSynchronized(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, AnimationReplacements* a_replacements, ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex)
{
	SetReplacements(a_replacements);
	ReplaceAnimation(a_replacementAnimation, a_variantIndex);

	BackupTriggers(a_synchronizedClipGenerator->clipGenerator);
}

void ActiveClip::OnGenerate([[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] const RE::hkbContext& a_context, RE::hkbGeneratorOutput& a_output)
{
	// manually blend the output pose with the previous animation's output pose
	if (!a_clipGenerator) {
		return;
	}

	if (!IsBlending()) {
		return;
	}

	if (a_output.IsValid(RE::hkbGeneratorOutput::StandardTracks::TRACK_POSE)) {
		RE::hkbGeneratorOutput::Track poseTrack = a_output.GetTrack(RE::hkbGeneratorOutput::StandardTracks::TRACK_POSE);

		auto poseOut = poseTrack.GetDataQsTransform();

		std::vector<RE::hkQsTransform> blendedTracks{};
		if (GetBlendedTracks(blendedTracks)) {
			float lerpAmount = std::clamp(Utils::InterpEaseInOut(0.f, 1.f, GetBlendWeight(), 2), 0.f, 1.f);
			auto numBlend = std::min(static_cast<size_t>(poseTrack.GetNumData()), blendedTracks.size());
			hkbBlendPoses(numBlend, poseOut, blendedTracks.data(), lerpAmount, poseOut);
		}
	}
}

bool ActiveClip::OnEcho(RE::hkbClipGenerator* a_clipGenerator, float a_echoDuration)
{
	// clear random condition results so they reroll and another animation replacements get a chance to play
	if (!ShouldKeepRandomResultsOnLoop()) {
		ClearRandomFloats();
	}

	if (ShouldReplaceOnEcho()) {
		const auto newReplacementAnim = OpenAnimationReplacer::GetSingleton().GetReplacementAnimation(_character, a_clipGenerator, _originalIndex);
		std::optional<uint16_t> variantIndex = std::nullopt;
		if (ShouldReplaceAnimation(newReplacementAnim, !ShouldKeepRandomResultsOnLoop(), variantIndex)) {
			const float blendTime = HasReplacementAnimation() ? GetReplacementAnimation()->GetCustomBlendTime(CustomBlendType::kEcho) : a_echoDuration;
			QueueReplacementAnimation(newReplacementAnim, blendTime, QueuedReplacement::Type::kRestart, AnimationLogEntry::Event::kEchoReplace, variantIndex);
			return true;
		}
	}

	return false;
}

bool ActiveClip::OnLoop(RE::hkbClipGenerator* a_clipGenerator)
{
	// special logic for sequential variants
	if (_currentReplacementAnimation && _currentReplacementAnimation->HasVariants()) {
		const auto& variants = _currentReplacementAnimation->GetVariants();
		if (variants.GetVariantMode() == ReplacementAnimation::VariantMode::kSequential && !variants.CanReplaceOnLoopBeforeSequenceFinishes()) {
			// check if the variant sequence has finished
			ReadLocker locker(_sequentialVariantLock);
			const auto search = _sequentialVariantData.find(_currentReplacementAnimation);
			if (search != _sequentialVariantData.end()) {
				const auto& sequentialVariantData = search->second;
				if (!sequentialVariantData.IsAtBeginningOfSequence(_currentReplacementAnimation)) {
					// don't run the entire logic - we don't want to replace this with another animation, just replace with the next variant of the same animation
					const uint16_t newVariantIndex = _currentReplacementAnimation->GetIndex(this);
					const float blendTime = HasReplacementAnimation() ? GetReplacementAnimation()->GetCustomBlendTime(CustomBlendType::kLoop) : Settings::fDefaultBlendTimeOnLoop;
					QueueReplacementAnimation(_currentReplacementAnimation, blendTime, QueuedReplacement::Type::kLoop, AnimationLogEntry::Event::kLoopReplace, newVariantIndex);
					return true;
				}
			}
		}
	}

	// clear random condition results so they reroll and another animation replacements get a chance to play
	if (!ShouldKeepRandomResultsOnLoop()) {
		ClearRandomFloats();
	}

	if (ShouldReplaceOnLoop()) {
		// reevaluate conditions on loop
		const auto newReplacementAnim = OpenAnimationReplacer::GetSingleton().GetReplacementAnimation(_character, a_clipGenerator, _originalIndex);
		std::optional<uint16_t> variantIndex = std::nullopt;
		if (ShouldReplaceAnimation(newReplacementAnim, !ShouldKeepRandomResultsOnLoop(), variantIndex)) {
			const float blendTime = HasReplacementAnimation() ? GetReplacementAnimation()->GetCustomBlendTime(CustomBlendType::kLoop) : Settings::fDefaultBlendTimeOnLoop;
			QueueReplacementAnimation(newReplacementAnim, blendTime, QueuedReplacement::Type::kLoop, AnimationLogEntry::Event::kLoopReplace, variantIndex);
			return true;
		}
	}

	return false;
}

RE::hkbClipGenerator* ActiveClip::GetLastBlendingClipGenerator() const
{
	if (_blendingClipGenerators.empty()) {
		return nullptr;
	}

	return reinterpret_cast<RE::hkbClipGenerator*>(&_blendingClipGenerators.back()->clipGenerator);
}

float ActiveClip::GetVariantRandom(const ReplacementAnimation* a_replacementAnimation)
{
	if (const auto parentSubMod = a_replacementAnimation->GetParentSubMod()) {
		if (parentSubMod->IsSharingRandomResults()) {
			return parentSubMod->GetVariantRandom(this);
		}
	}

	return Utils::GetRandomFloat(0.f, 1.f);
}

size_t ActiveClip::GetVariantSequential(const ReplacementAnimation* a_replacementAnimation)
{
	ReadLocker locker(_sequentialVariantLock);

	const auto search = _sequentialVariantData.find(a_replacementAnimation);
	if (search != _sequentialVariantData.end()) {
		auto& next = search->second.nextSequentialVariant;
		if (next >= a_replacementAnimation->GetVariants().GetActiveVariantCount()) {
			next = 0;
		}
		return next;
	}

	return 0;
}

float ActiveClip::GetRandomFloat(const Conditions::IRandomConditionComponent* a_randomComponent, SubMod* a_parentSubMod)
{
	// Check if the condition belongs to a submod that shares random results
	SubMod* parentSubMod = a_parentSubMod;
	if (!parentSubMod) {
		if (const auto parentCondition = a_randomComponent->GetParentCondition()) {
			if (const auto parentConditionSet = parentCondition->GetParentConditionSet()) {
				parentSubMod = parentConditionSet->GetParentSubMod();
			}
		}
	}

	if (parentSubMod && parentSubMod->IsSharingRandomResults()) {
		return parentSubMod->GetSharedRandom(this, a_randomComponent);
	}

	// Returns a saved random float if it exists, otherwise generates a new one and saves it
	{
		ReadLocker locker(_randomLock);
		const auto search = _randomFloats.find(a_randomComponent);
		if (search != _randomFloats.end()) {
			return search->second;
		}
	}

	WriteLocker locker(_randomLock);
	float randomFloat = Utils::GetRandomFloat(a_randomComponent->GetMinValue(), a_randomComponent->GetMaxValue());
	_randomFloats.emplace(a_randomComponent, randomFloat);

	return randomFloat;
}

void ActiveClip::ClearRandomFloats()
{
	// Check if the current replacement animation belongs to a submod that shares random results
	if (_currentReplacementAnimation) {
		if (const auto parentSubMod = _currentReplacementAnimation->GetParentSubMod()) {
			if (parentSubMod->IsSharingRandomResults()) {
				parentSubMod->ClearSharedRandom(_behaviorGraph);
			}
		}
	}

	WriteLocker locker(_randomLock);

	_randomFloats.clear();
}

void ActiveClip::RegisterDestroyedCallback(std::weak_ptr<DestroyedCallback>& a_callback)
{
	Locker locker(_callbacksLock);

	// cleanup expired callbacks
	std::erase_if(_destroyedCallbacks, [this](const std::weak_ptr<DestroyedCallback>& a_callbackWeakPtr) {
		return a_callbackWeakPtr.expired();
	});

	_destroyedCallbacks.emplace_back(a_callback);
}

void ActiveClip::RemoveNonAnnotationTriggersFromClipTriggerArray(RE::hkRefPtr<RE::hkbClipTriggerArray>& a_clipTriggerArray)
{
	// create a new array
	auto newClipTriggerArray = static_cast<RE::hkbClipTriggerArray*>(hkHeapAlloc(sizeof(RE::hkbClipTriggerArray)));
	memset(newClipTriggerArray, 0, sizeof(RE::hkbClipTriggerArray));
	newClipTriggerArray->memSizeAndFlags = static_cast<uint16_t>(0xFFFF);
	SKSE::stl::emplace_vtable(newClipTriggerArray);

	// only copy triggers that are annotations
	for (const auto& trigger : a_clipTriggerArray->triggers) {
		if (trigger.isAnnotation) {
			newClipTriggerArray->triggers.push_back(trigger);
		}
	}

	// replace old array with new
	a_clipTriggerArray = RE::hkRefPtr(newClipTriggerArray);

	_bRemovedNonAnnotationTriggers = true;
}

bool ActiveClip::GetBlendedTracks(std::vector<RE::hkQsTransform>& a_outBlendedTracks)
{
	if (_blendingClipGenerators.empty()) {
		return false;
	}

	auto getTracks = [](const auto& a_blendingClip, std::vector<RE::hkQsTransform>& a_sampledTracks) {
		if (const auto binding = a_blendingClip->clipGenerator.animationControl->binding) {
			if (const auto& blendFromAnimation = binding->animation) {
				a_sampledTracks.resize(blendFromAnimation->numberOfTransformTracks);
				std::vector<float> sampledFloatTracks{};
				sampledFloatTracks.resize(blendFromAnimation->numberOfFloatTracks);

				blendFromAnimation->SamplePartialTracks(a_blendingClip->clipGenerator.localTime, blendFromAnimation->numberOfTransformTracks, a_sampledTracks.data(), blendFromAnimation->numberOfFloatTracks, sampledFloatTracks.data(), nullptr);

				return true;
			}
		}

		return false;
	};

	if (_blendingClipGenerators.size() == 1) {
		const auto& blendingClip = _blendingClipGenerators.front();
		return getTracks(blendingClip, a_outBlendedTracks);
	}

	// get tracks from the first blending clip
	auto it = _blendingClipGenerators.begin();
	const auto& firstClip = *it;
	float blendWeight = firstClip->GetBlendWeight();
	if (!getTracks(*it, a_outBlendedTracks)) {
		return false;
	}
	++it;

	// blend with the rest
	while (it != _blendingClipGenerators.end()) {
		const auto& blendingClip = *it;
		std::vector<RE::hkQsTransform> sampledTransformTracks{};
		if (getTracks(blendingClip, sampledTransformTracks)) {
			const float lerpAmount = std::clamp(Utils::InterpEaseInOut(0.f, 1.f, blendWeight, 2), 0.f, 1.f);

			auto numBlend = std::min(a_outBlendedTracks.size(), static_cast<size_t>(blendingClip->clipGenerator.animationControl->binding->animation->numberOfTransformTracks));
			hkbBlendPoses(numBlend, sampledTransformTracks.data(), a_outBlendedTracks.data(), lerpAmount, a_outBlendedTracks.data());

			blendWeight = blendingClip->GetBlendWeight();
		}
		++it;
	}

	return true;
}

float ActiveClip::GetBlendWeight() const
{
	if (_blendingClipGenerators.empty()) {
		return 1.f;
	}

	if (auto& lastClip = _blendingClipGenerators.back()) {
		return lastClip->GetBlendWeight();
	}

	return 1.f;
}
