#include "StateDataContainer.h"

#include "OpenAnimationReplacer.h"

Conditions::IStateData* StateDataContainerEntry::AccessData(RE::hkbClipGenerator* a_clipGenerator)
{
	if (auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_clipGenerator)) {
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

	return _relevantClips.contains(a_activeClip);
}

bool StateDataContainerEntry::HasAnyActiveClips() const
{
	ReadLocker locker(_clipsLock);

	return !_activeClips.empty();
}

void StateDataContainerEntry::AddActiveClip(ActiveClip* a_activeClip)
{
	WriteLocker locker(_clipsLock);

	_activeClips.emplace(a_activeClip);
	if (_data->ShouldResetOnLoopOrEcho(a_activeClip->GetClipGenerator(), false) || _data->ShouldResetOnLoopOrEcho(a_activeClip->GetClipGenerator(), true)) {
		_relevantClips.emplace(a_activeClip);
	}

	// register callback
	auto [it, bSuccess] = _registeredCallbacks.try_emplace(a_activeClip, std::make_shared<DestroyedCallback>([&](auto a_destroyedClip) {
		RemoveActiveClip(a_destroyedClip);
	}));

	if (bSuccess) {
		auto weakPtr = std::weak_ptr(it->second);
		a_activeClip->RegisterDestroyedCallback(weakPtr);
	}
}

void StateDataContainerEntry::RemoveActiveClip(ActiveClip* a_activeClip)
{
	WriteLocker locker(_clipsLock);

	_activeClips.erase(a_activeClip);
	_relevantClips.erase(a_activeClip);
}

void IStateDataContainerHolder::RegisterStateDataContainer()
{
	OpenAnimationReplacer::GetSingleton().RegisterStateData(this);
}

void IStateDataContainerHolder::UnregisterStateDataContainer()
{
	OpenAnimationReplacer::GetSingleton().UnregisterStateData(this);
}
