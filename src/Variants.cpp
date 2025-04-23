#include "Variants.h"

#include "ActiveClip.h"
#include "OpenAnimationReplacer.h"
#include "ReplacerMods.h"
#include "Utils.h"

bool Variant::ShouldSaveToJson(VariantMode a_variantMode) const
{
	if (_bDisabled) {
		return true;
	}

	if (_bPlayOnce) {
		return true;
	}

	if (a_variantMode == VariantMode::kRandom && _weight != 1.f) {
		return true;
	}

	if (a_variantMode == VariantMode::kSequential) {
		return true;
	}

	return false;
}

void Variant::ResetSettings()
{
	_weight = 1.f;
	_bDisabled = false;
	_bPlayOnce = false;
}

uint16_t Variants::GetVariantIndex(Variant*& a_outVariant) const
{
	// no active clip, so only supports random variants
	const float randomWeight = Utils::GetRandomFloat(0.f, 1.f);

	return GetVariantIndex(a_outVariant, randomWeight);
}

uint16_t Variants::GetVariantIndex(Variant*& a_outVariant, float a_randomWeight) const
{
	ReadLocker locker(_lock);

	const auto it = std::ranges::lower_bound(_cumulativeWeights, a_randomWeight);
	const auto i = std::distance(_cumulativeWeights.begin(), it);
	a_outVariant = _activeVariants[i];
	return a_outVariant->GetIndex();
}

uint16_t Variants::GetVariantIndex(ActiveClip* a_activeClip, Variant*& a_outVariant) const
{
	const auto stateData = GetVariantStateData(a_activeClip);

	switch (_variantMode) {
	default:
	case VariantMode::kRandom:
		{
			ReadLocker locker(_lock);

			if (!_sequentialVariants.empty()) {
				auto nextSequentialVariant = stateData->GetNextSequentialVariant(a_activeClip->GetClipGenerator(), this);
				if (!stateData->HasPlayedOnce(a_activeClip->GetClipGenerator(), nextSequentialVariant, this)) {
					a_outVariant = _sequentialVariants[nextSequentialVariant];
					return a_outVariant->GetIndex();
				}
			}

			const float randomWeight = stateData->GetRandomFloat();

			const auto it = std::ranges::lower_bound(_cumulativeWeights, randomWeight);
			const auto i = std::distance(_cumulativeWeights.begin(), it);
			a_outVariant = _randomVariants[i];
			return a_outVariant->GetIndex();
		}
	case VariantMode::kSequential:
		{
			a_outVariant = _sequentialVariants[stateData->GetNextSequentialVariant(a_activeClip->GetClipGenerator(), this)];
			return a_outVariant->GetIndex();
		}
	}
}

void Variants::UpdateVariantCache()
{
	WriteLocker locker(_lock);

	_activeVariants.clear();
	_sequentialVariants.clear();
	_randomVariants.clear();

	switch (_variantMode) {
	case VariantMode::kRandom:
		{
			_totalWeight = 0.f;
			_cumulativeWeights.clear();
			float maxWeight = 0.f;

			std::ranges::sort(_variants, [](const Variant& a, const Variant& b) {
				if (a.ShouldPlayOnce() && b.ShouldPlayOnce()) {
					return a.GetOrder() < b.GetOrder();
				}
				if (a.ShouldPlayOnce() != b.ShouldPlayOnce()) {
					return a.ShouldPlayOnce();
				}
				return a.GetWeight() > b.GetWeight();
			});

			for (auto& variant : _variants) {
				if (!variant.IsDisabled()) {
					_activeVariants.emplace_back(&variant);
					if (variant.ShouldPlayOnce()) {
						_sequentialVariants.emplace_back(&variant);
					} else {
						_totalWeight += variant.GetWeight();
						maxWeight = std::max(maxWeight, variant.GetWeight());
						_randomVariants.emplace_back(&variant);
					}
				}
			}

			// cache
			float weightSum = 0.f;

			for (const auto randomVariant : _randomVariants) {
				const float normalizedWeight = randomVariant->GetWeight();
				weightSum += normalizedWeight;
				_cumulativeWeights.emplace_back(weightSum / _totalWeight);
			}
		}
		break;

	case VariantMode::kSequential:
		std::ranges::sort(_variants, [](const Variant& a, const Variant& b) {
			return a.GetOrder() < b.GetOrder();
		});

		for (auto& variant : _variants) {
			if (!variant.IsDisabled()) {
				_activeVariants.emplace_back(&variant);
				_sequentialVariants.emplace_back(&variant);
			}
		}

		break;
	}
}

