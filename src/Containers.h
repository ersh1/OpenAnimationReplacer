#pragma once
#include "DetectedProblems.h"
#include "Offsets.h"
#include "Settings.h"

#include "API/OpenAnimationReplacer-SharedTypes.h"

#include <unordered_set>
#include <rapidjson/document.h>

class ActiveClip;
class SubMod;

template <typename T, typename Derived>
class Set
{
public:
	using Callback = std::function<void()>;

	Set() = default;

	Set(SubMod* a_parentSubMod) :
		_parentSubMod(a_parentSubMod) {}

	bool IsEmpty() const { return _entries.empty(); }

	bool IsDirty() const { return _bDirty; }

	void SetDirty(bool a_bDirty)
	{
		_bDirty = a_bDirty;

		for (auto& callback : _onDirtyCallbacks) {
			callback();
		}
	}

	bool IsValid() const
	{
		ReadLocker locker(_lock);

		return std::ranges::all_of(_entries, [&](auto& entry) { return entry->IsValid(); });
	}

	RE::BSVisit::BSVisitControl ForEach(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<T>&)>& a_func)
	{
		using Result = RE::BSVisit::BSVisitControl;

		ReadLocker locker(_lock);

		auto result = Result::kContinue;

		for (auto& entry : _entries) {
			result = a_func(entry);
			if (result == Result::kStop) {
				return result;
			}
		}

		return result;
	}

	void Add(std::unique_ptr<T>& a_object, bool a_bSetDirty = false)
	{
		if (!a_object) {
			return;
		}

		{
			WriteLocker locker(_lock);
			auto& entry = _entries.emplace_back(std::move(a_object));
			SetAsParent(entry);
		}

		if (a_bSetDirty) {
			SetDirty(true);
		}
	}

	void Remove(const std::unique_ptr<T>& a_object)
	{
		if (!a_object) {
			return;
		}

		{
			WriteLocker locker(_lock);
			std::erase(_entries, a_object);
		}

		SetDirty(true);

		auto& detectedProblems = DetectedProblems::GetSingleton();
		if (detectedProblems.HasSubModsWithInvalidEntries()) {
			detectedProblems.CheckForSubModsWithInvalidEntries();
		}
	}

	std::unique_ptr<T> Extract(std::unique_ptr<T>& a_object)
	{
		WriteLocker locker(_lock);

		auto extracted = std::move(a_object);
		std::erase(_entries, a_object);

		return extracted;
	}

	void Insert(std::unique_ptr<T>& a_objectToInsert, const std::unique_ptr<T>& a_insertAfter, bool a_bSetDirty = false)
	{
		if (!a_objectToInsert) {
			return;
		}

		WriteLocker locker(_lock);

		if (a_bSetDirty) {
			SetDirty(true);
		}

		if (a_insertAfter) {
			if (const auto it = std::ranges::find(_entries, a_insertAfter); it != _entries.end()) {
				const auto entry = _entries.insert(it + 1, std::move(a_objectToInsert));
				SetAsParent(*entry);
				return;
			}
		}

		auto& entry = _entries.emplace_back(std::move(a_objectToInsert));
		SetAsParent(entry);
	}

	void Replace(std::unique_ptr<T>& a_objectToSubstitute, std::unique_ptr<T>& a_newObject)
	{
		if (a_objectToSubstitute == nullptr || a_newObject == nullptr) {
			return;
		}

		if (a_objectToSubstitute == a_newObject) {
			return;  // same object - nothing to do
		}

		{
			WriteLocker locker(_lock);
			a_objectToSubstitute = std::move(a_newObject);
		}

		SetDirty(true);

		auto& detectedProblems = DetectedProblems::GetSingleton();
		if (detectedProblems.HasSubModsWithInvalidEntries()) {
			detectedProblems.CheckForSubModsWithInvalidEntries();
		}
	}

	void Move(std::unique_ptr<T>& a_sourceObject, Set<T, Derived>* a_sourceSet, const std::unique_ptr<T>& a_targetObject, bool a_bInsertAfter = false)
	{
		if (a_sourceObject == a_targetObject) {
			return;  // same object - nothing to do
		}

		if (a_sourceSet == this) {
			// move within the same set
			WriteLocker locker(_lock);

			const auto sourceIndex = std::distance(_entries.begin(), std::ranges::find(_entries, a_sourceObject));
			auto targetIndex = std::distance(_entries.begin(), std::ranges::find(_entries, a_targetObject));

			if (a_bInsertAfter) {
				++targetIndex;
			}

			if (sourceIndex < targetIndex) {
				--targetIndex;
			}

			if (sourceIndex == targetIndex) {
				return;
			}

			auto extractedObject = std::move(a_sourceObject);
			_entries.erase(_entries.begin() + sourceIndex);
			_entries.insert(_entries.begin() + targetIndex, std::move(extractedObject));

			SetDirty(true);
		} else {
			// move from other set

			// make sure the source object is not an ancestor of this set
			if (IsChildOf(a_sourceObject.get())) {
				return;
			}

			WriteLocker locker(_lock);
			const auto targetIt = std::ranges::find(_entries, a_targetObject);
			auto extractedObject = a_sourceSet->Extract(a_sourceObject);
			if (targetIt != _entries.end()) {
				if (a_bInsertAfter) {
					const auto& entry = _entries.insert(targetIt + 1, std::move(extractedObject));
					SetAsParent(*entry);
				} else {
					const auto& entry = _entries.insert(targetIt, std::move(extractedObject));
					SetAsParent(*entry);
				}
			} else {
				auto& entry = _entries.emplace_back(std::move(extractedObject));
				SetAsParent(entry);
			}

			SetDirty(true);
		}
	}

	void MoveAll(Set<T, Derived>* a_otherSet)
	{
		WriteLocker locker(_lock);

		a_otherSet->ForEach([this](auto& entry) {
			SetAsParent(entry);
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		_entries = std::move(a_otherSet->_entries);
	}

	void Append(Set<T, Derived>* a_otherSet)
	{
		WriteLocker locker(_lock);

		a_otherSet->ForEach([this](auto& entry) {
			SetAsParent(entry);
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		WriteLocker otherLocker(a_otherSet->_lock);

		_entries.reserve(_entries.size() + a_otherSet->_entries.size());

		_entries.insert(_entries.end(), std::make_move_iterator(a_otherSet->_entries.begin()), std::make_move_iterator(a_otherSet->_entries.end()));

		SetDirty(true);
	}

	void Clear()
	{
		{
			WriteLocker locker(_lock);
			_entries.clear();
		}

		SetDirty(true);

		auto& detectedProblems = DetectedProblems::GetSingleton();
		if (detectedProblems.HasSubModsWithInvalidEntries()) {
			detectedProblems.CheckForSubModsWithInvalidEntries();
		}
	}

	void SetAsParent(std::unique_ptr<T>& a_object)
	{
		static_cast<Derived*>(this)->SetAsParentImpl(a_object);
	}

	rapidjson::Value Serialize(rapidjson::Document::AllocatorType& a_allocator)
	{
		rapidjson::Value entryArrayValue(rapidjson::kArrayType);

		ForEach([&](auto& entry) {
			rapidjson::Value entryValue(rapidjson::kObjectType);
			entry->Serialize(static_cast<void*>(&entryValue), static_cast<void*>(&a_allocator));
			entryArrayValue.PushBack(entryValue, a_allocator);

			return RE::BSVisit::BSVisitControl::kContinue;
		});

		return entryArrayValue;
	}

	bool IsChildOf(T* a_object)
	{
		return static_cast<Derived*>(this)->IsChildOfImpl(a_object);
	}

	size_t Num() const { return _entries.size(); }

	[[nodiscard]] std::string NumText() const
	{
		return static_cast<const Derived*>(this)->NumTextImpl();
	}

	[[nodiscard]] SubMod* GetParentSubMod() const
	{
		return static_cast<const Derived*>(this)->GetParentSubModImpl();
	}

	[[nodiscard]] bool IsDirtyRecursive() const 
	{
		return static_cast<const Derived*>(this)->IsDirtyRecursiveImpl();
	}

	void SetDirtyRecursive(bool a_bDirty)
	{
		static_cast<Derived*>(this)->SetDirtyRecursiveImpl(a_bDirty);
	}

	void AddOnDirtyCallback(Callback a_callback) 
	{
		_onDirtyCallbacks.push_back(a_callback);
	}

	void RemoveOnDirtyCallback(Callback a_callback)
	{
		_onDirtyCallbacks.erase(a_callback);
	}

protected:
	mutable SharedLock _lock;
	std::vector<std::unique_ptr<T>> _entries;
	std::vector<Callback> _onDirtyCallbacks;
	bool _bDirty = false;

	SubMod* _parentSubMod = nullptr;
};

class StateDataContainerEntry
{
public:
	StateDataContainerEntry(const RE::ObjectRefHandle& a_refHandle, IStateData* a_data) :
		_refHandle(a_refHandle), _data(a_data)
	{}

	[[nodiscard]] RE::ObjectRefHandle GetRefHandle() const { return _refHandle; }

	[[nodiscard]] IStateData* AccessData(RE::hkbClipGenerator* a_clipGenerator);

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
			if (!_data->Update(a_deltaTime) && !HasAnyActiveClips() && _timeSinceLastAccess > Settings::fStateDataLifetime) {  // expired
				return false;
			}

			_timeSinceLastAccess += a_deltaTime;
		}

		return true;
	}

private:
	bool CheckRelevantClips(ActiveClip* a_activeClip) const;
	bool HasAnyActiveClips() const;

	void AddActiveClip(std::shared_ptr<ActiveClip>& a_activeClip);

	RE::ObjectRefHandle _refHandle;
	std::unique_ptr<IStateData> _data;

	mutable SharedLock _clipsLock;
	std::set<std::weak_ptr<ActiveClip>, std::owner_less<std::weak_ptr<ActiveClip>>> _activeClips{};
	std::set<std::weak_ptr<ActiveClip>, std::owner_less<std::weak_ptr<ActiveClip>>> _relevantClips{};

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

	IStateData* AccessStateData(Key a_key, RE::hkbClipGenerator* a_clipGenerator)
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
	std::enable_if_t<std::is_same_v<KeyType, std::pair<RE::ObjectRefHandle, T>>, IStateData*> AddStateData(Key a_key, IStateData* a_stateData, RE::hkbClipGenerator* a_clipGenerator)
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
	std::enable_if_t<std::is_same_v<KeyType, RE::ObjectRefHandle>, IStateData*> AddStateData(RE::ObjectRefHandle a_key, IStateData* a_stateData, RE::hkbClipGenerator* a_clipGenerator)
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
