#pragma once

#include "AnimationLog.h"
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

        ReplacementAnimation* replacementAnimation;
        float blendTime;
        AnimationLogEntry::Event replacementEvent;
    };

	// a fake clip generator to blend from when transitioning between replacer animations of the same clip (on interrupt etc)
	// replicates the relevant parts of hkbClipGenerator so we basically can do the same thing as the update function does, to progress the clip naturally just like the game does
    class BlendFromClipGenerator
    {
    public:
        BlendFromClipGenerator(RE::hkbClipGenerator* a_clipGenerator);
        ~BlendFromClipGenerator();

        void Activate(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
        void Update(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, float a_deltaTime);

        uint64_t pad00 = 0;
        uint64_t pad08 = 0;
        uint64_t pad10 = 0;
        uint64_t pad18 = 0;
        uint64_t pad20 = 0;
        uint64_t pad28 = 0;
        uint64_t userData = 0;
        uint64_t pad38 = 0;
        uint64_t pad40 = 0;
        uint64_t pad48 = 0;
        uint64_t pad50 = 0;
        float cropStartAmountLocalTime;                                           // 058
        float cropEndAmountLocalTime;                                             // 05C
        float startTime;                                                          // 060
        float playbackSpeed;                                                      // 064
        float enforcedDuration;                                                   // 068
        float userControlledTimeFraction;                                         // 06C
        int16_t animationBindingIndex;                                            // 070
        SKSE::stl::enumeration<RE::hkbClipGenerator::PlaybackMode, uint8_t> mode; // 072
        uint8_t flags;                                                            // 073
        uint32_t pad74 = 0;
        uint64_t pad78 = 0;
        uint64_t pad80 = 0;
        RE::hkRefPtr<RE::hkaDefaultAnimationControl> animationControl; // 088
        uint64_t pad90 = 0;
        RE::hkaDefaultAnimationControlMapperData* mapperData; // 98
        RE::hkaAnimationBinding* binding;                     // 0A0
        uint64_t padA8 = 0;
        RE::hkQsTransform extractedMotion; // B0
        uint64_t padE0 = 0;
        uint64_t padE8 = 0;
        float localTime;                          // 0F0
        float time;                               // 0F4
        float previousUserControlledTimeFraction; // 0F8
        uint32_t padFC = 0;
        uint32_t pad100 = 0;
        bool atEnd;            // 104
        bool ignoreStartTime;  // 105
        bool pingPongBackward; // 106

        RE::hkRefPtr<RE::hkbBehaviorGraph> behaviorGraph = nullptr;
    };

    [[nodiscard]] ActiveClip(RE::hkbClipGenerator* a_clipGenerator, RE::hkbCharacter* a_character);
    ~ActiveClip();

    void ReplaceAnimation(ReplacementAnimation* a_replacementAnimation);
    void RestoreOriginalAnimation();
    void QueueReplacementAnimation(ReplacementAnimation* a_replacementAnimation, float a_blendTime, AnimationLogEntry::Event a_replacementEvent);
    [[nodiscard]] ReplacementAnimation* PopQueuedReplacementAnimation();
    void ReplaceActiveAnimation(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context);
    void SetReplacements(class AnimationReplacements* a_replacements) { _replacements = a_replacements; }

    [[nodiscard]] bool IsOriginal() const { return !HasReplacementAnimation(); }
    [[nodiscard]] bool HasReplacementAnimation() const { return _currentReplacementAnimation != nullptr; }
    [[nodiscard]] ReplacementAnimation* GetReplacementAnimation() const { return _currentReplacementAnimation; }
    [[nodiscard]] bool HasReplacements() const { return _replacements != nullptr; }
    [[nodiscard]] AnimationReplacements* GetReplacements() const { return _replacements; }
    [[nodiscard]] uint16_t GetOriginalIndex() const { return _originalIndex; }
    [[nodiscard]] uint8_t GetOriginalFlags() const { return _originalFlags; }
    [[nodiscard]] RE::hkbClipGenerator* GetClipGenerator() const { return _clipGenerator; }
    [[nodiscard]] RE::hkbCharacter* GetCharacter() const { return _character; }
    [[nodiscard]] RE::TESObjectREFR* GetRefr() const { return _refr; }

    // interruptible anim
    [[nodiscard]] bool IsInterruptible() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetInterruptible() : _bOriginalInterruptible; }
    [[nodiscard]] bool IsBlending() const { return _blendFromClipGenerator != nullptr; }
    [[nodiscard]] bool IsTransitioning() const { return _bTransitioning; }
    [[nodiscard]] bool ShouldKeepRandomResultsOnLoop() const { return _currentReplacementAnimation ? _currentReplacementAnimation->GetKeepRandomResultsOnLoop() : _bOriginalKeepRandomResultsOnLoop; }
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
    void ClearRandomFloats();

protected:
    AnimationReplacements* _replacements = nullptr;
    ReplacementAnimation* _currentReplacementAnimation = nullptr;
    std::optional<QueuedReplacement> _queuedReplacement = std::nullopt;

    RE::hkbClipGenerator* _clipGenerator;
    RE::hkbCharacter* _character;
    RE::TESObjectREFR* _refr;

    const uint16_t _originalIndex;
    const uint8_t _originalFlags;
    const bool _bOriginalInterruptible;
    const bool _bOriginalKeepRandomResultsOnLoop;

    bool _bTransitioning = false;

    // interruptible anim blending
    float _blendDuration = 0.f;
    float _blendElapsedTime = 0.f;
    float _lastGameTime = 0.f;

    std::unique_ptr<BlendFromClipGenerator> _blendFromClipGenerator = nullptr;

    SharedLock _randomLock;
    std::unordered_map<const Conditions::IRandomConditionComponent*, float> _randomFloats{};
};