void Variants::ResetSettings()
{
	_bShouldBlendBetweenVariants = true;
	_bShouldResetRandomOnLoopOrEcho = true;
	_variantMode = VariantMode::kRandom;
	_variantStateScope = Conditions::StateDataScope::kLocal;

	for (Variant& variant : _variants) {
		variant.ResetSettings();
	}

	UpdateVariantCache();
}

VariantStateData* Variants::TryGetVariantStateData(ActiveClip* a_activeClip) const
{
	return OpenAnimationReplacer::GetSingleton().GetVariantStateData(a_activeClip->GetRefr(), this, a_activeClip);
}

VariantStateData* Variants::GetVariantStateData(ActiveClip* a_activeClip) const
{
	auto stateData = TryGetVariantStateData(a_activeClip);
	if (!stateData) {  // create new
		stateData = CreateVariantStateData(a_activeClip);
	}

	return stateData;
}

SubMod* Variants::GetParentSubMod() const
{
	return GetParentReplacementAnimation()->GetParentSubMod();
}

RE::BSVisit::BSVisitControl Variants::ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(Variant&)>& a_func)
{
	using Result = RE::BSVisit::BSVisitControl;

	for (auto& variant : _variants) {
		const auto result = a_func(variant);
		if (result == Result::kStop) {
			return result;
		}
	}

	return Result::kContinue;
}

RE::BSVisit::BSVisitControl Variants::ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(const Variant&)>& a_func) const
{
	using Result = RE::BSVisit::BSVisitControl;

	for (auto& variant : _variants) {
		const auto result = a_func(variant);
		if (result == Result::kStop) {
			return result;
		}
	}

	return Result::kContinue;
}

void Variants::SwapVariants(int32_t a_variantIndexA, int32_t a_variantIndexB)
{
	if (a_variantIndexA < _variants.size() && a_variantIndexB < _variants.size()) {
		_variants[a_variantIndexA].SetOrder(a_variantIndexB);
		_variants[a_variantIndexB].SetOrder(a_variantIndexA);
	}

	UpdateVariantCache();
}

size_t Variants::GetActiveVariantCount() const
{
	ReadLocker locker(_lock);

	return _activeVariants.size();
}

size_t Variants::GetSequentialVariantCount() const
{
	ReadLocker locker(_lock);

	return _sequentialVariants.size();
}

Variant* Variants::GetActiveVariant(size_t a_variantIndex) const
{
	ReadLocker locker(_lock);

	if (a_variantIndex < _activeVariants.size()) {
		return _activeVariants[a_variantIndex];
	}

	return nullptr;
}

VariantStateData* Variants::CreateVariantStateData(ActiveClip* a_activeClip) const
{
	const auto newStateData = static_cast<VariantStateData*>(VariantStateData::Create());
	newStateData->Initialize(this, a_activeClip);
	return OpenAnimationReplacer::GetSingleton().AddVariantStateData(newStateData, a_activeClip->GetRefr(), this, a_activeClip);
}

