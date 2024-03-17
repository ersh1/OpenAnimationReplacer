#pragma once

#include "AnimationLog.h"
#include "FakeClipGenerator.h"
#include "ReplacementAnimation.h"

// a core class of OAR - holds additional data and logic about an active clip generator, created when a clip generator is activated and destroyed when it is deactivated
class ActiveClip
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

		QueuedReplacement(ReplacementAnimation* a_replacementAnimation, float a_blendTime, Type a_type, AnimationLogEntry::Event a_replacementEvent, std::optional<uint16_t> a_variantIndex) :
			replacementAnimation(a_replacementAnimation),
			blendTime(a_blendTime),
			type(a_type),
			replacementEvent(a_replacementEvent),
			variantIndex(a_variantIndex) {}

		ReplacementAnimation* replacementAnimation;
		float blendTime;
		Type type;;
		AnimationLogEntry::Event replacementEvent;
		std::optional<uint16_t> variantIndex = std::nullopt;
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

	struct SequentialVariantData
	{
		void SetActive(bool a_bActive)
		{
			if (a_bActive != bActive) {
				bActive = a_bActive;
				if (!bActive) {
					timeToRemove = Settings::fSequentialVariantLifetime;
				}
			}
		}

		bool Update(float a_deltaTime)
		{
			if (!bActive) {
				timeToRemove -= a_deltaTime;
				return timeToRemove > 0.f;
			}

			return true;
		}

		bool IsAtBeginningOfSequence(const ReplacementAnimation* a_replacementAnimation) const
		{
			if (nextSequentialVariant == 0) {
			    return true;
			}

			if (a_replacementAnimation->HasVariants()) {
				const auto& variants = a_replacementAnimation->GetVariants();
				for (int i = 0; i <= nextSequentialVariant; ++i) {
					if (!variants.GetActiveVariant(i)->ShouldPlayOnce() && nextSequentialVariant > i) {
						return false;
					}
				}

				return true;
			}

			return false;
		}

		size_t nextSequentialVariant;
		std::vector<bool> playOnceVariantHistory{};

		bool bActive = true;
		float timeToRemove = 0.f;
	};

	using DestroyedCallback = std::function<void(ActiveClip* a_destroyedClip)>;

	[[nodiscard]] ActiveClip(RE::hkbClipGenerator* a_clipGenerator, RE::hkbCharacter* a_character, RE::hkbBehaviorGraph* a_behaviorGraph);
	~ActiveClip();

	bool ShouldReplaceAnimation(const ReplacementAnimation* a_newReplacementAnimation, bool a_bTryVariant, std::optional<uint16_t>& a_outVariantIndex);
	void ReplaceAnimation(ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex = std::nullopt);
	void RestoreOriginalAnimation();
	void QueueReplacementAnimation(ReplacementAnimation* a_replacementAnimation, float a_blendTime, QueuedReplacement::Type a_type, AnimationLogEntry::Event a_replacementEvent, std::optional<uint16_t> a_variantIndex = std::nullopt);
	[[nodiscard]] ReplacementAnimation* PopQueuedReplacementAnimation();
	void ReplaceActiveAnimation(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
	void SetReplacements(class AnimationReplacements* a_replacements) { _replacements = a_replacements; }
	void SetSynchronizedParent(RE::BSSynchronizedClipGenerator* a_parentSynchronizedClipGenerator) { _parentSynchronizedClipGenerator = a_parentSynchronizedClipGenerator; }

	void OnReplacedAnimation(ReplacementAnimation* a_previousReplacementAnimation, ReplacementAnimation* a_newReplacementAnimation);

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
	[[nodiscard]] bool IsBlending() const { return !_blendingClipGenerators.empty(); }
	[[nodiscard]] bool IsTransitioning() const { return _bTransitioning; }
	[[nodiscard]] bool ShouldReplaceOnLoop() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetReplaceOnLoop() : true; }
	[[nodiscard]] bool ShouldReplaceOnEcho() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetReplaceOnEcho() : _bOriginalReplaceOnEcho; }
	[[nodiscard]] bool ShouldKeepRandomResultsOnLoop() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetKeepRandomResultsOnLoop() : _bOriginalKeepRandomResultsOnLoop; }
	[[nodiscard]] bool ShouldShareRandomResults() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetShareRandomResults() : false; }
	void SetTransitioning(bool a_bValue);
	void StartBlend(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, float a_blendTime);
	void PreUpdate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, float a_timestep);
	void OnActivate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
	void OnPostActivate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
	void BackupTriggers(RE::hkbClipGenerator* a_clipGenerator);
	void RestoreBackupTriggers();
	void RemoveNonAnnotationTriggers(RE::hkbClipGenerator* a_clipGenerator);
	void OnActivateSynchronized(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, AnimationReplacements* a_replacements, ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex = std::nullopt);
	void OnGenerate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, RE::hkbGeneratorOutput& a_output);
	bool OnEcho(RE::hkbClipGenerator* a_clipGenerator, float a_echoDuration);
	bool OnLoop(RE::hkbClipGenerator* a_clipGenerator);
	[[nodiscard]] RE::hkbClipGenerator* GetLastBlendingClipGenerator() const;

	float GetVariantRandom(const ReplacementAnimation* a_replacementAnimation);
	size_t GetVariantSequential(const ReplacementAnimation* a_replacementAnimation);

	float GetRandomFloat(const Conditions::IRandomConditionComponent* a_randomComponent, SubMod* a_parentSubMod);
	void ClearRandomFloats();
	void RegisterDestroyedCallback(std::weak_ptr<DestroyedCallback>& a_callback);

protected:
	void RemoveNonAnnotationTriggersFromClipTriggerArray(RE::hkRefPtr<RE::hkbClipTriggerArray>& a_clipTriggerArray);
	bool GetBlendedTracks(std::vector<RE::hkQsTransform>& a_outBlendedTracks);
	float GetBlendWeight() const;

	AnimationReplacements* _replacements = nullptr;
	ReplacementAnimation* _currentReplacementAnimation = nullptr;
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
	const bool _bOriginalInterruptible;
	const bool _bOriginalReplaceOnEcho;
	const bool _bOriginalKeepRandomResultsOnLoop;

	bool _bTransitioning = false;
	RE::BSSynchronizedClipGenerator* _parentSynchronizedClipGenerator = nullptr;

	// interruptible anim blending
	float _lastGameTime = 0.f;
	std::deque<std::unique_ptr<BlendingClip>> _blendingClipGenerators{};

	SharedLock _randomLock;
	std::unordered_map<const Conditions::IRandomConditionComponent*, float> _randomFloats{};

	SharedLock _sequentialVariantLock;
	std::unordered_map<const ReplacementAnimation*, SequentialVariantData> _sequentialVariantData{};

	ExclusiveLock _callbacksLock;
	std::vector<std::weak_ptr<DestroyedCallback>> _destroyedCallbacks;
};
