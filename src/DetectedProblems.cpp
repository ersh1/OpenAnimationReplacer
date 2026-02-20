#include "DetectedProblems.h"

#include "OpenAnimationReplacer.h"
#include "ReplacerMods.h"
#include "UI/UIManager.h"

#include <ranges>

void DetectedProblems::UpdateShowErrorBanner() const
{
	if (_bIsOutdated || !_missingPlugins.empty() || !_invalidPlugins.empty() || !_subModsWithInvalidEntries.empty() || !_replacerModsWithInvalidEntries.empty()) {
		UI::UIManager::GetSingleton().bShowErrorBanner = true;
	} else {
		UI::UIManager::GetSingleton().bShowErrorBanner = false;
	}
}

void DetectedProblems::MarkOutdatedVersion()
{
	_bIsOutdated = true;

	UpdateShowErrorBanner();
}

void DetectedProblems::AddMissingPluginName(std::string_view a_pluginName, REL::Version a_pluginVersion)
{
	{
		WriteLocker locker(_dataLock);
		_missingPlugins.emplace(a_pluginName, a_pluginVersion);
	}

	UpdateShowErrorBanner();
}

void DetectedProblems::AddInvalidPluginName(std::string_view a_pluginName, REL::Version a_pluginVersion)
{
	{
		WriteLocker locker(_dataLock);
		_invalidPlugins.emplace(a_pluginName, a_pluginVersion);
	}

	UpdateShowErrorBanner();
}

