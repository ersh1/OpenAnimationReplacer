#pragma once

#include "AnimationLog.h"
#include "FakeClipGenerator.h"
#include "ReplacementAnimation.h"
#include "Containers.h"

// a core class of OAR - holds additional data and logic about an active clip generator, created when a clip generator is activated and destroyed when it is deactivated
class ActiveClip : public std::enable_shared_from_this<ActiveClip>, public IStateDataContainerHolder, public RE::BSTEventSink<RE::BSAnimationGraphEvent>
{
public:
	struct QueuedReplacement
	{
		enum class Type
		{
			kRestart,
			kContinue,
			kLoop
		};

		QueuedReplacement(ReplacementAnimation* a_replacementAnimation, float a_blendTime, Type a_type, AnimationLogEntry::Event a_replacementEvent) :
			replacementAnimation(a_replacementAnimation),
			blendTime(a_blendTime),
			type(a_type),
			replacementEvent(a_replacementEvent) {}

		QueuedReplacement(ReplacementAnimation* a_replacementAnimation, float a_blendTime, Type a_type, AnimationLogEntry::Event a_replacementEvent, Variant* a_variant, bool a_bReplaceAtTrueEndOfLoop) :
			replacementAnimation(a_replacementAnimation),
			blendTime(a_blendTime),
			type(a_type),
			replacementEvent(a_replacementEvent),
			variant(a_variant),
			bReplaceAtTrueEndOfLoop(a_bReplaceAtTrueEndOfLoop) {}

		ReplacementAnimation* replacementAnimation;
		float blendTime;
		Type type;

		AnimationLogEntry::Event replacementEvent;
		Variant* variant = nullptr;

		bool bReplaceAtTrueEndOfLoop = false;
	};

	struct BlendingClip
	{
		BlendingClip(RE::hkbClipGenerator* a_clipGenerator, float a_blendDuration) :
			clipGenerator(a_clipGenerator), blendDuration(a_blendDuration)
		{}

		bool Update(const RE::hkbContext& a_context, float a_deltaTime);
		float GetBlendWeight() const { return std::max(1.f - (blendElapsedTime / blendDuration), 0.f); }

		FakeClipGenerator clipGenerator;
		float blendDuration = 0.f;
		float blendElapsedTime = 0.f;
	};

	enum class TransitioningReason
	{
		kDefault,
		kVariantEcho,
		kVariantLoop
	};

	std::shared_ptr<ActiveClip> getptr()
	{
		return shared_from_this();
	}

	[[nodiscard]] ActiveClip(RE::hkbClipGenerator* a_clipGenerator, RE::hkbCharacter* a_character, RE::hkbBehaviorGraph* a_behaviorGraph);
	virtual ~ActiveClip();

