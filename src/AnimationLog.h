#pragma once

#include "ReplacementAnimation.h"

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
		kInterrupt,
		kPairedMismatch
	};

	AnimationLogEntry() = default;
	AnimationLogEntry(Event a_event, class ActiveClip* a_activeClip, RE::hkbCharacter* a_character);

	[[nodiscard]] bool operator==(const AnimationLogEntry& a_rhs) const;
	[[nodiscard]] bool operator!=(const AnimationLogEntry& a_rhs) const;

	bool MatchesRegex(const std::regex& a_regex) const;
	void IncreaseCount();

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
	ReplacementTrace trace;

	float timeDrawn = 0.f;
	uint32_t count = 1;

	bool bIsValid = false;

protected:
	bool MatchesEvent(const std::regex& a_regex) const;
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
	[[nodiscard]] bool ShouldLogAnimationsForRefr(RE::TESObjectREFR* a_refr) const;
	[[nodiscard]] bool ShouldLogAnimationsForActiveClip(ActiveClip* a_activeClip, AnimationLogEntry::Event a_logEvent) const;
	void SetLogAnimations(bool a_enable);

	std::string filter = {};

	AnimationLogEntry tracedEntry;

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
