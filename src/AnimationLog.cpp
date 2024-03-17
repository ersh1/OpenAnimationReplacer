#include "AnimationLog.h"

#include "ActiveClip.h"
#include "OpenAnimationReplacer.h"
#include "ReplacementAnimation.h"
#include "Settings.h"
#include "UI/UIManager.h"
#include "Utils.h"

AnimationLogEntry::AnimationLogEntry(Event a_event, ActiveClip* a_activeClip, RE::hkbCharacter* a_character) :
	event(a_event)
{
	const auto replacementAnimation = a_activeClip->GetReplacementAnimation();
	bOriginal = replacementAnimation == nullptr;
	bInterruptible = a_activeClip->IsInterruptible();
	animationName = a_activeClip->GetClipGenerator()->animationName.data();
	clipName = a_activeClip->GetClipGenerator()->name.data();

	if (const auto stringData = Utils::GetStringDataFromHkbCharacter(a_character)) {
		projectName = stringData->name.data();
	}

	if (replacementAnimation) {
		if (const auto subMod = replacementAnimation->GetParentSubMod()) {
			subModName = subMod->GetName();
			if (const auto parentMod = subMod->GetParentMod()) {
				modName = parentMod->GetName();
			}
		}
		animPath = replacementAnimation->GetAnimPath();

		// shorten the path by removing the first two directories
		const auto slashCount = std::ranges::count(animPath, '\\');
		if (slashCount >= 2) {
			auto start = std::ranges::find(animPath, '\\');
			auto end = std::ranges::find(std::ranges::subrange(++start, animPath.end()), '\\');
			if (end != animPath.end()) {
				animPath.erase(0, std::ranges::distance(animPath.begin(), ++end));
			}
		}

		// variants
		bVariant = replacementAnimation->HasVariants();
		if (bVariant) {
			variantFilename = replacementAnimation->GetVariantFilename(a_activeClip->GetCurrentIndex());
		}
	} else {
		if (const auto stringData = Utils::GetStringDataFromHkbCharacter(a_character)) {
			animPath = Utils::GetOriginalAnimationName(stringData, a_activeClip->GetOriginalIndex());

			// shorten the path by removing the first directory
			const auto slashCount = std::ranges::count(animPath, '\\');
			if (slashCount >= 1) {
				const auto start = animPath.begin();
				auto end = std::ranges::find(std::ranges::subrange(start, animPath.end()), '\\');
				if (end != animPath.end()) {
					animPath.erase(start, ++end);
				}
			}
		}
	}

	if (Settings::bAnimationLogWriteToTextLog) {
		std::string infoString = bOriginal ? "original" : std::format("{} / {}", modName, subModName);
		std::string variantString = bVariant ? std::format(" [Variant - {}]", variantFilename) : "";
		std::string eventString;
		switch (event) {
		case Event::kActivate:
			eventString = "Activated"sv;
			break;
		case Event::kActivateSynchronized:
			eventString = "Activated paired"sv;
			break;
		case Event::kActivateReplace:
			eventString = "Activated (Replaced)"sv;
			break;
		case Event::kActivateReplaceSynchronized:
			eventString = "Activated paired (Replaced)"sv;
			break;
		case Event::kEcho:
			eventString = "Echo"sv;
			break;
		case Event::kEchoReplace:
			eventString = "Echo (Replaced)"sv;
			break;
		case Event::kLoop:
			eventString = "Loop"sv;
			break;
		case Event::kLoopReplace:
			eventString = "Loop (Replaced)"sv;
			break;
		}
		logger::info("AnimationLogEntry: {} animation \"{}\" (Project: {}, Clip: {}) - {} ({}){}", eventString, animationName, projectName, clipName, infoString, animPath, variantString);
	}
}

bool AnimationLogEntry::operator==(const AnimationLogEntry& a_rhs) const
{
	return event == a_rhs.event && 
	    bOriginal == a_rhs.bOriginal && 
		bVariant == a_rhs.bVariant && 
		bInterruptible == a_rhs.bInterruptible &&
		animationName == a_rhs.animationName &&
		clipName == a_rhs.clipName &&
		projectName == a_rhs.projectName &&
		modName == a_rhs.modName &&
		subModName == a_rhs.subModName &&
		animPath == a_rhs.animPath &&
		variantFilename == a_rhs.variantFilename;
}

bool AnimationLogEntry::operator!=(const AnimationLogEntry& a_rhs) const
{
	return !operator==(a_rhs);
}

bool AnimationLogEntry::MatchesRegex(const std::regex& a_regex) const
{
	if (MatchesEvent(a_regex)) {
	    return true;
	}

	if (std::regex_search(animationName, a_regex)) {
		return true;
	}

	if (std::regex_search(clipName, a_regex)) {
		return true;
	}

	if (std::regex_search(modName, a_regex)) {
		return true;
	}

	if (std::regex_search(subModName, a_regex)) {
		return true;
	}

	if (std::regex_search(animPath, a_regex)) {
		return true;
	}

	if (std::regex_search(variantFilename, a_regex)) {
		return true;
	}

	return false;
}

void AnimationLogEntry::IncreaseCount()
{
    ++count;
	timeDrawn = 0.f;
}

