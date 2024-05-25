#pragma once

#include "API/OpenAnimationReplacer-ConditionTypes.h"
#include "Settings.h"
#include "StateDataContainer.h"
#include "Utils.h"

class ActiveClip;
class ReplacementAnimation;
class VariantStateData;

enum class VariantMode : uint8_t
{
	kRandom = 0,
	kSequential = 1
};

class Variant
{
public:
	Variant(uint16_t a_index, std::string_view a_filename, int32_t a_order) :
		_index(a_index),
		_filename(a_filename),
		_order(a_order)
	{}

	uint16_t GetIndex() const { return _index; }
	std::string_view GetFilename() const { return _filename; }
	bool IsDisabled() const { return _bDisabled; }
	void SetDisabled(bool a_bDisable) { _bDisabled = a_bDisable; }
	bool ShouldPlayOnce() const { return _bPlayOnce; }
	void SetPlayOnce(bool a_bPlayOnce) { _bPlayOnce = a_bPlayOnce; }

	float GetWeight() const { return _weight; }
	void SetWeight(float a_weight) { _weight = a_weight; }

	int32_t GetOrder() const { return _order; }
	void SetOrder(int32_t a_order) { _order = a_order; }

	bool ShouldSaveToJson(VariantMode a_variantMode) const;

	void ResetSettings();

protected:
	uint16_t _index = static_cast<uint16_t>(-1);
	std::string _filename;
	bool _bDisabled = false;
	bool _bPlayOnce = false;

	float _weight = 1.f;
	int32_t _order;
};

class Variants
{
public:
	Variants(std::vector<Variant>& a_variants) :
		_variants(std::move(a_variants))
	{
		UpdateVariantCache();
	}

	uint16_t GetVariantIndex(Variant*& a_outVariant) const;
	uint16_t GetVariantIndex(ActiveClip* a_activeClip, Variant*& a_outVariant) const;
	void UpdateVariantCache();

	void ResetSettings();

	[[nodiscard]] VariantStateData* TryGetVariantStateData(ActiveClip* a_activeClip) const;
	[[nodiscard]] VariantStateData* GetVariantStateData(ActiveClip* a_activeClip) const;

	[[nodiscard]] Conditions::StateDataScope GetVariantStateScope() const { return _variantStateScope; }
	void SetVariantStateScope(Conditions::StateDataScope a_scope) { _variantStateScope = a_scope; }

	[[nodiscard]] VariantMode GetVariantMode() const { return _variantMode; }
	void SetVariantMode(VariantMode a_variantMode) { _variantMode = a_variantMode; }

	[[nodiscard]] ReplacementAnimation* GetParentReplacementAnimation() const { return _parentReplacementAnimation; }
	[[nodiscard]] class SubMod* GetParentSubMod() const;

	bool ShouldBlendBetweenVariants() const { return _bShouldBlendBetweenVariants; }
	void SetShouldBlendBetweenVariants(bool a_bEnable) { _bShouldBlendBetweenVariants = a_bEnable; }

	bool ShouldResetRandomOnLoopOrEcho() const { return _bShouldResetRandomOnLoopOrEcho; }
	void SetShouldResetRandomOnLoopOrEcho(bool a_bEnable) { _bShouldResetRandomOnLoopOrEcho = a_bEnable; }

	bool ShouldSharePlayedHistory() const { return _bShouldSharePlayedHistory; }
	void SetShouldSharePlayedHistory(bool a_bEnable) { _bShouldSharePlayedHistory = a_bEnable; }

	RE::BSVisit::BSVisitControl ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(Variant&)>& a_func);
	RE::BSVisit::BSVisitControl ForEachVariant(const std::function<RE::BSVisit::BSVisitControl(const Variant&)>& a_func) const;

	void SwapVariants(int32_t a_variantIndexA, int32_t a_variantIndexB);

	size_t GetVariantCount() const { return _variants.size(); }
	size_t GetActiveVariantCount() const;
	size_t GetSequentialVariantCount() const;

	Variant* GetActiveVariant(size_t a_variantIndex) const;

protected:
	VariantStateData* CreateVariantStateData(ActiveClip* a_activeClip) const;

	friend class ReplacementAnimation;
	ReplacementAnimation* _parentReplacementAnimation = nullptr;
	std::vector<Variant> _variants;
	bool _bShouldBlendBetweenVariants = true;
	bool _bShouldResetRandomOnLoopOrEcho = true;
	bool _bShouldSharePlayedHistory = false;

	mutable SharedLock _lock;
	std::vector<Variant*> _activeVariants;
	std::vector<Variant*> _sequentialVariants;
	std::vector<Variant*> _randomVariants;

	// modes
	VariantMode _variantMode = VariantMode::kRandom;

	// random mode
	float _totalWeight = 0.f;
	std::vector<float> _cumulativeWeights;
	Conditions::StateDataScope _variantStateScope = Conditions::StateDataScope::kLocal;
};

