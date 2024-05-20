#pragma once
#include "Offsets.h"
#include "Settings.h"

#include <API/OpenAnimationReplacer-ConditionTypes.h>

#include <unordered_set>

class ActiveClip;

class StateDataContainerEntry
{
public:
	StateDataContainerEntry(const RE::ObjectRefHandle& a_refHandle, Conditions::IStateData* a_data) :
		_refHandle(a_refHandle), _data(a_data)
	{}

	[[nodiscard]] RE::ObjectRefHandle GetRefHandle() const { return _refHandle; }

	[[nodiscard]] Conditions::IStateData* AccessData(RE::hkbClipGenerator* a_clipGenerator) const;

	void KeepAlive() const
	{
		_timeSinceLastAccess = 0.f;
	}

	void OnLoopOrEcho(ActiveClip* a_clipGenerator, bool a_bIsEcho);

	[[nodiscard]] bool ShouldResetOnLoopOrEcho(ActiveClip* a_activeClip, bool a_bIsEcho) const;

	[[nodiscard]] bool Update(float a_deltaTime)
	{
		if (_lastUpdateTimestamp != g_durationOfApplicationRunTimeMS) {  // only run if last update was not this frame
			_lastUpdateTimestamp = g_durationOfApplicationRunTimeMS;
			if (!_data->Update(a_deltaTime)) {
				if (_timeSinceLastAccess > Settings::fStateDataLifetime) {  // expired
					return false;
				}
			}

			_timeSinceLastAccess += a_deltaTime;
		}

		return true;
	}

private:
	bool CheckRelevantClips(ActiveClip* a_activeClip) const;

	RE::ObjectRefHandle _refHandle;
	std::unique_ptr<Conditions::IStateData> _data;
	mutable std::list<std::weak_ptr<ActiveClip>> _relevantClips{};

	uint32_t _lastUpdateTimestamp = 0;
	mutable float _timeSinceLastAccess = 0.f;
};

// Equality functor template
template <typename T>
struct KeyEqual
{
	using is_transparent = void;

	bool operator()(const T& lhs, const T& rhs) const
	{
		return lhs == rhs;
	}

	// Special equality support for std::string_view to enable heterogeneous lookup
	bool operator()(std::string_view lhs, std::string_view rhs) const
	{
		return lhs == rhs;
	}
};

// Specialization for a pair with string, enabling heterogeneous lookup
template <typename T>
struct KeyHash<std::pair<T, std::string>>
{
	using is_transparent = void;

	size_t operator()(const std::pair<T, std::string>& a_key) const
	{
		std::size_t seed = 0;
		boost::hash_combine(seed, a_key.first);
		boost::hash_combine(seed, a_key.second);
		return seed;
	}

	size_t operator()(const std::pair<T, std::string_view>& a_key) const
	{
		std::size_t seed = 0;
		boost::hash_combine(seed, a_key.first);
		boost::hash_combine(seed, a_key.second);
		return seed;
	}
};

template <typename T>
struct KeyEqual<std::pair<T, std::string>>
{
	using is_transparent = void;

	bool operator()(const std::pair<T, std::string>& lhs, const std::pair<T, std::string>& rhs) const
	{
		return lhs.first == rhs.first && lhs.second == rhs.second;
	}

	bool operator()(const std::pair<T, std::string>& lhs, const std::pair<T, std::string_view>& rhs) const
	{
		return lhs.first == rhs.first && lhs.second == rhs;
	}

	bool operator()(const std::pair<T, std::string_view>& lhs, const std::pair<T, std::string>& rhs) const
	{
		return lhs.first == rhs.first && lhs.second == rhs.second;
	}
};

class IStateDataContainerHolder
{
public:
	virtual ~IStateDataContainerHolder() = default;
	virtual bool StateDataUpdate(float a_deltaTime) = 0;
	virtual bool StateDataOnLoopOrEcho(RE::ObjectRefHandle a_refHandle, ActiveClip* a_activeClip, bool a_bIsEcho) = 0;
	virtual bool StateDataClearRefrData(RE::ObjectRefHandle a_refHandle) = 0;
	virtual void StateDataClearData() = 0;

	void RegisterStateDataContainer();
	void UnregisterStateDataContainer();
};

template <typename T = void>
class StateDataContainer
{
public:
	using Key = std::conditional_t<std::is_same_v<T, void>, RE::ObjectRefHandle, std::pair<RE::ObjectRefHandle, T>>;

	size_t GetDataCount() const
	{
		ReadLocker locker(_stateDataLock);

		return _stateData.size();
	}

