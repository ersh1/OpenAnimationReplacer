#pragma once

struct AnimationLogEntry
{
    enum class Event : uint8_t
    {
		kNone,
		
        kActivate,
		kActivateSynchronized,
        kEcho,
        kLoop,
        kActivateReplace,
		kActivateReplaceSynchronized,
        kEchoReplace,
        kLoopReplace,
        kInterrupt
    };

    AnimationLogEntry(Event a_event, class ActiveClip* a_activeClip, RE::hkbCharacter* a_character);

    Event event;
    bool bOriginal = false;
	bool bVariant = false;
    bool bInterruptible = false;
    std::string animationName{};
    std::string clipName{};
    std::string projectName{};
    std::string modName{};
    std::string subModName{};
    std::string animPath{};
	std::string variantFilename{};

    float timeDrawn = 0.f;
};

class AnimationLog
{
public:
    static AnimationLog& GetSingleton()
    {
        static AnimationLog singleton;
        return singleton;
    }

    void LogAnimation(AnimationLogEntry::Event a_event, ActiveClip* a_activeClip, RE::hkbCharacter* a_character);
    void ClampLog();
    [[nodiscard]] bool IsAnimationLogEmpty() const;
    void ForEachAnimationLogEntry(const std::function<void(AnimationLogEntry&)>& a_func);
    void ClearAnimationLog();
    [[nodiscard]] bool ShouldLogAnimations() const { return _bLogAnimations; }
    [[nodiscard]] bool ShouldLogAnimationsForActiveClip(ActiveClip* a_activeClip, AnimationLogEntry::Event a_logEvent) const;
    void SetLogAnimations(bool a_enable);

private:
    AnimationLog() = default;
    AnimationLog(const AnimationLog&) = delete;
    AnimationLog(AnimationLog&&) = delete;
    virtual ~AnimationLog() = default;

    AnimationLog& operator=(const AnimationLog&) = delete;
    AnimationLog& operator=(AnimationLog&&) = delete;

    mutable SharedLock _animationLogLock;
    std::deque<AnimationLogEntry> _animationLog = {};
    bool _bLogAnimations = false;
};
