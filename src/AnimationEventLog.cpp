#include "AnimationEventLog.h"

#include "Offsets.h"
#include "Settings.h"

AnimationEventLogEntry::AnimationEventLogEntry(const RE::BSAnimationGraphEvent* a_event) :
	tag(a_event->tag), payload(a_event->payload), bFromNotify(false), bTriggeredTransition(false)
{
	const auto holder = const_cast<RE::TESObjectREFR*>(a_event->holder);
	holderName = holder->GetDisplayFullName();

	Init();
}

AnimationEventLogEntry::AnimationEventLogEntry(RE::TESObjectREFR* a_holder, const RE::BSFixedString& a_eventName, bool a_eventTriggeredTransition) :
	tag(a_eventName), bFromNotify(true), bTriggeredTransition(a_eventTriggeredTransition)
{
	holderName = a_holder->GetDisplayFullName();

	Init();
}

void AnimationEventLogEntry::Init()
{
	timestamp = g_durationOfApplicationRunTimeMS;

	if (Settings::bAnimationEventLogWriteToTextLog) {
		if (!payload.empty()) {
			logger::info("AnimationEventLogEntry: [{}] {}.{}", holderName, tag.data(), payload.data());
		} else {
			logger::info("AnimationEventLogEntry: [{}] {}", holderName, tag.data());
		}
	}
}

bool AnimationEventLogEntry::MatchesRegex(const std::regex& a_regex) const
{
	if (std::regex_search(tag.data(), a_regex)) {
		return true;
	}

	if (std::regex_search(payload.data(), a_regex)) {
		return true;
	}

	if (std::regex_search(holderName, a_regex)) {
		return true;
	}

	return false;
}

RE::BSEventNotifyControl AnimationEventLog::ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
{
	WriteLocker locker(_logLock);
	_log.emplace_back(a_event);
	_bHasNewEvent = true;

	RefreshFilterInternal();

	return RE::BSEventNotifyControl::kContinue;
}

void AnimationEventLog::TryAddEvent(RE::IAnimationGraphManagerHolder* a_holder, const RE::BSFixedString& a_eventName, bool a_eventTriggeredTransition)
{
	const auto holder = SKSE::stl::adjust_pointer<RE::TESObjectREFR>(a_holder, -0x38);
	if (GetLogNotifies(holder->GetHandle())) {
		WriteLocker locker(_logLock);
		_log.emplace_back(holder, a_eventName, a_eventTriggeredTransition);
		_bHasNewEvent = true;

		RefreshFilterInternal();
	}
}

bool AnimationEventLog::IsLogEmpty() const
{
	ReadLocker locker(_logLock);

	return _log.empty();
}

int32_t AnimationEventLog::GetLogEntryCount() const
{
	ReadLocker locker(_logLock);

	return static_cast<int32_t>(_filteredLog.size());
}

void AnimationEventLog::ForEachLogEntry(const std::function<void(AnimationEventLogEntry*)>& a_func, bool a_bReverse)
{
	ReadLocker locker(_logLock);

	if (a_bReverse) {
		for (auto& logEntry : _filteredLog) {
			a_func(logEntry);
		}
	} else {
		for (auto it = _filteredLog.rbegin(); it != _filteredLog.rend(); ++it) {
			a_func(*it);
		}
	}
}

void AnimationEventLog::ForEachLogEntry(const std::function<void(AnimationEventLogEntry*)>& a_func, bool a_bReverse, int32_t a_clipperDisplayStart, int32_t a_clipperDisplayEnd)
{
	ReadLocker locker(_logLock);

	if (a_bReverse) {
		auto start_it = _filteredLog.begin();
		std::advance(start_it, a_clipperDisplayStart);
		auto end_it = _filteredLog.begin();
		std::advance(end_it, a_clipperDisplayEnd);

		for (auto it = start_it; it != end_it; ++it) {
			a_func(*it);
		}
	} else {
		auto start_it = _filteredLog.rbegin();
		std::advance(start_it, a_clipperDisplayStart);
		auto end_it = _filteredLog.rbegin();
		std::advance(end_it, a_clipperDisplayEnd);

		for (auto it = start_it; it != end_it; ++it) {
			a_func(*it);
		}
	}
}