struct VariantClipData
{
	void OnActive()
	{
		bActive = true;
		bExpired = false;
		timeInactive = 0.f;
	}

	void OnExpire()  // reset next sequential variant on expiration, but keep played history
	{
		bExpired = true;
		nextSequentialVariant = 0;
	}

	bool bActive = true;
	bool bExpired = false;
	size_t nextSequentialVariant = 0;
	float timeInactive = 0.f;
	std::vector<bool> playedHistory{};
};

struct SharedPlayedHistoryEntry
{
	bool HasPlayed() const
	{
		return _bPlayed && _timestamp != g_durationOfApplicationRunTimeMS;
	}

	void SetPlayed()
	{
		_bPlayed = true;
		_timestamp = g_durationOfApplicationRunTimeMS;
	}

private:
	bool _bPlayed = false;
	uint32_t _timestamp = 0;
};

class VariantStateData : public Conditions::IStateData
{
public:
	static IStateData* Create() { return new VariantStateData(); }

	void Initialize(const Variants* a_variants, ActiveClip* a_activeClip);

	bool Update(float a_deltaTime) override;
	void OnLoopOrEcho(RE::hkbClipGenerator* a_clipGenerator, bool a_bIsEcho) override;
	bool ShouldResetOnLoopOrEcho(RE::hkbClipGenerator* a_clipGenerator, bool a_bIsEcho) const override;

	bool IsActive() const;
	void SetActiveVariant(RE::hkbClipGenerator* a_clipGenerator, bool a_bActive);

	float GetRandomFloat();
	bool HasPlayedOnce(RE::hkbClipGenerator* a_clipGenerator, size_t a_index, const Variants* a_variants) const;
	size_t GetNextSequentialVariant(RE::hkbClipGenerator* a_clipGenerator, const Variants* a_variants);

	bool IsAtBeginningOfSequence(RE::hkbClipGenerator* a_clipGenerator, const Variants* a_variants) const;

	bool IsLocalScope() const { return _bIsLocalScope; }
	bool CheckLeadingClip(const Variants* a_variants, ActiveClip* a_activeClip) const;

	void OnStartVariant(const Variants* a_variants, Variant* a_variant, ActiveClip* a_activeClip);
	void OnEndVariant(const Variants* a_variants, ActiveClip* a_activeClip);
	void IterateSequence(const Variants* a_variants, ActiveClip* a_activeClip);

protected:
	mutable SharedLock _dataLock;
	std::unordered_map<std::string, VariantClipData, KeyHash<std::string>, KeyEqual<std::string>> _activeVariants{};
	std::vector<SharedPlayedHistoryEntry> _sharedPlayedHistory{};

	mutable Utils::UniqueQueue<std::weak_ptr<ActiveClip>> _clipPriorityQueue{};
	bool _bShouldResetRandomOnLoopOrEcho = false;
	bool _bIsLocalScope = true;

	// random mode
	std::optional<float> _randomFloat = std::nullopt;
};

// I'm too far into this to rework this overengineered mess again
// used by ReplacerMod class to store relevant state data together, so the ones with narrower scope can keep the others alive on update
struct VariantStateDataContainer
{
public:
	bool UpdateData(float a_deltaTime);
	bool OnLoopOrEcho(RE::ObjectRefHandle a_refHandle, ActiveClip* a_activeClip, bool a_bIsEcho);
	bool ClearRefrData(RE::ObjectRefHandle a_refHandle);
	void Clear();
	Conditions::IStateData* AccessStateData(RE::ObjectRefHandle a_key, RE::hkbClipGenerator* a_clipGenerator, const Variants* a_variants);
	Conditions::IStateData* AddStateData(RE::ObjectRefHandle a_key, Conditions::IStateData* a_stateData, RE::hkbClipGenerator* a_clipGenerator, const Variants* a_variants);

protected:
	StateDataContainer<> _replacerModVariantStateData{};
	std::unordered_map<SubMod*, StateDataContainer<>> _subModVariantStateData{};
	std::unordered_map<RE::hkbClipGenerator*, StateDataContainer<>> _localVariantStateData{};
};