void VariantStateData::Initialize(const Variants* a_variants, ActiveClip* a_activeClip)
{
	_bShouldResetRandomOnLoopOrEcho = a_variants->ShouldResetRandomOnLoopOrEcho();
	if (a_variants->GetVariantStateScope() > Conditions::StateDataScope::kLocal) {
		_bIsLocalScope = false;
		auto ptr = std::weak_ptr(a_activeClip->weak_from_this());
		_clipPriorityQueue.push(ptr);
	}
}

bool VariantStateData::Update([[maybe_unused]] float a_deltaTime)
{
	if (!IsLocalScope()) {
		ReadLocker locker(_dataLock);

		for (auto& entry : _activeVariants | std::views::values) {
			if (entry.bActive) {
				entry.OnActive();
			} else if (entry.timeInactive > Settings::fStateDataLifetime) {
				entry.OnExpire();
			} else {
				entry.timeInactive += +a_deltaTime;
			}
		}
	}

	return IsActive();
}

void VariantStateData::OnLoopOrEcho(RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] bool a_bIsEcho)
{
	if (!_bShouldResetRandomOnLoopOrEcho) {
		return;
	}

	// only reset the random float if we're in random mode, don't reset the sequential data
	if (auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_clipGenerator)) {
		if (auto replacementAnimation = activeClip->GetReplacementAnimation()) {
			if (replacementAnimation->HasVariants()) {
				auto& variants = replacementAnimation->GetVariants();

				if (!CheckLeadingClip(&variants, activeClip)) {
					return;
				}

				_randomFloat.reset();
			}
		}
	}
}