void AnimationEventLog::ClearLog()
{
	WriteLocker locker(_logLock);

	_log.clear();
	_lastEventTimestamp = 0;

	_filteredLog.clear();
}

void AnimationEventLog::OnPostDisplay()
{
	_bHasNewEvent = false;
}

bool AnimationEventLog::HasEventSources() const
{
	ReadLocker locker(_eventSourcesLock);

	return !_eventSources.empty();
}

bool AnimationEventLog::HasEventSource(const RE::ObjectRefHandle& a_handle) const
{
	ReadLocker locker(_eventSourcesLock);

	return _eventSources.contains(a_handle);
}

void AnimationEventLog::AddEventSource(const RE::ObjectRefHandle& a_handle)
{
	if (!a_handle) {
		return;
	}
	const auto refr = a_handle.get();
	if (!refr) {
		return;
	}

	RE::BSAnimationGraphManagerPtr graphManager;
	refr->GetAnimationGraphManager(graphManager);
	if (graphManager) {
		for (const auto& animationGraph : graphManager->graphs) {
			const auto eventSource = animationGraph->GetEventSource<RE::BSAnimationGraphEvent>();
			bool bAlreadyAdded = false;
			for (const auto& sink : eventSource->sinks) {
				if (sink == this) {
					bAlreadyAdded = true;
				}
			}
			if (!bAlreadyAdded) {
				graphManager->graphs.front()->GetEventSource<RE::BSAnimationGraphEvent>()->AddEventSink(this);
			}
		}

		WriteLocker locker(_eventSourcesLock);
		_eventSources.emplace(a_handle, EventSourceData());
	}
}

void AnimationEventLog::RemoveEventSource(const RE::ObjectRefHandle& a_handle)
{
	if (a_handle) {
		if (const auto refr = a_handle.get()) {
			RE::BSAnimationGraphManagerPtr graphManager;
			refr->GetAnimationGraphManager(graphManager);
			if (graphManager) {
				for (const auto& animationGraph : graphManager->graphs) {
					const auto eventSource = animationGraph->GetEventSource<RE::BSAnimationGraphEvent>();
					for (const auto& sink : eventSource->sinks) {
						if (sink == this) {
							eventSource->RemoveEventSink(this);
							break;
						}
					}
				}
			}
		}
	}

	WriteLocker locker(_eventSourcesLock);
	_eventSources.erase(a_handle);
}

bool AnimationEventLog::GetLogNotifies(const RE::ObjectRefHandle& a_handle) const
{
	ReadLocker locker(_eventSourcesLock);

	if (const auto search = _eventSources.find(a_handle); search != _eventSources.end()) {
		return search->second.bListNotify;
	}

	return false;
}

void AnimationEventLog::SetLogNotifies(const RE::ObjectRefHandle& a_handle, bool a_bLogNotify)
{
	ReadLocker locker(_eventSourcesLock);

	if (const auto search = _eventSources.find(a_handle); search != _eventSources.end()) {
		search->second.bListNotify = a_bLogNotify;
	}
}

void AnimationEventLog::ForEachEventSource(const std::function<void(const RE::ObjectRefHandle& a_handle)>& a_func) const
{
	ReadLocker locker(_eventSourcesLock);

	for (auto& logEntry : _eventSources) {
		a_func(logEntry.first);
	}
}

void AnimationEventLog::RefreshFilter()
{
	WriteLocker locker(_logLock);

	RefreshFilterInternal();
}

void AnimationEventLog::RefreshFilterInternal()
{
	bool bDoFilter = !filter.empty();
	// construct regex from the filter string
	std::regex filterRegex;
	try {
		filterRegex = std::regex(filter, std::regex::icase);
	} catch (const std::regex_error&) {
		bDoFilter = false;
	}

	_filteredLog.clear();
	for (auto& entry : _log) {
		if (!bDoFilter || entry.MatchesRegex(filterRegex)) {
			_filteredLog.emplace_back(&entry);
		}
	}
}
