#include "ActiveClip.h"

#include "Offsets.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"

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

void ActiveClip::ReplaceAnimation(const ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex /* = std::nullopt*/)
{
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

void ActiveClip::QueueReplacementAnimation(ReplacementAnimation* a_replacementAnimation, float a_blendTime, bool a_bRestart, AnimationLogEntry::Event a_replacementEvent, std::optional<uint16_t> a_variantIndex /* = std::nullopt*/)
{
    _queuedReplacement = { a_replacementAnimation, a_blendTime, a_bRestart, a_replacementEvent, a_variantIndex };
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
	const bool bRestart = _queuedReplacement->bRestart;

	float startTime = 0.f;
	if (!bRestart && a_clipGenerator->animationControl) {
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

	if (!bRestart) {
	    a_clipGenerator->animationControl->localTime = startTime;
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
    _blendFromClipGenerator = std::make_unique<FakeClipGenerator>(a_clipGenerator);
    _blendDuration = a_blendTime;

    _blendElapsedTime = 0.f;
    _lastGameTime = OpenAnimationReplacer::gameTimeCounter;

    _blendFromClipGenerator->Activate(a_context);
}

void ActiveClip::StopBlend()
{
    _blendFromClipGenerator = nullptr;
    _blendDuration = 0.f;
}

void ActiveClip::PreUpdate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, [[maybe_unused]] float a_timestep)
{
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
				QueueReplacementAnimation(newReplacementAnim, blendTime, true, AnimationLogEntry::Event::kInterrupt);
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
			hkbClipGenerator_ComputeBeginAndEndLocalTime(a_clipGenerator, a_timestep + blendTime, &prevLocalTime, &newLocalTime, &numLoops, &newAtEnd);
			if (numLoops > 0) {
				// recheck conditions to see if we should replace the animation
				if (!OnLoop(a_clipGenerator)) {
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

	if (IsBlending()) {
		const float currentGameTime = OpenAnimationReplacer::gameTimeCounter;
		const float deltaTime = currentGameTime - _lastGameTime;
		_blendElapsedTime += deltaTime;
		_lastGameTime = currentGameTime;

		if (_blendElapsedTime < _blendDuration) {
			_blendFromClipGenerator->Update(a_context, deltaTime);
		}
	}
}

void ActiveClip::PreGenerate([[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] const RE::hkbContext& a_context, [[maybe_unused]] RE::hkbGeneratorOutput& a_output)
{
    const float currentGameTime = OpenAnimationReplacer::gameTimeCounter;
    const float deltaTime = currentGameTime - _lastGameTime;
    _blendElapsedTime += deltaTime;
    _lastGameTime = currentGameTime;

    if (_blendElapsedTime < _blendDuration) {
        _blendFromClipGenerator->Update(a_context, deltaTime);
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

void ActiveClip::OnActivateSynchronized(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, AnimationReplacements* a_replacements, const ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex)
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

    if (_blendElapsedTime >= _blendDuration) {
        StopBlend();
        return;
    }

    if (a_output.IsValid(RE::hkbGeneratorOutput::StandardTracks::TRACK_POSE)) {
        RE::hkbGeneratorOutput::Track poseTrack = a_output.GetTrack(RE::hkbGeneratorOutput::StandardTracks::TRACK_POSE);

        auto poseOut = poseTrack.GetDataQsTransform();

        if (const auto binding = _blendFromClipGenerator->animationControl->binding) {
            if (const auto& blendFromAnimation = binding->animation) {
                std::vector<RE::hkQsTransform> sampledTransformTracks{};
                sampledTransformTracks.resize(blendFromAnimation->numberOfTransformTracks);
                std::vector<float> sampledFloatTracks{};
                sampledFloatTracks.resize(blendFromAnimation->numberOfFloatTracks);

                blendFromAnimation->SamplePartialTracks(_blendFromClipGenerator->localTime, blendFromAnimation->numberOfTransformTracks, sampledTransformTracks.data(), blendFromAnimation->numberOfFloatTracks, sampledFloatTracks.data(), nullptr);

                float lerpAmount = std::clamp(Utils::InterpEaseInOut(0.f, 1.f, _blendElapsedTime / _blendDuration, 2), 0.f, 1.f);

                auto numBlend = std::min(poseTrack.GetNumData(), static_cast<int16_t>(blendFromAnimation->numberOfTransformTracks));
                hkbBlendPoses(numBlend, sampledTransformTracks.data(), poseOut, lerpAmount, poseOut);
            }
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
			QueueReplacementAnimation(newReplacementAnim, blendTime, true, AnimationLogEntry::Event::kEchoReplace);
			return true;
		}
	}

    return false;
}

bool ActiveClip::OnLoop(RE::hkbClipGenerator* a_clipGenerator)
{
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
			QueueReplacementAnimation(newReplacementAnim, blendTime, true, AnimationLogEntry::Event::kLoopReplace);
			return true;
		}
	}

    return false;
}

float ActiveClip::GetRandomFloat(const Conditions::IRandomConditionComponent* a_randomComponent)
{
    // Check if the condition belongs to a submod that shares random results
	if (const auto parentCondition = a_randomComponent->GetParentCondition()) {
	    if (const auto parentConditionSet = parentCondition->GetParentConditionSet()) {
	        if (const auto parentSubMod = parentConditionSet->GetParentSubMod()) {
	            if (parentSubMod->IsSharingRandomResults()) {
	                return parentSubMod->GetSharedRandom(this, a_randomComponent);
	            }
	        }
	    }
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

float ActiveClip::GetVariantRandom(const ReplacementAnimation* a_replacementAnimation)
{
    if (const auto parentSubMod = a_replacementAnimation->GetParentSubMod()) {
		if (parentSubMod->IsSharingRandomResults()) {
		    return parentSubMod->GetVariantRandom(this);
		}
    }

	return Utils::GetRandomFloat(0.f, 1.f);
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