	// override BSTEventSink
	RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) override;

	bool StateDataUpdate(float a_deltaTime) override;
	bool StateDataOnLoopOrEcho(RE::ObjectRefHandle a_refHandle, ActiveClip* a_activeClip, bool a_bIsEcho) override;
	bool StateDataClearRefrData(RE::ObjectRefHandle a_refHandle) override;
	void StateDataClearData() override;

	bool ShouldReplaceAnimation(const ReplacementAnimation* a_newReplacementAnimation, bool a_bTryVariant, Variant*& a_outVariant);
	void ReplaceAnimation(ReplacementAnimation* a_replacementAnimation, Variant*& a_variant);
	void RestoreOriginalAnimation();
	void QueueReplacementAnimation(ReplacementAnimation* a_replacementAnimation, float a_blendTime, QueuedReplacement::Type a_type, AnimationLogEntry::Event a_replacementEvent, Variant* a_variant = nullptr, bool a_bReplaceAtTrueEndOfLoop = false);
	[[nodiscard]] ReplacementAnimation* PopQueuedReplacementAnimation();
	void ReplaceActiveAnimation(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
	void SetReplacements(class AnimationReplacements* a_replacements) { _replacements = a_replacements; }
	void SetSynchronizedParent(RE::BSSynchronizedClipGenerator* a_parentSynchronizedClipGenerator) { _parentSynchronizedClipGenerator = a_parentSynchronizedClipGenerator; }

	[[nodiscard]] bool IsOriginal() const { return !HasReplacementAnimation(); }
	[[nodiscard]] bool HasReplacementAnimation() const { return _currentReplacementAnimation != nullptr; }
	[[nodiscard]] const ReplacementAnimation* GetReplacementAnimation() const { return _currentReplacementAnimation; }
	[[nodiscard]] bool HasReplacements() const { return _replacements != nullptr; }
	[[nodiscard]] AnimationReplacements* GetReplacements() const { return _replacements; }
	[[nodiscard]] uint16_t GetCurrentIndex() const { return _clipGenerator->animationBindingIndex; }
	[[nodiscard]] uint16_t GetOriginalIndex() const { return _originalIndex; }
	[[nodiscard]] RE::hkbClipGenerator::PlaybackMode GetOriginalMode() const { return _originalMode; }
	[[nodiscard]] uint8_t GetOriginalFlags() const { return _originalFlags; }
	[[nodiscard]] RE::hkbClipGenerator* GetClipGenerator() const { return _clipGenerator; }
	[[nodiscard]] RE::hkbCharacter* GetCharacter() const { return _character; }
	[[nodiscard]] RE::hkbBehaviorGraph* GetBehaviorGraph() const { return _behaviorGraph; }
	[[nodiscard]] RE::TESObjectREFR* GetRefr() const { return _refr; }
	[[nodiscard]] bool IsSynchronizedClip() const { return _parentSynchronizedClipGenerator != nullptr; }
	[[nodiscard]] RE::BSSynchronizedClipGenerator* GetParentSynchronizedClipGenerator() const { return _parentSynchronizedClipGenerator; }
	[[nodiscard]] bool HasRemovedNonAnnotationTriggers() const { return _bRemovedNonAnnotationTriggers; }

	// interruptible anim
	[[nodiscard]] bool IsInterruptible() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetInterruptible() : _bOriginalInterruptible; }
	[[nodiscard]] bool HasQueuedReplacement() const { return _queuedReplacement.has_value(); }
	[[nodiscard]] bool IsReadyToReplace(bool a_bIsAtTrueEndOfLoop = false) const;
	[[nodiscard]] bool IsBlending() const { return !_blendingClipGenerators.empty(); }
	[[nodiscard]] bool IsTransitioning() const { return _bTransitioning; }
	[[nodiscard]] bool ShouldReplaceOnLoop() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetReplaceOnLoop() : true; }
	[[nodiscard]] bool ShouldReplaceOnEcho() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetReplaceOnEcho() : _bOriginalReplaceOnEcho; }
	void SetTransitioning(bool a_bValue, TransitioningReason a_transitioningReason = TransitioningReason::kDefault);
	void StartBlend(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, float a_blendTime);
	void PreUpdate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, float a_timestep);
	void OnActivate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
	void OnPostActivate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
	void OnDeactivate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
	void BackupTriggers(RE::hkbClipGenerator* a_clipGenerator);
	void RestoreBackupTriggers();
	void RemoveNonAnnotationTriggers(RE::hkbClipGenerator* a_clipGenerator);
	void OnActivateSynchronized(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, AnimationReplacements* a_replacements, ReplacementAnimation* a_replacementAnimation, Variant* a_variant = nullptr);
	void OnGenerate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, RE::hkbGeneratorOutput& a_output);
	bool OnEcho(RE::hkbClipGenerator* a_clipGenerator, float a_echoDuration);
	bool OnLoop(RE::hkbClipGenerator* a_clipGenerator);
	[[nodiscard]] RE::hkbClipGenerator* GetLastBlendingClipGenerator() const;

	bool IsInLoopSequence();

	bool ReplacementHasOnTriggerFunctions() const { return _currentReplacementAnimation ? _currentReplacementAnimation->HasValidFunctionSet(Functions::FunctionSetType::kOnTrigger) : false; }
	void RegisterEventSink();
	void UnregisterEventSink();

	[[nodiscard]] bool ShouldRunFunctionsOnLoop() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetRunFunctionsOnLoop() : false; }
	[[nodiscard]] bool ShouldRunFunctionsOnEcho() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetRunFunctionsOnEcho() : false; }

	StateDataContainer<const Conditions::ICondition*> conditionStateData;

	ReplacementTrace trace;

protected:
	bool OnLoopOrEcho(RE::hkbClipGenerator* a_clipGenerator, bool a_bIsEcho, float a_echoDuration = 0.f);
	void RemoveNonAnnotationTriggersFromClipTriggerArray(RE::hkRefPtr<RE::hkbClipTriggerArray>& a_clipTriggerArray);
	bool GetBlendedTracks(std::vector<RE::hkQsTransform>& a_outBlendedTracks);
	float GetBlendWeight() const;

	AnimationReplacements* _replacements = nullptr;
	ReplacementAnimation* _currentReplacementAnimation = nullptr;
	Variant* _currentVariant = nullptr;
	std::optional<QueuedReplacement> _queuedReplacement = std::nullopt;

	RE::hkbClipGenerator* _clipGenerator;
	RE::hkbCharacter* _character;
	RE::hkbBehaviorGraph* _behaviorGraph;
	RE::TESObjectREFR* _refr;

	const uint16_t _originalIndex;
	const RE::hkbClipGenerator::PlaybackMode _originalMode;
	const uint8_t _originalFlags;
	RE::hkRefPtr<RE::hkbClipTriggerArray> _triggersBackup = nullptr;
	RE::hkRefPtr<RE::hkbClipTriggerArray> _originalTriggersBackup = nullptr;
	bool _bTriggersBackedUp = false;
	bool _bRemovedNonAnnotationTriggers = false;
	bool _bIsAtEndOfLoop = false;
	bool _bLogAtEndOfLoop = false;
	const bool _bOriginalInterruptible;
	const bool _bOriginalReplaceOnEcho;

	bool _bTransitioning = false;
	TransitioningReason _transitioningReason = TransitioningReason::kDefault;
	RE::BSSynchronizedClipGenerator* _parentSynchronizedClipGenerator = nullptr;

	// interruptible anim blending
	float _lastGameTime = 0.f;
	std::deque<std::unique_ptr<BlendingClip>> _blendingClipGenerators{};

	bool _bRegisteredSink = false;
};