bool AnimationLogEntry::MatchesEvent(const std::regex& a_regex) const
{
	auto getEventStrings = [](Event a_event) -> std::vector<std::string_view> {
		switch (a_event) {
		case Event::kNone:
		    return { "None"sv };
		case Event::kActivate:
			return { "Activate"sv };
		case Event::kActivateSynchronized:
			return { "Activate"sv, "Synchronized"sv, "Paired"sv };
		case Event::kEcho:
			return { "Echo"sv };
		case Event::kLoop:
			return { "Loop"sv };
		case Event::kActivateReplace:
			return { "Activate"sv, "Replace"sv };
		case Event::kActivateReplaceSynchronized:
			return { "Activate"sv, "Replace"sv, "Synchronized"sv, "Paired"sv };
		case Event::kEchoReplace:
			return { "Echo"sv, "Replace"sv };
		case Event::kLoopReplace:
			return { "Loop"sv, "Replace"sv };
		case Event::kInterrupt:
			return { "Interrupt"sv };
		case Event::kPairedMismatch:
			return { "Synchronized"sv, "Paired"sv, "Mismatch"sv };
		default:
		    return { ""sv };
		}
	};

	for (auto& str : getEventStrings(event)) {
		if (std::regex_search(str.data(), a_regex)) {
		    return true;
		}
	}
	
	return false;
}

void AnimationLog::LogAnimation(AnimationLogEntry::Event a_event, ActiveClip* a_activeClip, RE::hkbCharacter* a_character)
{
	{
		WriteLocker locker(_animationLogLock);

		const AnimationLogEntry newEntry{ a_event, a_activeClip, a_character };

		// does it match the filter string?
        if (bool bDoFilter = !filter.empty()) {
			std::regex filterRegex;
			try {
				filterRegex = std::regex(filter, std::regex::icase);
			} catch (const std::regex_error&) {
				bDoFilter = false;
			}

			if (bDoFilter) {
				if (!newEntry.MatchesRegex(filterRegex)) {
					return;
				}
			}
		}

		if (_animationLog.size() > 0 && _animationLog.front() == newEntry) {
			auto& frontEntry = _animationLog.front();
			if (frontEntry == newEntry) {
				frontEntry.IncreaseCount();
			}
		} else {
			_animationLog.emplace_front(newEntry);
		}
	}

	ClampLog();
}

void AnimationLog::ClampLog()
{
	WriteLocker locker(_animationLogLock);

	while (_animationLog.size() > Settings::uAnimationLogMaxEntries) {
		_animationLog.pop_back();
	}
}

bool AnimationLog::IsAnimationLogEmpty() const
{
	ReadLocker locker(_animationLogLock);

	return _animationLog.empty();
}

void AnimationLog::ForEachAnimationLogEntry(const std::function<void(AnimationLogEntry&)>& a_func)
{
	ReadLocker locker(_animationLogLock);

	for (auto& logEntry : _animationLog) {
		a_func(logEntry);
	}
}

void AnimationLog::ClearAnimationLog()
{
	WriteLocker locker(_animationLogLock);

	_animationLog.clear();
}

bool AnimationLog::ShouldLogAnimationsForActiveClip(ActiveClip* a_activeClip, AnimationLogEntry::Event a_logEvent) const
{
	if (ShouldLogAnimations()) {
		const auto refrToEvaluate = UI::UIManager::GetSingleton().GetRefrToEvaluate();
		if (refrToEvaluate && (refrToEvaluate == Utils::GetActorFromHkbCharacter(a_activeClip->GetCharacter()))) {
			if (Settings::bAnimationLogOnlyActiveGraph) {
				// check if the current graph is what the animation graph manager considers an active graph
				RE::BSAnimationGraphManagerPtr graphManager = nullptr;
				a_activeClip->GetRefr()->GetAnimationGraphManager(graphManager);
				if (graphManager) {
					const auto& activeGraph = graphManager->graphs[graphManager->GetRuntimeData().activeGraph];
					if (activeGraph->behaviorGraph != a_activeClip->GetCharacter()->behaviorGraph.get()) {
						return false;
					}
				}
			}

			if (a_logEvent >= AnimationLogEntry::Event::kActivateReplace) {
				return true;
			}

			auto shouldLog = [&](Settings::AnimationLogMode a_mode) {
				switch (a_mode) {
				case Settings::AnimationLogMode::kLogReplacementOnly:
					return a_activeClip->HasReplacementAnimation();
				case Settings::AnimationLogMode::kLogPotentialReplacements:
					return a_activeClip->HasReplacements();
				case Settings::AnimationLogMode::kLogAll:
					return true;
				}
				return false;
			};

			switch (a_logEvent) {
			case AnimationLogEntry::Event::kActivate:
			case AnimationLogEntry::Event::kActivateSynchronized:
				return shouldLog(static_cast<Settings::AnimationLogMode>(Settings::uAnimationActivateLogMode));
			case AnimationLogEntry::Event::kEcho:
				return shouldLog(static_cast<Settings::AnimationLogMode>(Settings::uAnimationEchoLogMode));
			case AnimationLogEntry::Event::kLoop:
				return shouldLog(static_cast<Settings::AnimationLogMode>(Settings::uAnimationLoopLogMode));
			}
		}
	}

	return false;
}

void AnimationLog::SetLogAnimations(bool a_enable)
{
	_bLogAnimations = a_enable;
}