bool VariantStateData::ShouldResetOnLoopOrEcho([[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] bool a_bIsEcho) const
{
	// always return false because we don't want to reset the entire thing (playonce/sequence history etc.)
	// the random float is reset in OnLoopOrEcho if necessary
	return false;
}

bool VariantStateData::IsActive() const
{
	ReadLocker locker(_dataLock);

	for (auto& entry : _activeVariants | std::views::values) {
		if (entry.bActive) {
			return true;
		}
	}

	return false;
}

void VariantStateData::SetActiveVariant(RE::hkbClipGenerator* a_clipGenerator, bool a_bActive)
{
	if (a_bActive) {
		WriteLocker locker(_dataLock);

		auto [it, bAdded] = _activeVariants.try_emplace(a_clipGenerator->name.data(), VariantClipData());
		if (!bAdded) {
			// already exists, set active and reset the timer
			it->second.timeInactive = 0.f;
			it->second.bActive = true;
		}
	} else {
		ReadLocker locker(_dataLock);

		auto search = _activeVariants.find(a_clipGenerator->name.data());
		if (search != _activeVariants.end()) {
			search->second.bActive = false;
		}
	}
}

float VariantStateData::GetRandomFloat()
{
	if (!_randomFloat.has_value()) {
		_randomFloat = Utils::GetRandomFloat(0.f, 1.f);
	}

	return *_randomFloat;
}

bool VariantStateData::HasPlayedOnce(RE::hkbClipGenerator* a_clipGenerator, size_t a_index, const Variants* a_variants) const
{
	ReadLocker locker(_dataLock);

	if (a_variants->ShouldSharePlayedHistory()) {
		if (_sharedPlayedHistory.size() > a_index) {
			return _sharedPlayedHistory[a_index].HasPlayed();
		}
	} else {
		auto search = _activeVariants.find(a_clipGenerator->name.data());
		if (search != _activeVariants.end()) {
			if (search->second.playedHistory.size() > a_index) {
				return search->second.playedHistory[a_index];
			}
		}
	}

	return false;
}

size_t VariantStateData::GetNextSequentialVariant(RE::hkbClipGenerator* a_clipGenerator, const Variants* a_variants)
{
	ReadLocker locker(_dataLock);

	auto search = _activeVariants.find(a_clipGenerator->name.data());
	if (search != _activeVariants.end()) {
		auto& variantClipData = search->second;
		variantClipData.nextSequentialVariant = variantClipData.nextSequentialVariant % a_variants->GetSequentialVariantCount();

		if (a_variants->ShouldSharePlayedHistory()) {
			if (a_variants->GetActiveVariant(variantClipData.nextSequentialVariant)->ShouldPlayOnce()) {
				for (size_t i = variantClipData.nextSequentialVariant; i < a_variants->GetSequentialVariantCount(); ++i) {
					if (!a_variants->GetActiveVariant(i)->ShouldPlayOnce() || _sharedPlayedHistory.size() <= i || !_sharedPlayedHistory[i].HasPlayed()) {
						variantClipData.nextSequentialVariant = i;
						return variantClipData.nextSequentialVariant;
					}
				}

				// if we're here, all sequential variants have been played, and we should clear the history
				variantClipData.playedHistory.clear();
				variantClipData.nextSequentialVariant = 0;
			}
		}

		return variantClipData.nextSequentialVariant;
	}

	return 0;
}

bool VariantStateData::IsAtBeginningOfSequence(RE::hkbClipGenerator* a_clipGenerator, const Variants* a_variants) const
{
	ReadLocker locker(_dataLock);

	auto search = _activeVariants.find(a_clipGenerator->name.data());
	if (search != _activeVariants.end()) {
		auto& variantClipData = search->second;
		const auto nextSequentialVariant = variantClipData.nextSequentialVariant;
		if (nextSequentialVariant == 0) {
			return true;
		}

		if (a_variants) {
			for (int i = 0; i < nextSequentialVariant; ++i) {
				if (!a_variants->GetActiveVariant(i)->ShouldPlayOnce()) {
					return false;
				}
			}

			return true;
		}
	}

	return false;
}

bool VariantStateData::CheckLeadingClip(const Variants* a_variants, ActiveClip* a_activeClip) const
{
	if (a_variants->GetVariantStateScope() > Conditions::StateDataScope::kLocal) {
		if (a_activeClip) {
			// remove invalid pointers
			while (!_clipPriorityQueue.empty()) {
				if (!_clipPriorityQueue.front().lock()) {
					_clipPriorityQueue.pop();
				} else {
					break;
				}
			}

			_clipPriorityQueue.push(a_activeClip->getptr());

			if (!_clipPriorityQueue.empty()) {
				if (auto leadingClip = _clipPriorityQueue.front().lock()) {
					return leadingClip.get() == a_activeClip;
				}
			}
		}
	}

	return true;
}

void VariantStateData::OnStartVariant(const Variants* a_variants, Variant* a_variant, ActiveClip* a_activeClip)
{
	SetActiveVariant(a_activeClip->GetClipGenerator(), true);

	/*if (!CheckLeadingClip(a_variants, a_activeClip)) {
		return;
	}*/

	if (a_variant->ShouldPlayOnce()) {
		ReadLocker locker(_dataLock);

		auto search = _activeVariants.find(a_activeClip->GetClipGenerator()->name.data());
		if (search != _activeVariants.end()) {
			auto& variantClipData = search->second;
			auto nextSequentialVariant = variantClipData.nextSequentialVariant;

			if (a_variants->ShouldSharePlayedHistory()) {
				if (_sharedPlayedHistory.empty()) {
					// initialize the vector
					_sharedPlayedHistory.resize(std::max(nextSequentialVariant + 1, a_variants->GetSequentialVariantCount()));
				} else if (_sharedPlayedHistory.size() <= nextSequentialVariant) {
					// resize the vector
					_sharedPlayedHistory.resize(nextSequentialVariant + 1);
				}

				_sharedPlayedHistory[nextSequentialVariant].SetPlayed();
			} else {
				if (variantClipData.playedHistory.empty()) {
					// initialize the vector
					variantClipData.playedHistory.resize(std::max(nextSequentialVariant + 1, a_variants->GetSequentialVariantCount()), false);
				} else if (variantClipData.playedHistory.size() <= nextSequentialVariant) {
					// resize the vector
					variantClipData.playedHistory.resize(nextSequentialVariant + 1, false);
				}

				variantClipData.playedHistory[nextSequentialVariant] = true;
			}
		}
	}

	if (a_variants->GetVariantMode() == VariantMode::kSequential || a_variant->ShouldPlayOnce()) {
		IterateSequence(a_variants, a_activeClip);
	}
}

void VariantStateData::OnEndVariant([[maybe_unused]] const Variants* a_variants, ActiveClip* a_activeClip)
{
	SetActiveVariant(a_activeClip->GetClipGenerator(), false);
}

void VariantStateData::IterateSequence(const Variants* a_variants, ActiveClip* a_activeClip)
{
	const auto sequentialVariantCount = a_variants->GetSequentialVariantCount();

	ReadLocker locker(_dataLock);

	auto search = _activeVariants.find(a_activeClip->GetClipGenerator()->name.data());
	if (search != _activeVariants.end()) {
		auto& variantClipData = search->second;

		//iterate the index
		++variantClipData.nextSequentialVariant;

		const auto iteratedVariant = variantClipData.nextSequentialVariant;
		for (size_t i = 0; i < sequentialVariantCount; ++i) {
			variantClipData.nextSequentialVariant = (iteratedVariant + i) % sequentialVariantCount;  // wrap around if necessary

			if (!a_variants->GetActiveVariant(variantClipData.nextSequentialVariant)->ShouldPlayOnce()) {
				return;
			}

			if (a_variants->ShouldSharePlayedHistory()) {
				if (_sharedPlayedHistory.size() <= variantClipData.nextSequentialVariant || !_sharedPlayedHistory[variantClipData.nextSequentialVariant].HasPlayed()) {
					return;
				}
			} else if (variantClipData.playedHistory.size() <= variantClipData.nextSequentialVariant || !variantClipData.playedHistory[variantClipData.nextSequentialVariant]) {
				return;
			}
		}

		if (a_variants->GetVariantMode() == VariantMode::kSequential) {
			// if we're here, all sequential variants have been played, and we should clear the history
			variantClipData.playedHistory.clear();
			variantClipData.nextSequentialVariant = 0;
		}
	}
}

bool VariantStateDataContainer::UpdateData(float a_deltaTime)
{
	bool bActive = false;

	std::unordered_set<RE::ObjectRefHandle> activeKeys{};

	{
		WriteLocker locker(_localMapLock);
		for (auto it = _localVariantStateData.begin(); it != _localVariantStateData.end();) {
			auto& entry = it->second;
			if (entry.UpdateData(a_deltaTime, activeKeys)) {
				++it;
				bActive = true;
			} else {
				it = _localVariantStateData.erase(it);
			}
		}
	}

	{
		WriteLocker locker(_subModMapLock);
		for (auto it = _subModVariantStateData.begin(); it != _subModVariantStateData.end();) {
			auto& entry = it->second;
			if (entry.UpdateData(a_deltaTime, activeKeys)) {
				++it;
				bActive = true;
			} else {
				it = _subModVariantStateData.erase(it);
			}
		}
	}

	if (_replacerModVariantStateData.UpdateData(a_deltaTime, activeKeys)) {
		bActive = true;
	}

	return bActive;
}

bool VariantStateDataContainer::OnLoopOrEcho(RE::ObjectRefHandle a_refHandle, ActiveClip* a_activeClip, bool a_bIsEcho)
{
	bool bActive = false;

	{
		WriteLocker locker(_localMapLock);
		for (auto it = _localVariantStateData.begin(); it != _localVariantStateData.end();) {
			auto& entry = it->second;
			if (entry.OnLoopOrEcho(a_refHandle, a_activeClip, a_bIsEcho)) {
				++it;
				bActive = true;
			} else {
				it = _localVariantStateData.erase(it);
			}
		}
	}

	{
		WriteLocker locker(_subModMapLock);
		for (auto it = _subModVariantStateData.begin(); it != _subModVariantStateData.end();) {
			auto& entry = it->second;
			if (entry.OnLoopOrEcho(a_refHandle, a_activeClip, a_bIsEcho)) {
				++it;
				bActive = true;
			} else {
				it = _subModVariantStateData.erase(it);
			}
		}
	}

	if (_replacerModVariantStateData.OnLoopOrEcho(a_refHandle, a_activeClip, a_bIsEcho)) {
		bActive = true;
	}

	return bActive;
}

bool VariantStateDataContainer::ClearRefrData(RE::ObjectRefHandle a_refHandle)
{
	bool bActive = false;

	{
		WriteLocker locker(_localMapLock);
		for (auto it = _localVariantStateData.begin(); it != _localVariantStateData.end();) {
			auto& entry = it->second;
			if (entry.ClearRefrData(a_refHandle)) {
				++it;
				bActive = true;
			} else {
				it = _localVariantStateData.erase(it);
			}
		}
	}

	{
		WriteLocker locker(_subModMapLock);
		for (auto it = _subModVariantStateData.begin(); it != _subModVariantStateData.end();) {
			auto& entry = it->second;
			if (entry.ClearRefrData(a_refHandle)) {
				++it;
				bActive = true;
			} else {
				it = _subModVariantStateData.erase(it);
			}
		}
	}

	if (_replacerModVariantStateData.ClearRefrData(a_refHandle)) {
		bActive = true;
	}

	return bActive;
}

void VariantStateDataContainer::Clear()
{
	{
		WriteLocker locker(_localMapLock);
		_localVariantStateData.clear();
	}

	{
		WriteLocker locker(_subModMapLock);
		_subModVariantStateData.clear();
	}
	_replacerModVariantStateData.Clear();
}

Conditions::IStateData* VariantStateDataContainer::AccessStateData(RE::ObjectRefHandle a_key, RE::hkbClipGenerator* a_clipGenerator, const Variants* a_variants)
{
	switch (a_variants->GetVariantStateScope()) {
	case Conditions::StateDataScope::kLocal:
		{
			ReadLocker locker(_localMapLock);
			if (auto search = _localVariantStateData.find(a_clipGenerator); search != _localVariantStateData.end()) {
				return search->second.AccessStateData(a_key, a_clipGenerator);
			}
		}
		break;
	case Conditions::StateDataScope::kSubMod:
		{
			ReadLocker locker(_subModMapLock);
			if (auto search = _subModVariantStateData.find(a_variants->GetParentSubMod()); search != _subModVariantStateData.end()) {
				return search->second.AccessStateData(a_key, a_clipGenerator);
			}
		}
		break;
	case Conditions::StateDataScope::kReplacerMod:
		return _replacerModVariantStateData.AccessStateData(a_key, a_clipGenerator);
	}

	return nullptr;
}

Conditions::IStateData* VariantStateDataContainer::AddStateData(RE::ObjectRefHandle a_key, Conditions::IStateData* a_stateData, RE::hkbClipGenerator* a_clipGenerator, const Variants* a_variants)
{
	switch (a_variants->GetVariantStateScope()) {
	case Conditions::StateDataScope::kLocal:
		{
			WriteLocker locker(_localMapLock);
			return _localVariantStateData[a_clipGenerator].AddStateData(a_key, a_stateData, a_clipGenerator);
		}
	case Conditions::StateDataScope::kSubMod:
		{
			WriteLocker locker(_subModMapLock);
			return _subModVariantStateData[a_variants->GetParentSubMod()].AddStateData(a_key, a_stateData, a_clipGenerator);
		}
	case Conditions::StateDataScope::kReplacerMod:
		return _replacerModVariantStateData.AddStateData(a_key, a_stateData, a_clipGenerator);
	}

	return nullptr;
}
