#pragma once

#include "ActiveClip.h"
#include "ReplacerMods.h"
#include "Jobs.h"

class OpenAnimationReplacer
{
public:
    static OpenAnimationReplacer& GetSingleton()
    {
        static OpenAnimationReplacer singleton;
        return singleton;
    }

    void OnDataLoaded() const;

    [[nodiscard]] ReplacementAnimation* GetReplacementAnimation(RE::hkbCharacterStringData* a_stringData, RE::hkbClipGenerator* a_clipGenerator, uint16_t a_originalIndex, RE::TESObjectREFR* a_refr) const;
    [[nodiscard]] ReplacementAnimation* GetReplacementAnimation(RE::hkbCharacter* a_character, RE::hkbClipGenerator* a_clipGenerator, uint16_t a_originalIndex) const;
    [[nodiscard]] bool HasReplacementData(RE::hkbCharacterStringData* a_stringData) const;
    bool RemoveReplacementData(RE::hkbCharacterStringData* a_stringData);
    ReplacerMod* GetReplacerMod(std::string_view a_path) const;
	ReplacerMod* GetReplacerModByName(std::string_view a_name) const;
    void AddReplacerMod(std::string_view a_path, std::unique_ptr<ReplacerMod>& a_replacerMod);
    ReplacerMod* GetOrCreateLegacyReplacerMod();
	void OnReplacerModNameChanged(std::string_view a_previousName, ReplacerMod* a_replacerMod);

    void InitializeReplacementAnimations(RE::hkbCharacterStringData* a_stringData) const;
    [[nodiscard]] AnimationReplacements* GetReplacements(RE::hkbCharacter* a_character, uint16_t a_originalIndex) const;

    [[nodiscard]] ActiveClip* GetActiveClip(RE::hkbClipGenerator* a_clipGenerator) const;
    [[nodiscard]] ActiveClip* GetActiveClipFromRefr(RE::TESObjectREFR* a_refr) const;
    ActiveClip* AddOrGetActiveClip(RE::hkbClipGenerator* a_clipGenerator, const RE::hkbContext& a_context, bool& a_bOutAdded);
    void RemoveActiveClip(RE::hkbClipGenerator* a_clipGenerator);

    [[nodiscard]] bool IsOriginalAnimationInterruptible(RE::hkbCharacter* a_character, uint16_t a_originalIndex) const;
    [[nodiscard]] bool ShouldOriginalAnimationReplaceOnEcho(RE::hkbCharacter* a_character, uint16_t a_originalIndex) const;
    [[nodiscard]] bool ShouldOriginalAnimationKeepRandomResultsOnLoop(RE::hkbCharacter* a_character, uint16_t a_originalIndex) const;

    void CreateReplacementAnimations(const char* a_path, RE::hkbCharacterStringData* a_stringData, RE::BShkbHkxDB::ProjectDBData* a_projectDBData);

    [[nodiscard]] ReplacerProjectData* GetReplacerProjectData(RE::hkbCharacterStringData* a_stringData) const;
    [[nodiscard]] ReplacerProjectData* GetOrAddReplacerProjectData(RE::hkbCharacterStringData* a_stringData, RE::BShkbHkxDB::ProjectDBData* a_projectDBData);
    void ForEachReplacerProjectData(const std::function<void(RE::hkbCharacterStringData*, ReplacerProjectData*)>& a_func) const;
    void ForEachReplacerMod(const std::function<void(ReplacerMod*)>& a_func) const;

    void SetSynchronizedClipsIDOffset(RE::hkbCharacterStringData* a_stringData, uint16_t a_offset);
    [[nodiscard]] uint16_t GetSynchronizedClipsIDOffset(RE::hkbCharacterStringData* a_stringData) const;
    [[nodiscard]] uint16_t GetSynchronizedClipsIDOffset(RE::hkbCharacter* a_character) const;

    static void LoadAnimation(RE::hkbCharacter* a_character, uint16_t a_animationIndex);
    static void UnloadAnimation(RE::hkbCharacter* a_character, uint16_t a_animationIndex);

