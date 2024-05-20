#include "StateDataContainer.h"

#include "OpenAnimationReplacer.h"

Conditions::IStateData* StateDataContainerEntry::AccessData(RE::hkbClipGenerator* a_clipGenerator) const
{
	if (_data->ShouldResetOnLoopOrEcho(a_clipGenerator, false) || _data->ShouldResetOnLoopOrEcho(a_clipGenerator, true)) {
		auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClipSharedPtr(a_clipGenerator);
		const bool bFound = CheckRelevantClips(activeClip.get());

		if (!bFound) {
			_relevantClips.emplace_back(activeClip);
		}
	}

	_timeSinceLastAccess = 0.f;
	return _data.get();
}

void StateDataContainerEntry::OnLoopOrEcho(ActiveClip* a_clipGenerator, bool a_bIsEcho)
{
	_data->OnLoopOrEcho(a_clipGenerator->GetClipGenerator(), a_bIsEcho);
}

bool StateDataContainerEntry::ShouldResetOnLoopOrEcho(ActiveClip* a_activeClip, bool a_bIsEcho) const
{
	if (_data->ShouldResetOnLoopOrEcho(a_activeClip->GetClipGenerator(), a_bIsEcho)) {
		return CheckRelevantClips(a_activeClip);
	}

	return false;
}

bool StateDataContainerEntry::CheckRelevantClips(ActiveClip* a_activeClip) const
{
	for (auto it = _relevantClips.begin(); it != _relevantClips.end();) {
		if (auto clip = it->lock()) {
			if (clip.get() == a_activeClip) {
				return true;
			}
			++it;
		} else {
			it = _relevantClips.erase(it);
		}
	}

	return false;
}

void IStateDataContainerHolder::RegisterStateDataContainer()
{
	OpenAnimationReplacer::GetSingleton().RegisterStateData(this);
}

void IStateDataContainerHolder::UnregisterStateDataContainer()
{
	OpenAnimationReplacer::GetSingleton().UnregisterStateData(this);
}
