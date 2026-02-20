#include "Containers.h"

#include "OpenAnimationReplacer.h"

IStateData* StateDataContainerEntry::AccessData(RE::hkbClipGenerator* a_clipGenerator)
{
	if (auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClipSharedPtr(a_clipGenerator)) {
		AddActiveClip(activeClip);
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
	ReadLocker locker(_clipsLock);

	for (auto& entry : _activeClips) {
		if (auto activeClip = entry.lock()) {
			if (activeClip.get() == a_activeClip) {
				return true;
			}
		}
	}

	return _activeClips.contains(a_activeClip->shared_from_this());

	return false;
}

bool StateDataContainerEntry::HasAnyActiveClips() const
{
	ReadLocker locker(_clipsLock);

	for (auto& entry : _activeClips) {
		if (!entry.expired()) {
			return true;
		}
	}

	return false;
}

void StateDataContainerEntry::AddActiveClip(std::shared_ptr<ActiveClip>& a_activeClip)
{
	WriteLocker locker(_clipsLock);

	_activeClips.emplace(a_activeClip);
	if (_data->ShouldResetOnLoopOrEcho(a_activeClip->GetClipGenerator(), false) || _data->ShouldResetOnLoopOrEcho(a_activeClip->GetClipGenerator(), true)) {
		_relevantClips.emplace(a_activeClip);
	}
}

void IStateDataContainerHolder::RegisterStateDataContainer()
{
	OpenAnimationReplacer::GetSingleton().RegisterStateData(this);
}

void IStateDataContainerHolder::UnregisterStateDataContainer()
{
	OpenAnimationReplacer::GetSingleton().UnregisterStateData(this);
}