    [[nodiscard]] bool AreFactoriesInitialized() const { return _bFactoriesInitialized; }
    void InitFactories();
    bool HasConditionFactory(std::string_view a_conditionName) const;
    void ForEachConditionFactory(const std::function<void(std::string_view, std::function<std::unique_ptr<Conditions::ICondition>()>)>& a_func) const;
    [[nodiscard]] std::unique_ptr<Conditions::ICondition> CreateCondition(std::string_view a_conditionName);

    bool IsPluginLoaded(std::string_view a_pluginName, REL::Version a_pluginVersion) const;
    REL::Version GetPluginVersion(std::string_view a_pluginName) const;
    OAR_API::Conditions::APIResult AddCustomCondition(std::string_view a_pluginName, REL::Version a_pluginVersion, std::string_view a_conditionName, Conditions::ConditionFactory a_conditionFactory);
    bool IsCustomCondition(std::string_view a_conditionName) const;

    void LoadKeywords() const;

    void RunJobs();

    template <class T, typename... Args>
    void QueueJob(Args&&... a_args)
    {
        WriteLocker locker(_jobsLock);

        static_assert(std::is_base_of_v<Jobs::GenericJob, T>);
        _jobs.push_back(std::make_unique<T>(std::forward<Args>(a_args)...));
    }

    static inline bool bKeywordsLoaded = false;
    static inline RE::BGSKeyword* kywd_weapTypeWarhammer = nullptr;
    static inline RE::BGSKeyword* kywd_weapTypeBattleaxe = nullptr;

    static inline std::atomic_bool bIsPreLoading = false;
    static inline float gameTimeCounter = 0.f;

protected:
    ExclusiveLock _parseLock;
    mutable SharedLock _dataLock;
    std::unordered_map<RE::hkbCharacterStringData*, std::unique_ptr<ReplacerProjectData>> _replacerProjectDatas;

    mutable SharedLock _modLock;
	std::unordered_map<std::string, std::unique_ptr<ReplacerMod>> _replacerMods;
	std::unique_ptr<ReplacerMod> _legacyReplacerMod = nullptr;

	mutable SharedLock _replacerModNameLock;
	std::unordered_map<std::string, ReplacerMod*> _replacerModNameMap;

    mutable SharedLock _activeClipsLock;
    std::unordered_map<RE::hkbClipGenerator*, std::unique_ptr<ActiveClip>> _activeClips;

    void InitDefaultProjects() const;
    [[nodiscard]] RE::Character* CreateDummyCharacter(RE::TESNPC* a_baseForm) const;

    void AddModParseResult(Parsing::ModParseResult& a_parseResult, RE::hkbCharacterStringData* a_stringData, RE::BShkbHkxDB::ProjectDBData* a_projectDBData);
    void AddSubModParseResult(ReplacerMod* a_replacerMod, Parsing::SubModParseResult& a_parseResult, RE::hkbCharacterStringData* a_stringData, RE::BShkbHkxDB::ProjectDBData* a_projectDBData);

    ExclusiveLock _factoriesLock;
    bool _bFactoriesInitialized = false;
    std::map<std::string, std::function<std::unique_ptr<Conditions::ICondition>()>> _conditionFactories;
    std::map<std::string, std::function<std::unique_ptr<Conditions::ICondition>()>> _hiddenConditionFactories;

    mutable SharedLock _customConditionsLock;
    std::unordered_map<std::string, REL::Version> _customConditionPlugins;
    std::unordered_map<std::string, Conditions::ConditionFactory> _customConditionFactories;

    mutable SharedLock _jobsLock;
    std::vector<std::unique_ptr<Jobs::GenericJob>> _jobs;

private:
    OpenAnimationReplacer() = default;
    OpenAnimationReplacer(const OpenAnimationReplacer&) = delete;
    OpenAnimationReplacer(OpenAnimationReplacer&&) = delete;
    virtual ~OpenAnimationReplacer() = default;

    OpenAnimationReplacer& operator=(const OpenAnimationReplacer&) = delete;
    OpenAnimationReplacer& operator=(OpenAnimationReplacer&&) = delete;
};
