#include "ActiveClip.h"

#include "Offsets.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"

#include <ranges>

ActiveClip::ActiveClip(RE::hkbClipGenerator* a_clipGenerator, RE::hkbCharacter* a_character, RE::hkbBehaviorGraph* a_behaviorGraph) :
    _clipGenerator(a_clipGenerator),
    _character(a_character),
	_behaviorGraph(a_behaviorGraph),
    _originalIndex(a_clipGenerator->animationBindingIndex),
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

void ActiveClip::ReplaceAnimation(ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex/* = std::nullopt*/)
{
    if (_currentReplacementAnimation) {
        RestoreOriginalAnimation();
    }

    _currentReplacementAnimation = a_replacementAnimation;

    if (_currentReplacementAnimation) {
        // alter variables for the lifetime of this object to the ones read from the replacement animation
		const uint16_t newBindingIndex = a_variantIndex ? *a_variantIndex : _currentReplacementAnimation->GetIndex(this);
		_clipGenerator->animationBindingIndex = newBindingIndex;  // this is the most important part - this is what actually replaces the animation
        // the animation binding index is the index of an entry inside hkbCharacterStringData->animationNames, which contains the actual path to the animation file or one of the replacements
        if (_currentReplacementAnimation->GetIgnoreNoTriggersFlag()) {
            _clipGenerator->flags &= ~0x10;
        }
    }
}

void ActiveClip::RestoreOriginalAnimation()
{
    _clipGenerator->animationBindingIndex = _originalIndex;
    _clipGenerator->flags = _originalFlags;
    if (_currentReplacementAnimation) {
        _currentReplacementAnimation = nullptr;
    }
}

void ActiveClip::QueueReplacementAnimation(ReplacementAnimation* a_replacementAnimation, float a_blendTime, AnimationLogEntry::Event a_replacementEvent, std::optional<uint16_t> a_variantIndex/* = std::nullopt*/)
{
    _queuedReplacement = { a_replacementAnimation, a_blendTime, a_replacementEvent, a_variantIndex };
}

ReplacementAnimation* ActiveClip::PopQueuedReplacementAnimation()
{
    if (!_queuedReplacement) {
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

    if (blendTime > 0.f) {
        StartBlend(a_clipGenerator, a_context, blendTime);

        // set to null before deactivation so it isn't cleared when hkbClipGenerator::Deactivate is called (we continue using this animation control in the copied clip generator)
        a_clipGenerator->animationControl = nullptr;
    }

    SetTransitioning(true);
    a_clipGenerator->Deactivate(a_context);

	const std::optional<uint16_t> newIndex = _queuedReplacement ? _queuedReplacement->variantIndex : std::nullopt;
	const auto replacement = PopQueuedReplacementAnimation();
	ReplaceAnimation(replacement, newIndex);

    auto& animationLog = AnimationLog::GetSingleton();

    if (animationLog.ShouldLogAnimations() && animationLog.ShouldLogAnimationsForActiveClip(this, replacementEvent)) {
        animationLog.LogAnimation(replacementEvent, this, a_context.character);
    }

    a_clipGenerator->Activate(a_context);
    SetTransitioning(false);
}

void ActiveClip::StartBlend(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, float a_blendTime)
{
    _blendFromClipGenerator = std::make_unique<BlendFromClipGenerator>(a_clipGenerator);
    _blendDuration = a_blendTime;

    _blendElapsedTime = 0.f;
    _lastGameTime = OpenAnimationReplacer::gameTimeCounter;

    _blendFromClipGenerator->Activate(a_clipGenerator, a_context);
}

void ActiveClip::StopBlend()
{
    _blendFromClipGenerator = nullptr;
    _blendDuration = 0.f;
}

void ActiveClip::PreUpdate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, [[maybe_unused]] float a_timestep)
{
    // check if the animation should be interrupted (queue a replacement if so)
    if (!_queuedReplacement && IsInterruptible()) {
        const auto newReplacementAnim = OpenAnimationReplacer::GetSingleton().GetReplacementAnimation(a_context.character, a_clipGenerator, _originalIndex);
		// do not try to replace with other variants here
		std::optional<uint16_t> dummy = std::nullopt;
        if (ShouldReplaceAnimation(newReplacementAnim, false, dummy)) {
            QueueReplacementAnimation(newReplacementAnim, Settings::fBlendTimeOnInterrupt, AnimationLogEntry::Event::kInterrupt);
        }
    }

    // replace anim if queued
    if (_queuedReplacement) {
        ReplaceActiveAnimation(a_clipGenerator, a_context);
    }
}