bool DetectedProblems::CheckForSubModsSharingPriority()
{
	// check for duplicated priorities
	WriteLocker locker(_dataLock);
	_subModsSharingPriority.clear();

	OpenAnimationReplacer::GetSingleton().ForEachReplacerMod([&](const auto& a_replacerMod) {
		std::unordered_map<int32_t, const SubMod*> priorityMap{};
		a_replacerMod->ForEachSubMod([&](const SubMod* a_subMod) {
			const auto priority = a_subMod->GetPriority();
			auto [it, bInserted] = priorityMap.try_emplace(priority, a_subMod);
			if (!bInserted) {
				_subModsSharingPriority[priority].insert(it->second);
				_subModsSharingPriority[priority].insert(a_subMod);
			}

			return RE::BSVisit::BSVisitControl::kContinue;
		});
	});

	// filter out results further by checking if they share the same project data, or if they are legacy ActorBase replacers
	for (auto outerIt = _subModsSharingPriority.begin(); outerIt != _subModsSharingPriority.end();) {
		// Create a map of ReplacerProjectData pointers to SubMod objects that contain them, also remove those that are legacy ActorBase replacers
		auto& subMods = outerIt->second;

		std::map<ReplacerProjectData*, std::set<const SubMod*>> projectSubMods;
		for (auto it = subMods.begin(); it != subMods.end();) {
			if (auto& subMod = *it; subMod->IsFromLegacyActorBase()) {
				it = subMods.erase(it);
			} else {
				subMod->ForEachReplacerProject([&](const auto& a_replacerProject) {
					projectSubMods[a_replacerProject].insert(subMod);
					return RE::BSVisit::BSVisitControl::kContinue;
				});
				++it;
			}
		}

		// Erase subMods that don't share any ReplacerProjectData with another subMod
		for (auto it = subMods.begin(); it != subMods.end();) {
			bool bShared = false;
			(*it)->ForEachReplacerProject([&](const auto& a_replacerProject) {
				if (projectSubMods[a_replacerProject].size() > 1) {
					bShared = true;
					return RE::BSVisit::BSVisitControl::kStop;
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});

			if (!bShared) {
				it = subMods.erase(it);
			} else {
				++it;
			}
		}

		if (subMods.empty()) {
			outerIt = _subModsSharingPriority.erase(outerIt);
		} else {
			++outerIt;
		}
	}

	UpdateShowErrorBanner();

	return HasSubModsSharingPriority();
}

bool DetectedProblems::CheckForSubModsWithInvalidEntries()
{
	WriteLocker locker(_dataLock);
	_subModsWithInvalidEntries.clear();

	OpenAnimationReplacer::GetSingleton().ForEachReplacerMod([&](const auto& a_replacerMod) {
		a_replacerMod->ForEachSubMod([&](const SubMod* a_subMod) {
			if (a_subMod->HasInvalidConditions()) {
				_subModsWithInvalidEntries.insert(a_subMod);
			}
			if (a_subMod->HasInvalidFunctions()) {
				_subModsWithInvalidEntries.insert(a_subMod);
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
	});

	UpdateShowErrorBanner();

	return HasSubModsWithInvalidEntries();
}

bool DetectedProblems::CheckForReplacerModsWithInvalidEntries()
{
	WriteLocker locker(_dataLock);
	_replacerModsWithInvalidEntries.clear();

	OpenAnimationReplacer::GetSingleton().ForEachReplacerMod([&](const auto& a_replacerMod) {
		if (a_replacerMod->HasInvalidConditions(true)) {
			_replacerModsWithInvalidEntries.insert(a_replacerMod);
		}
	});

	UpdateShowErrorBanner();

	return HasReplacerModsWithInvalidEntries();
}

bool DetectedProblems::CheckForProblems()
{
	bool bHasProblems = false;

	if (CheckForSubModsSharingPriority()) {
		bHasProblems = true;
	}
	if (CheckForSubModsWithInvalidEntries()) {
		bHasProblems = true;
	}
	if (CheckForReplacerModsWithInvalidEntries()) {
		bHasProblems = true;
	}

	return bHasProblems;
}

DetectedProblems::Severity DetectedProblems::GetProblemSeverity() const
{
	if (IsOutdated() || HasMissingPlugins() || HasInvalidPlugins() || HasSubModsWithInvalidEntries() || HasReplacerModsWithInvalidEntries()) {
		return Severity::kError;
	}

	if (HasSubModsSharingPriority()) {
		return Severity::kWarning;
	}

	return Severity::kNone;
}

std::string_view DetectedProblems::GetProblemMessage() const
{
	if (IsOutdated()) {
		return "ERROR: Required newer Open Animation Replacer version! Click for details..."sv;
	}

	if (HasMissingPlugins()) {
		return "ERROR: Missing required Open Animation Replacer plugins! Click for details..."sv;
	}

	if (HasInvalidPlugins()) {
		return "ERROR: At least one Open Animation Replacer plugin failed to initialize properly! Click for details..."sv;
	}

	if (HasSubModsWithInvalidEntries() || HasReplacerModsWithInvalidEntries()) {
		return "ERROR: Detected mods with invalid conditions! Click for details..."sv;
	}

	if (HasSubModsSharingPriority()) {
		return "WARNING: Detected mods sharing the same priority. Click for details..."sv;
	}

	return "No problems detected."sv;
}

void DetectedProblems::ForEachMissingPlugin(const std::function<void(const std::pair<std::string, REL::Version>&)>& a_func) const
{
	ReadLocker locker(_dataLock);

	for (auto& entry : _missingPlugins) {
		a_func(entry);
	}
}

void DetectedProblems::ForEachInvalidPlugin(const std::function<void(const std::pair<std::string, REL::Version>&)>& a_func) const
{
	ReadLocker locker(_dataLock);

	for (auto& entry : _invalidPlugins) {
		a_func(entry);
	}
}

void DetectedProblems::ForEachSubModSharingPriority(const std::function<void(const SubMod*)>& a_func) const
{
	ReadLocker locker(_dataLock);

	for (const auto& subMods : _subModsSharingPriority | std::views::values) {
		for (auto& subMod : subMods) {
			a_func(subMod);
		}
	}
}

void DetectedProblems::ForEachSubModWithInvalidEntries(const std::function<void(const SubMod*)>& a_func) const
{
	ReadLocker locker(_dataLock);

	for (auto& subMod : _subModsWithInvalidEntries) {
		a_func(subMod);
	}
}

void DetectedProblems::ForEachReplacerModWithInvalidEntries(const std::function<void(const ReplacerMod*)>& a_func) const
{
	ReadLocker locker(_dataLock);

	for (auto& replacerMod : _replacerModsWithInvalidEntries) {
		a_func(replacerMod);
	}
}
