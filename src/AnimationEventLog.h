#pragma once

struct AnimationEventLogEntry
{
	AnimationEventLogEntry(const RE::BSAnimationGraphEvent* a_event);

	[[nodiscard]] bool MatchesRegex(const std::regex& a_regex) const;

	RE::BSFixedString tag;
	std::string holderName;
	RE::BSFixedString payload;
	uint32_t timestamp;

	float timeDrawn = 0.f;
};

class AnimationEventLog :
	public RE::BSTEventSink<RE::BSAnimationGraphEvent>
{
public:
	static AnimationEventLog& GetSingleton()
	{
		static AnimationEventLog singleton;
		return singleton;
	}

	// override BSTEventSink
	RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) override;

	[[nodiscard]] bool IsLogEmpty() const;
	void ForEachLogEntry(const std::function<void(AnimationEventLogEntry&)>& a_func, bool a_bReverse);
	void ClearLog();
	bool HasNewEvent() const { return _bHasNewEvent; }
	void ForceHasNewEvent() { _bHasNewEvent = true; }
	void OnPostDisplay();

	[[nodiscard]] bool HasEventSources() const;
	[[nodiscard]] bool HasEventSource(const RE::ObjectRefHandle& a_handle) const;
	void AddEventSource(const RE::ObjectRefHandle& a_handle);
	void RemoveEventSource(const RE::ObjectRefHandle& a_handle);
	void ForEachEventSource(const std::function<void(const RE::ObjectRefHandle& a_handle)>& a_func) const;
	uint32_t GetLastTimestamp() const { return _lastEventTimestamp; }

private:
	AnimationEventLog() = default;
	AnimationEventLog(const AnimationEventLog&) = delete;
	AnimationEventLog(AnimationEventLog&&) = delete;
	virtual ~AnimationEventLog() = default;

	AnimationEventLog& operator=(const AnimationEventLog&) = delete;
	AnimationEventLog& operator=(AnimationEventLog&&) = delete;

	mutable SharedLock _logLock;
	std::deque<AnimationEventLogEntry> _log = {};

	mutable SharedLock _eventSourcesLock;
	std::set<RE::ObjectRefHandle> _eventSources = {};

	bool _bHasNewEvent = false;
	uint32_t _lastEventTimestamp = 0;
};