void ActiveClip::PreGenerate([[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] const RE::hkbContext& a_context, [[maybe_unused]] RE::hkbGeneratorOutput& a_output)
{
    const float currentGameTime = OpenAnimationReplacer::gameTimeCounter;
    const float deltaTime = currentGameTime - _lastGameTime;
    _blendElapsedTime += deltaTime;
    _lastGameTime = currentGameTime;

    if (_blendElapsedTime < _blendDuration) {
        _blendFromClipGenerator->Update(a_clipGenerator, a_context, deltaTime);
    }
}

void ActiveClip::OnActivate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context)
{
    if (!IsTransitioning()) {
        // don't try to replace animation while transitioning interruptible anims as we already replaced it, this should only run on the actual Activate called by the game
        if (const auto replacements = OpenAnimationReplacer::GetSingleton().GetReplacements(a_context.character, a_clipGenerator->animationBindingIndex)) {
            SetReplacements(replacements);
            const RE::BShkbAnimationGraph* animGraph = SKSE::stl::adjust_pointer<RE::BShkbAnimationGraph>(a_context.character, -0xC0);
            RE::Actor* actor = animGraph->holder;
            if (const auto replacementAnimation = replacements->EvaluateConditionsAndGetReplacementAnimation(actor, a_clipGenerator)) {
                ReplaceAnimation(replacementAnimation);
            }
        }
    }
}

