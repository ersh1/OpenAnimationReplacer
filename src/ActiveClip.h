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
        QueuedReplacement(ReplacementAnimation* a_replacementAnimation, float a_blendTime, AnimationLogEntry::Event a_replacementEvent) :
            replacementAnimation(a_replacementAnimation),
            blendTime(a_blendTime),
            replacementEvent(a_replacementEvent) {}

		QueuedReplacement(ReplacementAnimation* a_replacementAnimation, float a_blendTime, AnimationLogEntry::Event a_replacementEvent, std::optional<uint16_t> a_variantIndex) :
			replacementAnimation(a_replacementAnimation),
			blendTime(a_blendTime),
			replacementEvent(a_replacementEvent),
            variantIndex(a_variantIndex) {}

        ReplacementAnimation* replacementAnimation;
        float blendTime;
        AnimationLogEntry::Event replacementEvent;
		std::optional<uint16_t> variantIndex = std::nullopt;
    };
	
	using DestroyedCallback = std::function<void(ActiveClip* a_destroyedClip)>;

    [[nodiscard]] ActiveClip(RE::hkbClipGenerator* a_clipGenerator, RE::hkbCharacter* a_character, RE::hkbBehaviorGraph* a_behaviorGraph);
    ~ActiveClip();

	bool ShouldReplaceAnimation(const ReplacementAnimation* a_newReplacementAnimation, bool a_bTryVariant, std::optional<uint16_t>& a_outVariantIndex);
	void ReplaceAnimation(ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex = std::nullopt);
    void RestoreOriginalAnimation();
    void QueueReplacementAnimation(ReplacementAnimation* a_replacementAnimation, float a_blendTime, AnimationLogEntry::Event a_replacementEvent, std::optional<uint16_t> a_variantIndex = std::nullopt);
    [[nodiscard]] ReplacementAnimation* PopQueuedReplacementAnimation();
    void ReplaceActiveAnimation(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
    void SetReplacements(class AnimationReplacements* a_replacements) { _replacements = a_replacements; }

    [[nodiscard]] bool IsOriginal() const { return !HasReplacementAnimation(); }
    [[nodiscard]] bool HasReplacementAnimation() const { return _currentReplacementAnimation != nullptr; }
    [[nodiscard]] ReplacementAnimation* GetReplacementAnimation() const { return _currentReplacementAnimation; }
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

    // interruptible anim
	[[nodiscard]] bool IsInterruptible() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetInterruptible() : _bOriginalInterruptible; }
    [[nodiscard]] bool IsBlending() const { return _blendFromClipGenerator != nullptr; }
    [[nodiscard]] bool IsTransitioning() const { return _bTransitioning; }
	[[nodiscard]] bool ShouldReplaceOnLoop() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetReplaceOnLoop() : true; }
	[[nodiscard]] bool ShouldReplaceOnEcho() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetReplaceOnEcho() : _bOriginalReplaceOnEcho; }
	[[nodiscard]] bool ShouldKeepRandomResultsOnLoop() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetKeepRandomResultsOnLoop() : _bOriginalKeepRandomResultsOnLoop; }
	[[nodiscard]] bool ShouldShareRandomResults() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetShareRandomResults() : false; }
    void SetTransitioning(bool a_bValue) { _bTransitioning = a_bValue; }
    void StartBlend(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, float a_blendTime);
    void StopBlend();
    void PreUpdate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, float a_timestep);
    void PreGenerate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, RE::hkbGeneratorOutput& a_output);
	void OnActivate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
    void OnGenerate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, RE::hkbGeneratorOutput& a_output);
    bool OnEcho(RE::hkbClipGenerator* a_clipGenerator, float a_echoDuration);
    bool OnLoop(RE::hkbClipGenerator* a_clipGenerator);
    [[nodiscard]] RE::hkbClipGenerator* GetBlendFromClipGenerator() const { return reinterpret_cast<RE::hkbClipGenerator*>(_blendFromClipGenerator.get()); }

    float GetRandomFloat(const Conditions::IRandomConditionComponent* a_randomComponent);
	float GetVariantRandom(const ReplacementAnimation* a_replacementAnimation);
    void ClearRandomFloats();
	void RegisterDestroyedCallback(std::weak_ptr<DestroyedCallback>& a_callback);

protected:
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
    const bool _bOriginalInterruptible;
	const bool _bOriginalReplaceOnEcho;
    const bool _bOriginalKeepRandomResultsOnLoop;

    bool _bTransitioning = false;

    // interruptible anim blending
    float _blendDuration = 0.f;
    float _blendElapsedTime = 0.f;
    float _lastGameTime = 0.f;

    std::unique_ptr<FakeClipGenerator> _blendFromClipGenerator = nullptr;

    SharedLock _randomLock;
    std::unordered_map<const Conditions::IRandomConditionComponent*, float> _randomFloats{};

	ExclusiveLock _callbacksLock;
	std::vector<std::weak_ptr<DestroyedCallback>> _destroyedCallbacks;
};
