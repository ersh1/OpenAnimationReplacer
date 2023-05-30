#include "AnimationLog.h"

#include "ActiveClip.h"
#include "OpenAnimationReplacer.h"
#include "ReplacementAnimation.h"
#include "Settings.h"
#include "Utils.h"
#include "UI/UIManager.h"


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
        std::string eventString;
        switch (event) {
        case Event::kActivate:
            eventString = "Activated"sv;
            break;
        case Event::kActivateReplace:
            eventString = "Activated (Replaced)"sv;
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
        logger::info("AnimationLogEntry: {} animation \"{}\" (Project: {}, Clip: {}) - {} ({})", eventString, animationName, projectName, clipName, infoString, animPath);
    }
}


void AnimationLog::LogAnimation(AnimationLogEntry::Event a_event, ActiveClip* a_activeClip, RE::hkbCharacter* a_character)
{
    {
        WriteLocker locker(_animationLogLock);

        _animationLog.emplace_front(a_event, a_activeClip, a_character);
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