	bool UpdateData(float a_deltaTime)
	{
		WriteLocker locker(_stateDataLock);

		for (auto it = _stateData.begin(); it != _stateData.end();) {
			auto& entry = it->second;
			if (entry.Update(a_deltaTime)) {
				++it;
			} else {
				_keyMap.erase(entry.GetRefHandle());
				it = _stateData.erase(it);
			}
		}

		return !_stateData.empty();
	}

	// version used in variants. Propagates a set of active keys that will be used to keep the state data alive
	bool UpdateData(float a_deltaTime, std::unordered_set<Key>& a_outActiveKeys)
	{
		WriteLocker locker(_stateDataLock);

		for (auto it = _stateData.begin(); it != _stateData.end();) {
			auto& entry = it->second;
			if (entry.Update(a_deltaTime)) {
				a_outActiveKeys.emplace(it->first);
				++it;
			} else if (a_outActiveKeys.contains(it->first)) {
				++it;
			} else {
				_keyMap.erase(entry.GetRefHandle());
				it = _stateData.erase(it);
			}
		}

		return !_stateData.empty();
	}

	bool OnLoopOrEcho(RE::ObjectRefHandle a_refHandle, ActiveClip* a_activeClip, bool a_bIsEcho)
	{
		WriteLocker locker(_stateDataLock);

		if (const auto keySearch = _keyMap.find(a_refHandle); keySearch != _keyMap.end()) {
			for (auto& key : keySearch->second) {
				if (const auto search = _stateData.find(key); search != _stateData.end()) {
					search->second.OnLoopOrEcho(a_activeClip, a_bIsEcho);
					if (search->second.ShouldResetOnLoopOrEcho(a_activeClip, a_bIsEcho)) {
						_stateData.erase(search);
					}
				}
			}
		}

		return !_stateData.empty();
	}

	bool ClearRefrData(RE::ObjectRefHandle a_refHandle)
	{
		WriteLocker locker(_stateDataLock);

		if (const auto keySearch = _keyMap.find(a_refHandle); keySearch != _keyMap.end()) {
			for (auto& key : keySearch->second) {
				if (const auto search = _stateData.find(key); search != _stateData.end()) {
					_stateData.erase(search);
				}
			}
		}

		return !_stateData.empty();
	}

	void Clear()
	{
		WriteLocker locker(_stateDataLock);

		_stateData.clear();
	}

	Conditions::IStateData* AccessStateData(Key a_key, RE::hkbClipGenerator* a_clipGenerator) const
	{
		ReadLocker locker(_stateDataLock);

		const auto search = _stateData.find(a_key);
		if (search != _stateData.end()) {
			return search->second.AccessData(a_clipGenerator);
		}

		return nullptr;
	}

	void KeepStateDataAlive(Key a_key) const
	{
		ReadLocker locker(_stateDataLock);

		const auto search = _stateData.find(a_key);
		if (search != _stateData.end()) {
			search->second.KeepAlive();
		}
	}

	template <typename KeyType = Key>
	std::enable_if_t<std::is_same_v<KeyType, std::pair<RE::ObjectRefHandle, T>>, Conditions::IStateData*> AddStateData(Key a_key, Conditions::IStateData* a_stateData, RE::hkbClipGenerator* a_clipGenerator)
	{
		WriteLocker locker(_stateDataLock);

		const auto [it, bSuccess] = _stateData.try_emplace(a_key, a_key.first, a_stateData);
		if (bSuccess) {
			_keyMap[a_key.first].emplace(a_key);
			return it->second.AccessData(a_clipGenerator);
		}

		return nullptr;
	}

	template <typename KeyType = Key>
	std::enable_if_t<std::is_same_v<KeyType, RE::ObjectRefHandle>, Conditions::IStateData*> AddStateData(RE::ObjectRefHandle a_key, Conditions::IStateData* a_stateData, RE::hkbClipGenerator* a_clipGenerator)
	{
		WriteLocker locker(_stateDataLock);

		const auto [it, bSuccess] = _stateData.try_emplace(a_key, a_key, a_stateData);
		if (bSuccess) {
			_keyMap[a_key].emplace(a_key);
			return it->second.AccessData(a_clipGenerator);
		}

		return nullptr;
	}

protected:
	mutable SharedLock _stateDataLock;
	std::unordered_map<Key, StateDataContainerEntry, KeyHash<Key>, KeyEqual<Key>> _stateData{};
	std::unordered_map<RE::ObjectRefHandle, std::unordered_set<Key, KeyHash<Key>, KeyEqual<Key>>> _keyMap{};
};
