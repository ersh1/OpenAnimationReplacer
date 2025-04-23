#pragma once

struct AnimationEventLogEntry
{
	AnimationEventLogEntry(const RE::BSAnimationGraphEvent* a_event);
	AnimationEventLogEntry(RE::TESObjectREFR* a_holder, const RE::BSFixedString& a_eventName, bool a_eventTriggeredTransition);

	void Init();

	[[nodiscard]] bool MatchesRegex(const std::regex& a_regex) const;

	RE::BSFixedString tag;
	std::string holderName;
	RE::BSFixedString payload;
	uint32_t timestamp;

	bool bFromNotify = false;
	bool bTriggeredTransition = false;

	float timeDrawn = 0.f;
};

struct EventSourceData
{
	EventSourceData() = default;
	bool bListNotify = false;
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

	void TryAddEvent(RE::IAnimationGraphManagerHolder* a_holder, const RE::BSFixedString& a_eventName, bool a_eventTriggeredTransition);

	[[nodiscard]] bool IsLogEmpty() const;
	int32_t GetLogEntryCount() const;
	void ForEachLogEntry(const std::function<void(AnimationEventLogEntry*)>& a_func, bool a_bReverse);
	void ForEachLogEntry(const std::function<void(AnimationEventLogEntry*)>& a_func, bool a_bReverse, int32_t a_clipperDisplayStart, int32_t a_clipperDisplayEnd);
	void ClearLog();
	bool HasNewEvent() const { return _bHasNewEvent; }
	void ForceHasNewEvent() { _bHasNewEvent = true; }
	void OnPostDisplay();

	[[nodiscard]] bool HasEventSources() const;
	[[nodiscard]] bool HasEventSource(const RE::ObjectRefHandle& a_handle) const;
	void AddEventSource(const RE::ObjectRefHandle& a_handle);
	void RemoveEventSource(const RE::ObjectRefHandle& a_handle);
	bool GetLogNotifies(const RE::ObjectRefHandle& a_handle) const;
	void SetLogNotifies(const RE::ObjectRefHandle& a_handle, bool a_bLogNotify);
	void ForEachEventSource(const std::function<void(const RE::ObjectRefHandle& a_handle)>& a_func) const;
	uint32_t GetLastTimestamp() const { return _lastEventTimestamp; }
	void RefreshFilter();

	std::string filter = {};

private:
	AnimationEventLog() = default;
	AnimationEventLog(const AnimationEventLog&) = delete;
	AnimationEventLog(AnimationEventLog&&) = delete;
	virtual ~AnimationEventLog() = default;

	AnimationEventLog& operator=(const AnimationEventLog&) = delete;
	AnimationEventLog& operator=(AnimationEventLog&&) = delete;

	void RefreshFilterInternal();

	mutable SharedLock _logLock;
	std::vector<AnimationEventLogEntry> _log = {};
	std::vector<AnimationEventLogEntry*> _filteredLog = {};

	mutable SharedLock _eventSourcesLock;
	std::map<RE::ObjectRefHandle, EventSourceData> _eventSources = {};

	bool _bHasNewEvent = false;
	uint32_t _lastEventTimestamp = 0;
};