void ActiveClip::OnGenerate([[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] const RE::hkbContext& a_context, RE::hkbGeneratorOutput& a_output)
{
    // manually blend the output pose with the previous animation's output pose
    using TrackHeader = RE::hkbGeneratorOutput::TrackHeader;

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

                auto numBlend = std::min(poseTrack.GetNumData(), blendFromAnimation->numberOfTransformTracks);
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
			QueueReplacementAnimation(newReplacementAnim, a_echoDuration, AnimationLogEntry::Event::kEchoReplace);
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
			QueueReplacementAnimation(newReplacementAnim, Settings::fBlendTimeOnLoop, AnimationLogEntry::Event::kLoopReplace);
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

ActiveClip::BlendFromClipGenerator::BlendFromClipGenerator(RE::hkbClipGenerator* a_clipGenerator) :
    //userData(a_clipGenerator->userData),
    cropStartAmountLocalTime(a_clipGenerator->cropStartAmountLocalTime),
    cropEndAmountLocalTime(a_clipGenerator->cropEndAmountLocalTime),
    startTime(a_clipGenerator->startTime),
    playbackSpeed(a_clipGenerator->playbackSpeed),
    enforcedDuration(a_clipGenerator->enforcedDuration),
    userControlledTimeFraction(a_clipGenerator->userControlledTimeFraction),
    animationBindingIndex(a_clipGenerator->animationBindingIndex),
    mode(a_clipGenerator->mode),
    flags(a_clipGenerator->flags),
    animationControl(a_clipGenerator->animationControl),
    mapperData(a_clipGenerator->mapperData),
    binding(a_clipGenerator->binding),
    extractedMotion(a_clipGenerator->extractedMotion),
    localTime(a_clipGenerator->localTime),
    time(a_clipGenerator->time),
    previousUserControlledTimeFraction(a_clipGenerator->previousUserControlledTimeFraction),
    atEnd(a_clipGenerator->atEnd),
    ignoreStartTime(a_clipGenerator->ignoreStartTime),
    pingPongBackward(a_clipGenerator->pingPongBackward) {}

ActiveClip::BlendFromClipGenerator::~BlendFromClipGenerator()
{
    struct FakeCharacter
    {
        FakeCharacter(RE::hkbBehaviorGraph* a_graph) :
            graph(a_graph) {}

        uint64_t pad00;
        uint64_t pad08;
        uint64_t pad10;
        uint64_t pad18;
        uint64_t pad20;
        uint64_t pad28;
        uint64_t pad30;
        uint64_t pad38;
        uint64_t pad40;
        uint64_t pad48;
        uint64_t pad50;
        RE::hkbBehaviorGraph* graph;
    };

    struct FakeContext
    {
        FakeContext(FakeCharacter* a_character) :
            character(a_character) {}

        FakeCharacter* character;
    };

    if (userData) {
        FakeCharacter fakeCharacter(behaviorGraph.get());
        FakeContext fakeContext(&fakeCharacter);
        const auto& fakeContextPtr = reinterpret_cast<RE::hkbContext&>(fakeContext);
        const auto fakeClipGeneratorPtr = reinterpret_cast<RE::hkbClipGenerator*>(this);
        RE::AnimationFileManagerSingleton::GetSingleton()->Unload(fakeContextPtr, fakeClipGeneratorPtr, nullptr);
    }
}

void ActiveClip::BlendFromClipGenerator::Activate([[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context)
{
    const auto fakeClipGeneratorPtr = reinterpret_cast<RE::hkbClipGenerator*>(this); // pretend this is an actual hkbClipGenerator

    // replicate the relevant part of hkbClipGenerator::Activate

    if (!userData) {
        const auto animationFileManagerSingleton = RE::AnimationFileManagerSingleton::GetSingleton();
        animationFileManagerSingleton->Queue(a_context, fakeClipGeneratorPtr, nullptr);
    }

    behaviorGraph = a_context.character->behaviorGraph;
}

void ActiveClip::BlendFromClipGenerator::Update([[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, float a_deltaTime)
{
    auto fakeClipGeneratorPtr = reinterpret_cast<RE::hkbClipGenerator*>(this); // pretend this is an actual hkbClipGenerator

    // replicate what happens in hkbClipGenerator::Update. Not sure everything here is necessary but there might be rare edge cases where it matters (looping etc)

    if (userData == 8) {
        const auto animationFileManagerSingleton = RE::AnimationFileManagerSingleton::GetSingleton();
        if (animationFileManagerSingleton->Load(a_context, fakeClipGeneratorPtr, nullptr)) {
            Activate(a_clipGenerator, a_context);
        }
    }

    if (fabs(playbackSpeed) <= 0.001f) {
        playbackSpeed = playbackSpeed <= 0.f ? 0.001f : -0.001f;
    }

    animationControl->localTime = localTime;
    if (atEnd && mode) {
        hkbClipGenerator_UnkUpdateFunc1(fakeClipGeneratorPtr);
    }

    if (mode == RE::hkbClipGenerator::PlaybackMode::kModeUserControlled) {
        userControlledTimeFraction = std::clamp(userControlledTimeFraction, 0.f, 1.f);
    }

    hkbClipGenerator_UnkUpdateFunc2(fakeClipGeneratorPtr);
    float prevLocalTime = 0.f;
    float newLocalTime = 0.f;
    int32_t loops = 0;
    bool newAtEnd = false;
    hkbClipGenerator_UnkUpdateFunc3_CalcLocalTime(fakeClipGeneratorPtr, a_deltaTime, &prevLocalTime, &newLocalTime, &loops, &newAtEnd);
    if (!atEnd) {
        localTime = newLocalTime;
        animationControl->localTime = newLocalTime;

        atEnd = newAtEnd;
        RE::hkbEventQueue* eventQueue = a_context.eventQueue;
        if (!eventQueue) {
            eventQueue = *reinterpret_cast<RE::hkbEventQueue**>(&a_context.character->eventQueue);
        }
        hkbClipGenerator_UnkUpdateFunc4(fakeClipGeneratorPtr, prevLocalTime, newLocalTime, loops, eventQueue);
        if ((loops & 1) != 0) {
            if (mode != RE::hkbClipGenerator::PlaybackMode::kModeUserControlled) {
                pingPongBackward = !pingPongBackward;
            }
        }
    }

    if (mode != RE::hkbClipGenerator::PlaybackMode::kModeUserControlled) {
        time += a_deltaTime * animationControl->playbackSpeed;
    }

    previousUserControlledTimeFraction = userControlledTimeFraction;
}
