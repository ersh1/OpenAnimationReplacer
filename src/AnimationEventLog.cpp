#include "AnimationEventLog.h"

#include "Offsets.h"
#include "Settings.h"

AnimationEventLogEntry::AnimationEventLogEntry(const RE::BSAnimationGraphEvent* a_event) :
	tag(a_event->tag), payload(a_event->payload)
{
	const auto holder = const_cast<RE::TESObjectREFR*>(a_event->holder);
	holderName = holder->GetDisplayFullName();

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
	_log.emplace_front(a_event);
	_bHasNewEvent = true;

	return RE::BSEventNotifyControl::kContinue;
}

bool AnimationEventLog::IsLogEmpty() const
{
	ReadLocker locker(_logLock);

	return _log.empty();
}

void AnimationEventLog::ForEachLogEntry(const std::function<void(AnimationEventLogEntry&)>& a_func, bool a_bReverse)
{
	ReadLocker locker(_logLock);

	if (a_bReverse) {
		for (auto it = _log.rbegin(); it != _log.rend(); ++it) {
			a_func(*it);
		}
	} else {
		for (auto& logEntry : _log) {
			a_func(logEntry);
		}
	}
}

void AnimationEventLog::ClearLog()
{
	WriteLocker locker(_logLock);

	_log.clear();
	_lastEventTimestamp = 0;
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
		_eventSources.emplace(a_handle);
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

void AnimationEventLog::ForEachEventSource(const std::function<void(const RE::ObjectRefHandle& a_handle)>& a_func) const
{
	ReadLocker locker(_eventSourcesLock);

	for (auto& logEntry : _eventSources) {
		a_func(logEntry);
	}
}
