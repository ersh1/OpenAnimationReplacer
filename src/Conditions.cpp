#include "Conditions.h"
#include "DetectedProblems.h"
#include "Offsets.h"
#include "OpenAnimationReplacer.h"
#include "Utils.h"

#include <imgui_stdlib.h>
#include <ranges>

namespace Conditions
{
    constexpr char CharToLower(const char a_char)
    {
        return (a_char >= 'A' && a_char <= 'Z') ? a_char + ('a' - 'A') : a_char;
    }

    constexpr uint32_t Hash(const char* a_data, const size_t a_size) noexcept
    {
        uint32_t hash = 5381;

        for (const char* c = a_data; c < a_data + a_size; ++c)
            hash = ((hash << 5) + hash) + CharToLower(*c);

        return hash;
    }

    constexpr uint32_t operator"" _h(const char* a_str, size_t a_size) noexcept
    {
        return Hash(a_str, a_size);
    }

    std::string_view CorrectLegacyConditionName(std::string_view a_conditionName)
    {
        if (a_conditionName == "IsEquippedShout"sv) {
            return "IsEquippedPower"sv;
        }

        return a_conditionName;
    }

    std::unique_ptr<ICondition> CreateConditionFromString(std::string_view a_line)
    {
        if (a_line.starts_with(";"sv)) {
            return nullptr;
        }

        const bool bNOT = a_line.starts_with("NOT"sv);

        if (bNOT) {
            a_line = a_line.substr(3);
            a_line = Utils::TrimWhitespace(a_line);
        }
        const size_t functionEndPos = a_line.find_first_of(" ("sv);
        const size_t argumentStartPos = a_line.find_first_not_of(" ("sv, functionEndPos);
        const size_t argumentEndPos = a_line.find(")"sv);

        std::string conditionName;
        std::string argument;
        if (functionEndPos == std::string_view::npos || argumentStartPos == std::string_view::npos) {
            conditionName = a_line;
            argument = ""sv;
        } else {
            conditionName = a_line.substr(0, functionEndPos);
            argument = a_line.substr(argumentStartPos, argumentEndPos - argumentStartPos);
        }

        if (auto condition = OpenAnimationReplacer::GetSingleton().CreateCondition(CorrectLegacyConditionName(conditionName))) {
            condition->PreInitialize();
            condition->InitializeLegacy(argument.data());
            condition->SetNegated(bNOT);
            condition->PostInitialize();
            return std::move(condition);
        }

        auto errorStr = std::format("Invalid line: \"{}\"", a_line);
        return std::make_unique<InvalidCondition>(errorStr);
    }

    std::unique_ptr<ICondition> CreateConditionFromJson(rapidjson::Value& a_value)
    {
        if (!a_value.IsObject()) {
            logger::error("Missing condition value!");
            return std::make_unique<InvalidCondition>("Missing condition value");
        }

        const auto object = a_value.GetObj();

        if (const auto conditionNameIt = object.FindMember("condition"); conditionNameIt != object.MemberEnd() && conditionNameIt->value.IsString()) {
            bool bHasRequiredPlugin = false;
            std::string requiredPluginName;
            if (const auto requiredPluginIt = object.FindMember("requiredPlugin"); requiredPluginIt != object.MemberEnd() && requiredPluginIt->value.IsString()) {
                bHasRequiredPlugin = true;
                requiredPluginName = requiredPluginIt->value.GetString();
            }

            REL::Version requiredVersion;
            if (const auto requiredVersionIt = object.FindMember("requiredVersion"); requiredVersionIt != object.MemberEnd() && requiredVersionIt->value.IsString()) {
                requiredVersion = REL::Version(requiredVersionIt->value.GetString());
            }

            std::string conditionName = conditionNameIt->value.GetString();

            if (bHasRequiredPlugin && !requiredPluginName.empty()) {
                // check if required plugin is loaded and is the required version or higher
                if (!OpenAnimationReplacer::GetSingleton().IsPluginLoaded(requiredPluginName, requiredVersion)) {
                    DetectedProblems::GetSingleton().AddMissingPluginName(requiredPluginName, requiredVersion);
                    auto errorStr = std::format("Missing required plugin {} version {}!", requiredPluginName, requiredVersion);
                    logger::error("{}", errorStr);
                    return std::make_unique<InvalidCondition>(errorStr);
                }
            } else {
                // no required plugin, compare required version with OAR conditions version
                if (requiredVersion > Plugin::VERSION) {
                    DetectedProblems::GetSingleton().MarkOutdatedVersion();
                    auto errorStr = std::format("Condition {} requires a newer version of OAR! ({})", conditionName, requiredVersion);
                    logger::error("{}", errorStr);
                    return std::make_unique<InvalidCondition>(errorStr);
                }
            }

            if (auto condition = OpenAnimationReplacer::GetSingleton().CreateCondition(conditionName)) {
                condition->PreInitialize();
                condition->Initialize(&a_value);
                condition->PostInitialize();
                return std::move(condition);
            }
            if (bHasRequiredPlugin) {
                DetectedProblems::GetSingleton().AddMissingPluginName(requiredPluginName, requiredVersion);
                auto errorStr = std::format("Condition {} not found in plugin {}!", conditionName, requiredPluginName);
                logger::error("{}", errorStr);
                return std::make_unique<InvalidCondition>(errorStr);
            }

            auto errorStr = std::format("Condition {} not found!", conditionName);
            logger::error("{}", errorStr);
            return std::make_unique<InvalidCondition>(errorStr);
        }

        auto errorStr = "Condition name not found!";
        logger::error("{}", errorStr);;
        return std::make_unique<InvalidCondition>(errorStr);
    }

    std::unique_ptr<ICondition> CreateCondition(std::string_view a_conditionName)
    {
        if (auto condition = OpenAnimationReplacer::GetSingleton().CreateCondition(a_conditionName)) {
            condition->PreInitialize();
            condition->PostInitialize();
            return std::move(condition);
        }

        return std::make_unique<InvalidCondition>(a_conditionName);
    }

    std::unique_ptr<ICondition> DuplicateCondition(const std::unique_ptr<ICondition>& a_conditionToDuplicate)
    {
        // serialize condition to json
        rapidjson::Document doc(rapidjson::kObjectType);

        rapidjson::Value serializedCondition(rapidjson::kObjectType);
        a_conditionToDuplicate->Serialize(&serializedCondition, &doc.GetAllocator());

        // create new condition from the serialized json
        return CreateConditionFromJson(serializedCondition);
    }

    std::unique_ptr<ConditionSet> DuplicateConditionSet(ConditionSet* a_conditionSetToDuplicate)
    {
        // serialize the condition set to json
        rapidjson::Document doc(rapidjson::kObjectType);

        rapidjson::Value serializedConditionSet = a_conditionSetToDuplicate->Serialize(doc.GetAllocator());

        // create new conditions from the serialized json
        auto newConditionSet = std::make_unique<ConditionSet>();

        for (auto& conditionValue : serializedConditionSet.GetArray()) {
            if (auto condition = CreateConditionFromJson(conditionValue)) {
                newConditionSet->AddCondition(condition);
            }
        }

        return newConditionSet;
    }

    bool ORCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        return conditionsComponent->conditionSet->EvaluateAny(a_refr, a_clipGenerator);
    }

    bool ANDCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        return conditionsComponent->conditionSet->EvaluateAll(a_refr, a_clipGenerator);
    }

    bool IsFormCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        return a_refr == formComponent->GetTESFormValue();
    }

    void IsEquippedCondition::InitializeLegacy(const char* a_argument)
    {
        formComponent->form.ParseLegacy(a_argument);
    }

    RE::BSString IsEquippedCondition::GetArgument() const
    {
        const auto formArgument = formComponent->GetArgument();
        return std::format("{} in {} hand", formArgument.data(), boolComponent->GetBoolValue() ? "left"sv : "right"sv).data();
    }

    RE::BSString IsEquippedCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return Utils::GetFormNameString(GetEquippedForm(a_refr)).data();
    }

    bool IsEquippedCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid()) {
            const auto equippedForm = GetEquippedForm(a_refr);
            return formComponent->GetTESFormValue() == equippedForm;
        }

        return false;
    }

    RE::TESForm* IsEquippedCondition::GetEquippedForm(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->GetEquippedObject(boolComponent->GetBoolValue());
            }
        }

        return nullptr;
    }

    void IsEquippedTypeCondition::InitializeLegacy(const char* a_argument)
    {
        numericComponent->value.ParseLegacy(a_argument);
    }

    void IsEquippedTypeCondition::PostInitialize()
    {
        ConditionBase::PostInitialize();
        numericComponent->value.getEnumMap = &IsEquippedTypeCondition::GetEnumMap;
    }

    RE::BSString IsEquippedTypeCondition::GetArgument() const
    {
        const auto numericArgument = numericComponent->GetArgument();
        return std::format("{} in {} hand", numericArgument.data(), boolComponent->GetBoolValue() ? "left"sv : "right"sv).data();
    }

    RE::BSString IsEquippedTypeCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        static auto map = GetEnumMap();
        if (const auto it = map.find(GetEquippedType(a_refr)); it != map.end()) {
            return it->second.data();
        }

        return std::format("Unknown ({})", Utils::GetFormNameString(GetEquippedForm(a_refr))).data();
    }

    bool IsEquippedTypeCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        const auto type = static_cast<int8_t>(numericComponent->GetNumericValue(a_refr));
        return GetEquippedType(a_refr) == type;
    }

    RE::TESForm* IsEquippedTypeCondition::GetEquippedForm(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->GetEquippedObject(boolComponent->GetBoolValue());
            }
        }

        return nullptr;
    }

    int8_t IsEquippedTypeCondition::GetEquippedType(RE::TESObjectREFR* a_refr) const
    {
        int8_t currentType = -1;

        if (const auto equippedForm = GetEquippedForm(a_refr)) {
            switch (*equippedForm->formType) {
            case RE::FormType::Weapon:
                if (const auto equippedWeapon = equippedForm->As<RE::TESObjectWEAP>()) {
                    switch (equippedWeapon->GetWeaponType()) {
                    case RE::WEAPON_TYPE::kHandToHandMelee:
                        currentType = 0;
                        break;
                    case RE::WEAPON_TYPE::kOneHandSword:
                        currentType = 1;
                        break;
                    case RE::WEAPON_TYPE::kOneHandDagger:
                        currentType = 2;
                        break;
                    case RE::WEAPON_TYPE::kOneHandAxe:
                        currentType = 3;
                        break;
                    case RE::WEAPON_TYPE::kOneHandMace:
                        currentType = 4;
                        break;
                    case RE::WEAPON_TYPE::kTwoHandSword:
                        currentType = 5;
                        break;
                    case RE::WEAPON_TYPE::kTwoHandAxe:
                        if (!OpenAnimationReplacer::bKeywordsLoaded) {
                            OpenAnimationReplacer::GetSingleton().LoadKeywords();
                        }
                    /*if (equippedWeapon->HasKeyword(OpenAnimationReplacer::kywd_weapTypeBattleaxe)) {
                        currentType = 6;
                    }*/
                        if (equippedWeapon->HasKeyword(OpenAnimationReplacer::kywd_weapTypeWarhammer)) {
                            currentType = 10;
                        } else {
                            // just fall back to battleaxe
                            currentType = 6;
                        }
                        break;
                    case RE::WEAPON_TYPE::kBow:
                        currentType = 7;
                        break;
                    case RE::WEAPON_TYPE::kStaff:
                        currentType = 8;
                        break;
                    case RE::WEAPON_TYPE::kCrossbow:
                        currentType = 9;
                        break;
                    }
                }
                break;
            case RE::FormType::Armor:
                if (auto equippedShield = equippedForm->As<RE::TESObjectARMO>()) {
                    currentType = 11;
                }
                break;
            case RE::FormType::Spell:
                if (const auto equippedSpell = equippedForm->As<RE::SpellItem>()) {
                    const RE::ActorValue associatedSkill = equippedSpell->GetAssociatedSkill();
                    switch (associatedSkill) {
                    case RE::ActorValue::kAlteration:
                        currentType = 12;
                        break;
                    case RE::ActorValue::kIllusion:
                        currentType = 13;
                        break;
                    case RE::ActorValue::kDestruction:
                        currentType = 14;
                        break;
                    case RE::ActorValue::kConjuration:
                        currentType = 15;
                        break;
                    case RE::ActorValue::kRestoration:
                        currentType = 16;
                        break;
                    }
                }
                break;
            case RE::FormType::Scroll:
                if (auto equippedScroll = equippedForm->As<RE::ScrollItem>()) {
                    currentType = 17;
                }
                break;
            case RE::FormType::Light:
                if (auto equippedTorch = equippedForm->As<RE::TESObjectLIGH>()) {
                    currentType = 18;
                }
                break;
            }
        } else {
            // nothing equipped
            currentType = 0;
        }

        return currentType;
    }

    std::map<int32_t, std::string_view> IsEquippedTypeCondition::GetEnumMap()
    {
        std::map<int32_t, std::string_view> enumMap;
        enumMap[-1] = "Other"sv;
        enumMap[0] = "Unarmed"sv;
        enumMap[1] = "Sword"sv;
        enumMap[2] = "Dagger"sv;
        enumMap[3] = "War Axe"sv;
        enumMap[4] = "Mace"sv;
        enumMap[5] = "Greatsword"sv;
        enumMap[6] = "Battleaxe"sv;
        enumMap[7] = "Bow"sv;
        enumMap[8] = "Staff"sv;
        enumMap[9] = "Crossbow"sv;
        enumMap[10] = "Warhammer"sv;
        enumMap[11] = "Shield"sv;
        enumMap[12] = "Alteration Spell"sv;
        enumMap[13] = "Illusion Spell"sv;
        enumMap[14] = "Destruction Spell"sv;
        enumMap[15] = "Conjuration Spell"sv;
        enumMap[16] = "Restoration Spell"sv;
        enumMap[17] = "Scroll"sv;
        enumMap[18] = "Torch"sv;
        return enumMap;
    }

    void IsEquippedHasKeywordCondition::InitializeLegacy(const char* a_argument)
    {
        keywordComponent->keyword.ParseLegacy(a_argument);
    }

    RE::BSString IsEquippedHasKeywordCondition::GetArgument() const
    {
        const auto keywordArgument = keywordComponent->GetArgument();
        return std::format("{} in {} hand", keywordArgument.data(), boolComponent->GetBoolValue() ? "left"sv : "right"sv).data();
    }

    RE::BSString IsEquippedHasKeywordCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return Utils::GetFormKeywords(GetEquippedKeywordForm(a_refr)).data();
    }

    bool IsEquippedHasKeywordCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (const auto bgsKeywordForm = GetEquippedKeywordForm(a_refr)) {
            return keywordComponent->HasKeyword(bgsKeywordForm);
        }

        return false;
    }

    RE::BGSKeywordForm* IsEquippedHasKeywordCondition::GetEquippedKeywordForm(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (const auto equippedForm = actor->GetEquippedObject(boolComponent->GetBoolValue())) {
                    return equippedForm->As<RE::BGSKeywordForm>();
                }
            }
        }

        return nullptr;
    }

    RE::BSString IsEquippedPowerCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return Utils::GetFormNameString(GetEquippedPower(a_refr)).data();
    }

    bool IsEquippedPowerCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid()) {
            const auto equippedPower = GetEquippedPower(a_refr);
            return equippedPower == formComponent->GetTESFormValue();
        }

        return false;
    }

    RE::TESForm* IsEquippedPowerCondition::GetEquippedPower(RE::TESObjectREFR* a_refr)
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->GetActorRuntimeData().selectedPower;
            }
        }

        return nullptr;
    }

    bool IsWornCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid() && a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                const auto inv = actor->GetInventory([this](const RE::TESBoundObject& a_object) {
                    return a_object.GetFormID() == formComponent->GetTESFormValue()->GetFormID();
                });

                for (const auto& invData : inv | std::views::values) {
                    const auto& [count, entry] = invData;
                    if (count > 0 && entry->IsWorn()) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool IsWornHasKeywordCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (keywordComponent->IsValid()) {
            if (auto inventoryChanges = TESObjectREFR_GetInventoryChanges(a_refr)) {
                bool bFound = false;
                keywordComponent->keyword.ForEachKeyword([&](auto a_kywd) {
                    if (InventoryChanges_WornHasKeyword(inventoryChanges, a_kywd)) {
                        bFound = true;
                        return RE::BSContainer::ForEachResult::kStop;
                    }
                    return RE::BSContainer::ForEachResult::kContinue;
                });
                return bFound;
            }
        }

        return false;
    }

    bool IsFemaleCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (const auto tesNPC = actor->GetActorBase()) {
                    return tesNPC->IsFemale();
                }
            }
        }

        return false;
    }

    bool IsChildCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->IsChild();
            }
        }

        return false;
    }

    bool IsPlayerTeammateCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->IsPlayerTeammate();
            }
        }

        return false;
    }

    bool IsInInteriorCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (const auto cell = actor->GetParentCell()) {
                    return cell->IsInteriorCell();
                }
            }
        }

        return false;
    }

    bool IsInFactionCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr && formComponent->IsValid()) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (const auto faction = formComponent->GetTESFormValue()->As<RE::TESFaction>()) {
                    return actor->IsInFaction(faction);
                }
            }
        }

        return false;
    }

    bool HasKeywordCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            bool bFound = false;
            keywordComponent->keyword.ForEachKeyword([&](auto a_kywd) {
                if (a_refr->HasKeyword(a_kywd)) {
                    bFound = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return bFound;
        }

        return false;
    }

    void HasMagicEffectCondition::InitializeLegacy(const char* a_argument)
    {
        formComponent->form.ParseLegacy(a_argument);
        boolComponent->SetBoolValue(false);
    }

    RE::BSString HasMagicEffectCondition::GetArgument() const
    {
        auto ret = formComponent->form.GetArgument();
        if (boolComponent->GetBoolValue()) {
            ret.append(" | Active Only"sv);
        }
        return ret.data();
    }

    bool HasMagicEffectCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid() && a_refr) {
            if (const auto magicEffect = formComponent->GetTESFormValue()->As<RE::EffectSetting>()) {
                if (const auto actor = a_refr->As<RE::Actor>()) {
                    const auto magicTarget = actor->AsMagicTarget();

                    if (boolComponent->GetBoolValue()) {
                        // active effects only, do the same thing as the game does but check the inactive flag as well
                        if (auto activeEffects = magicTarget->GetActiveEffectList()) {
                            for (RE::ActiveEffect* activeEffect : *activeEffects) {
                                if (!activeEffect->flags.any(RE::ActiveEffect::Flag::kInactive)) {
                                    if (activeEffect->GetBaseObject() == magicEffect) {
                                        return true;
                                    }
                                }
                            }
                        }
                    } else {
                        return magicTarget->HasMagicEffect(magicEffect);
                    }
                }
            }
        }

        return false;
    }

    void HasMagicEffectWithKeywordCondition::InitializeLegacy(const char* a_argument)
    {
        keywordComponent->keyword.ParseLegacy(a_argument);
        boolComponent->SetBoolValue(false);
    }

    RE::BSString HasMagicEffectWithKeywordCondition::GetArgument() const
    {
        auto ret = keywordComponent->keyword.GetArgument();
        if (boolComponent->GetBoolValue()) {
            ret.append(" | Active Only"sv);
        }
        return ret.data();
    }

    bool HasMagicEffectWithKeywordCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (keywordComponent->IsValid() && a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                auto magicTarget = actor->AsMagicTarget();

                if (boolComponent->GetBoolValue()) {
                    // active effects only, do the same thing as the game does but check the inactive flag as well
                    if (auto activeEffects = magicTarget->GetActiveEffectList()) {
                        for (RE::ActiveEffect* activeEffect : *activeEffects) {
                            if (!activeEffect->flags.any(RE::ActiveEffect::Flag::kInactive)) {
                                if (keywordComponent->HasKeyword(activeEffect->GetBaseObject())) {
                                    return true;
                                }
                            }
                        }
                    }
                } else {
                    bool bFound = false;
                    keywordComponent->keyword.ForEachKeyword([&](auto a_kywd) {
                        if (MagicTarget_HasMagicEffectWithKeyword(magicTarget, a_kywd, nullptr)) {
                            bFound = true;
                            return RE::BSContainer::ForEachResult::kStop;
                        }
                        return RE::BSContainer::ForEachResult::kContinue;
                    });
                    return bFound;
                }
            }
        }

        return false;
    }

    bool HasPerkCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid() && a_refr) {
            if (const auto perk = formComponent->GetTESFormValue()->As<RE::BGSPerk>()) {
                if (const auto actor = a_refr->As<RE::Actor>()) {
                    return actor->HasPerk(perk);
                }
            }
        }

        return false;
    }

    bool HasSpellCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid() && a_refr) {
            if (const auto spell = formComponent->GetTESFormValue()->As<RE::SpellItem>()) {
                if (const auto actor = a_refr->As<RE::Actor>()) {
                    return actor->HasSpell(spell);
                }
            }
        }

        return false;
    }

    void CompareValues::InitializeLegacy(const char* a_argument)
    {
        std::string_view argument(a_argument);
        if (const size_t splitPos = argument.find(','); splitPos != std::string_view::npos) {
            const std::string_view firstArg = Utils::TrimWhitespace(argument.substr(0, splitPos));
            const std::string_view secondArg = Utils::TrimWhitespace(argument.substr(splitPos + 1));

            const bool bIsActorValue = numericComponentA->value.GetType() == NumericValue::Type::kActorValue;
            numericComponentA->value.ParseLegacy(firstArg, bIsActorValue);
            numericComponentB->value.ParseLegacy(secondArg);
        } else {
            logger::error("Invalid argument: {}", argument);
        }
    }

    RE::BSString CompareValues::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        return std::format("{} {} {}", numericComponentA->value.GetArgument(), separator, numericComponentB->value.GetArgument()).data();
    }

    RE::BSString CompareValues::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return std::format("{}, {}", numericComponentA->GetNumericValue(a_refr), numericComponentB->GetNumericValue(a_refr)).data();
    }

    bool CompareValues::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        return comparisonComponent->GetComparisonResult(numericComponentA->GetNumericValue(a_refr), numericComponentB->GetNumericValue(a_refr));
    }

    void LevelCondition::InitializeLegacy(const char* a_argument)
    {
        numericComponent->value.ParseLegacy(a_argument);
    }

    RE::BSString LevelCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        return std::format("Level {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString LevelCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return std::to_string(actor->GetLevel()).data();
            }
        }

        return ""sv.data();
    }

    bool LevelCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return comparisonComponent->GetComparisonResult(actor->GetLevel(), numericComponent->GetNumericValue(a_refr));
            }
        }

        return false;
    }

    RE::BSString IsActorBaseCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (const auto tesNPC = GetActorBase(a_refr)) {
            return Utils::GetFormNameString(tesNPC).data();
        }

        return ""sv.data();
    }

    bool IsActorBaseCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (const auto tesNPC = GetActorBase(a_refr)) {
            return tesNPC == formComponent->GetTESFormValue();
        }

        return false;
    }

    RE::TESNPC* IsActorBaseCondition::GetActorBase(RE::TESObjectREFR* a_refr)
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->GetActorBase();
            }
        }

        return nullptr;
    }

    RE::BSString IsRaceCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (const auto race = GetRace(a_refr)) {
            return Utils::GetFormNameString(race).data();
        }

        return ""sv.data();
    }

    bool IsRaceCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (const auto race = GetRace(a_refr)) {
            return race == formComponent->GetTESFormValue();
        }

        return false;
    }

    RE::TESRace* IsRaceCondition::GetRace(RE::TESObjectREFR* a_refr)
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->GetRace();
            }
        }

        return nullptr;
    }

    RE::BSString CurrentWeatherCondition::GetCurrent([[maybe_unused]] RE::TESObjectREFR* a_refr) const
    {
        if (const auto currentWeather = RE::Sky::GetSingleton()->currentWeather) {
            return Utils::GetFormNameString(currentWeather).data();
        }

        return ""sv.data();
    }

    bool CurrentWeatherCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid()) {
            const auto currentWeather = RE::Sky::GetSingleton()->currentWeather;
            if (const auto weatherToCheck = formComponent->GetTESFormValue()->As<RE::TESWeather>()) {
                return currentWeather == weatherToCheck;
            }
            if (const auto weatherFormListToCheck = formComponent->GetTESFormValue()->As<RE::BGSListForm>()) {
                return weatherFormListToCheck->HasForm(currentWeather);
            }
        }

        return false;
    }

    RE::BSString CurrentGameTimeCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        return std::format("Time {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString CurrentGameTimeCondition::GetCurrent([[maybe_unused]] RE::TESObjectREFR* a_refr) const
    {
        float hours = GetHours();

        auto hoursInt = static_cast<uint32_t>(hours);
        auto minutesInt = static_cast<uint32_t>((hours - hoursInt) * 60.0f);
        return std::format("{:.3f} ({:02d}:{:02d})", hours, hoursInt, minutesInt).data();
    }

    bool CurrentGameTimeCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        const float v = numericComponent->GetNumericValue(a_refr);
        const float hours = GetHours();

        return comparisonComponent->GetComparisonResult(hours, v);
    }

    float CurrentGameTimeCondition::GetHours()
    {
        const float time = GetCurrentGameTime();

        float days;
        return modff(time, &days) * 24.f;
    }

    void RandomCondition::InitializeLegacy(const char* a_argument)
    {
        numericComponent->value.ParseLegacy(a_argument);
        comparisonComponent->SetComparisonOperator(ComparisonOperator::kLess);
        randomComponent->SetMinValue(0.f);
        randomComponent->SetMaxValue(1.f);
    }

    RE::BSString RandomCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        std::string randomArgument(randomComponent->GetArgument());

        return std::format("{} {} {}", randomArgument, separator, numericComponent->value.GetArgument()).data();
    }

    bool RandomCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const
    {
        const float v = numericComponent->GetNumericValue(a_refr);

        float randomFloat;
        if (!randomComponent->GetRandomFloat(a_clipGenerator, randomFloat)) {
            return true;
        }

        return comparisonComponent->GetComparisonResult(randomFloat, v);
    }

    bool IsUniqueCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (const auto actorBase = actor->GetActorBase()) {
                    return actorBase->IsUnique();
                }
            }
        }

        return false;
    }

    RE::BSString IsClassCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return Utils::GetFormNameString(GetTESClass(a_refr)).data();
    }

    bool IsClassCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid()) {
            if (const auto tesClass = GetTESClass(a_refr)) {
                return tesClass == formComponent->GetTESFormValue();
            }
        }

        return false;
    }

    RE::TESClass* IsClassCondition::GetTESClass(RE::TESObjectREFR* a_refr)
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (const auto actorBase = actor->GetActorBase()) {
                    return actorBase->npcClass;
                }
            }
        }

        return nullptr;
    }

    RE::BSString IsCombatStyleCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return Utils::GetFormNameString(GetCombatStyle(a_refr)).data();
    }

    bool IsCombatStyleCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid()) {
            if (const auto combatStyle = GetCombatStyle(a_refr)) {
                return combatStyle == formComponent->GetTESFormValue();
            }
        }

        return false;
    }

    RE::TESCombatStyle* IsCombatStyleCondition::GetCombatStyle(RE::TESObjectREFR* a_refr)
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (const auto actorBase = actor->GetActorBase()) {
                    return actorBase->GetCombatStyle();
                }
            }
        }

        return nullptr;
    }

    RE::BSString IsVoiceTypeCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return Utils::GetFormNameString(GetVoiceType(a_refr)).data();
    }

    bool IsVoiceTypeCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid()) {
            if (const auto voiceType = GetVoiceType(a_refr)) {
                return voiceType == formComponent->GetTESFormValue();
            }
        }

        return false;
    }

    RE::BGSVoiceType* IsVoiceTypeCondition::GetVoiceType(RE::TESObjectREFR* a_refr)
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (const auto actorBase = actor->GetActorBase()) {
                    return actorBase->GetVoiceType();
                }
            }
        }

        return nullptr;
    }

    bool IsAttackingCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->IsAttacking();
            }
        }

        return false;
    }

    bool IsRunningCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->IsRunning();
            }
        }

        return false;
    }

    bool IsSneakingCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->IsSneaking();
            }
        }

        return false;
    }

    bool IsSprintingCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->AsActorState()->IsSprinting();
            }
        }

        return false;
    }

    bool IsInAirCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->IsInMidair();
            }
        }

        return false;
    }

    bool IsInCombatCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->IsInCombat();
            }
        }

        return false;
    }

    bool IsWeaponDrawnCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->AsActorState()->IsWeaponDrawn();
            }
        }

        return false;
    }

    RE::BSString IsInLocationCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            return Utils::GetFormNameString(a_refr->GetCurrentLocation()).data();
        }

        return ""sv.data();
    }

    bool IsInLocationCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid() && a_refr) {
            if (const auto locationToCheck = formComponent->GetTESFormValue()->As<RE::BGSLocation>()) {
                if (const auto currentLocation = a_refr->GetCurrentLocation()) {
                    return IsLocation(currentLocation, locationToCheck);
                }
            }
        }

        return false;
    }

    bool IsInLocationCondition::IsLocation(RE::BGSLocation* a_location, RE::BGSLocation* a_locationToCompare)
    {
        if (a_location == a_locationToCompare) {
            return true;
        }

        if (a_location->parentLoc) {
            return IsLocation(a_location->parentLoc, a_locationToCompare);
        }

        return false;
    }

    void HasRefTypeCondition::InitializeLegacy(const char* a_argument)
    {
        locRefTypeComponent->locRefType.ParseLegacy(a_argument);
    }

    RE::BSString HasRefTypeCondition::GetArgument() const
    {
        return locRefTypeComponent->GetArgument();
    }

    RE::BSString HasRefTypeCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto currentLocRefType = TESObjectREFR_GetLocationRefType(a_refr)) {
                return currentLocRefType->GetFormEditorID();
            }
        }

        return ""sv.data();
    }

    bool HasRefTypeCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (auto currentLocRefType = TESObjectREFR_GetLocationRefType(a_refr)) {
                bool bFound = false;
                locRefTypeComponent->locRefType.ForEachKeyword([&](auto kywd) {
                    if (currentLocRefType == kywd) {
                        bFound = true;
                        return RE::BSContainer::ForEachResult::kStop;
                    }
                    return RE::BSContainer::ForEachResult::kContinue;
                });
                return bFound;
            }
        }

        return false;
    }

    RE::BSString IsParentCellCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            return Utils::GetFormNameString(a_refr->GetParentCell()).data();
        }

        return ""sv.data();
    }

    bool IsParentCellCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto parentCell = a_refr->GetParentCell()) {
                return parentCell == formComponent->GetTESFormValue();
            }
        }

        return false;
    }

    RE::BSString IsWorldSpaceCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            return Utils::GetFormNameString(a_refr->GetWorldspace()).data();
        }

        return ""sv.data();
    }

    bool IsWorldSpaceCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto worldspace = a_refr->GetWorldspace()) {
                return worldspace == formComponent->GetTESFormValue();
            }
        }

        return false;
    }

    void FactionRankCondition::InitializeLegacy(const char* a_argument)
    {
        std::string_view argument(a_argument);
        if (const size_t splitPos = argument.find(','); splitPos != std::string_view::npos) {
            const std::string_view firstArg = Utils::TrimWhitespace(argument.substr(0, splitPos));
            const std::string_view secondArg = Utils::TrimWhitespace(argument.substr(splitPos + 1));

            numericComponent->value.ParseLegacy(firstArg);
            factionComponent->form.ParseLegacy(secondArg);
        }

        logger::error("Invalid argument: {}", argument);
    }

    RE::BSString FactionRankCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        return std::format("{} rank {} {}", factionComponent->form.GetArgument(), separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString FactionRankCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return std::to_string(GetFactionRank(a_refr)).data();
    }

    bool FactionRankCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (factionComponent->IsValid() && a_refr) {
            const int32_t rank = GetFactionRank(a_refr);
            const float v = numericComponent->GetNumericValue(a_refr);
            return comparisonComponent->GetComparisonResult(static_cast<float>(rank), v);
        }

        return false;
    }

    int32_t FactionRankCondition::GetFactionRank(RE::TESObjectREFR* a_refr) const
    {
        int32_t rank = -2;

        if (factionComponent->IsValid() && a_refr) {
            if (auto faction = factionComponent->GetTESFormValue()->As<RE::TESFaction>()) {
                if (auto actor = a_refr->As<RE::Actor>()) {
                    bool bIsPlayer = actor->IsPlayerRef();

                    if (actor->IsInFaction(faction)) {
                        rank = Actor_GetFactionRank(actor, faction, bIsPlayer);
                    }
                }
            }
        }

        return rank;
    }

    void IsMovementDirectionCondition::InitializeLegacy(const char* a_argument)
    {
        numericComponent->value.ParseLegacy(a_argument);
    }

    void IsMovementDirectionCondition::PostInitialize()
    {
        ConditionBase::PostInitialize();
        numericComponent->value.getEnumMap = &IsMovementDirectionCondition::GetEnumMap;
    }

    RE::BSString IsMovementDirectionCondition::GetArgument() const
    {
        return numericComponent->GetArgument();
    }

    bool IsMovementDirectionCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        const float v = numericComponent->GetNumericValue(a_refr);
        if (v < 0 || v > 4) {
            return false;
        }

        if (a_refr) {
            if (auto actor = a_refr->As<RE::Actor>()) {
                float direction = 0;
                if (Actor_IsMoving(actor)) {
                    float movementDirectionRelativeToFacing = Actor_GetMovementDirectionRelativeToFacing(actor);
                    if (movementDirectionRelativeToFacing <= RE::NI_TWO_PI) {
                        if (movementDirectionRelativeToFacing < 0.f) {
                            movementDirectionRelativeToFacing = fmodf(movementDirectionRelativeToFacing, RE::NI_TWO_PI) + RE::NI_TWO_PI;
                        }
                    } else {
                        movementDirectionRelativeToFacing = fmodf(movementDirectionRelativeToFacing, RE::NI_TWO_PI);
                    }

                    direction = fmodf(((movementDirectionRelativeToFacing / RE::NI_TWO_PI) * 4.f) + 0.5f, 4.f);
                    direction = floorf(direction) + 1.f;
                }

                return direction == v;
            }
        }

        return false;
    }

    std::map<int32_t, std::string_view> IsMovementDirectionCondition::GetEnumMap()
    {
        std::map<int32_t, std::string_view> enumMap;
        enumMap[0] = "None"sv;
        enumMap[1] = "Forward"sv;
        enumMap[2] = "Right"sv;
        enumMap[3] = "Back"sv;
        enumMap[4] = "Left"sv;
        return enumMap;
    }

    RE::BSString IsEquippedShoutCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return Utils::GetFormNameString(GetEquippedShout(a_refr)).data();
    }

    bool IsEquippedShoutCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid()) {
            const auto equippedShout = GetEquippedShout(a_refr);
            return equippedShout == formComponent->GetTESFormValue();
        }

        return false;
    }

    RE::TESShout* IsEquippedShoutCondition::GetEquippedShout(RE::TESObjectREFR* a_refr)
    {
        if (a_refr) {
            if (auto actor = a_refr->As<RE::Actor>()) {
                return Actor_GetEquippedShout(actor);
            }
        }

        return nullptr;
    }

    RE::BSString HasGraphVariableCondition::GetArgument() const
    {
        return textComponent->GetTextValue();
    }

    bool HasGraphVariableCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            //int32_t graphVariableType = static_cast<int32_t>(numericComponent->GetNumericValue(a_refr));
            //switch (graphVariableType) {
            //case 0:  // Float
            //	float f;
            //	return a_refr->GetGraphVariableFloat(textComponent->GetTextValue(), f);
            //case 1:  // Int
            //	int32_t i;
            //	return a_refr->GetGraphVariableInt(textComponent->GetTextValue(), i);
            //case 2:  // Bool
            //	bool b;
            //	return a_refr->GetGraphVariableBool(textComponent->GetTextValue(), b);
            //case 3:  // NiPoint3
            //	RE::NiPoint3 vec;
            //	return a_refr->GetGraphVariableNiPoint3(textComponent->GetTextValue(), vec);
            //}

            // seems that the return is correct regardless of the type
            float f;
            return a_refr->GetGraphVariableFloat(textComponent->GetTextValue(), f);
        }

        return false;
    }

    RE::BSString SubmergeLevelCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        return std::format("Submerge level {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString SubmergeLevelCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        float submergeLevel = 0.f;
        if (a_refr) {
            submergeLevel = TESObjectREFR_GetSubmergeLevel(a_refr, a_refr->GetPositionZ(), a_refr->GetParentCell());
        }

        return std::to_string(submergeLevel).data();
    }

    bool SubmergeLevelCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            const float submergeLevel = TESObjectREFR_GetSubmergeLevel(a_refr, a_refr->GetPositionZ(), a_refr->GetParentCell());
            return comparisonComponent->GetComparisonResult(submergeLevel, numericComponent->GetNumericValue(a_refr));
        }

        return false;
    }

    RE::BSString IsReplacerEnabledCondition::GetArgument() const
    {
        const auto submodArgument = textComponentSubmod->text.GetValue();
        if (submodArgument.empty()) {
            return textComponentMod->GetArgument();
        }

        return std::format("{} - {}", textComponentMod->text.GetValue(), submodArgument).data();
    }

    bool IsReplacerEnabledCondition::EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (const auto replacerMod = OpenAnimationReplacer::GetSingleton().GetReplacerModByName(textComponentMod->text.GetValue())) {
            const auto submodName = textComponentSubmod->text.GetValue();
            const auto result = replacerMod->ForEachSubMod([&](const SubMod* a_submod) {
                if (!a_submod->IsDisabled()) {
                    if (submodName.empty() || a_submod->GetName() == submodName) {
                        return RE::BSVisit::BSVisitControl::kStop;
                    }
                }

                return RE::BSVisit::BSVisitControl::kContinue;
            });

            if (result == RE::BSVisit::BSVisitControl::kStop) {
                return true;
            }
        }

        return false;
    }

    RE::BSString IsCurrentPackageCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return Utils::GetFormNameString(GetCurrentPackage(a_refr)).data();
    }

    bool IsCurrentPackageCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid()) {
            if (const auto currentPackage = GetCurrentPackage(a_refr)) {
                return currentPackage == formComponent->GetTESFormValue();
            }
        }

        return false;
    }

    RE::TESPackage* IsCurrentPackageCondition::GetCurrentPackage(RE::TESObjectREFR* a_refr)
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->GetCurrentPackage();
            }
        }

        return nullptr;
    }

    void IsWornInSlotHasKeywordCondition::PostInitialize()
    {
        ConditionBase::PostInitialize();
        slotComponent->value.getEnumMap = &IsWornInSlotHasKeywordCondition::GetEnumMap;
    }

    RE::BSString IsWornInSlotHasKeywordCondition::GetArgument() const
    {
        std::string slotName = "(Invalid)";
        const auto slot = static_cast<uint32_t>(slotComponent->GetNumericValue(nullptr));

        static auto map = GetEnumMap();
        if (const auto it = map.find(slot); it != map.end()) {
            slotName = it->second;
        }

        return std::format("{}, {}", slotName, keywordComponent->keyword.GetArgument()).data();
    }

    bool IsWornInSlotHasKeywordCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                const auto slot = static_cast<RE::BIPED_MODEL::BipedObjectSlot>(1 << static_cast<uint32_t>(slotComponent->GetNumericValue(a_refr)));
                if (const auto armor = actor->GetWornArmor(slot, true)) {
                    return keywordComponent->HasKeyword(armor);
                }
            }
        }

        return false;
    }

    std::map<int32_t, std::string_view> IsWornInSlotHasKeywordCondition::GetEnumMap()
    {
        std::map<int32_t, std::string_view> enumMap;

        enumMap[0] = "Head"sv;
        enumMap[1] = "Hair"sv;
        enumMap[2] = "Body"sv;
        enumMap[3] = "Hands"sv;
        enumMap[4] = "Forearms"sv;
        enumMap[5] = "Amulet"sv;
        enumMap[6] = "Ring"sv;
        enumMap[7] = "Feet"sv;
        enumMap[8] = "Calves"sv;
        enumMap[9] = "Shield"sv;
        enumMap[10] = "Tail"sv;
        enumMap[11] = "LongHair"sv;
        enumMap[12] = "Circlet"sv;
        enumMap[13] = "Ears"sv;
        enumMap[14] = "ModMouth"sv;
        enumMap[15] = "ModNeck"sv;
        enumMap[16] = "ModChestPrimary"sv;
        enumMap[17] = "ModBack"sv;
        enumMap[18] = "ModMisc1"sv;
        enumMap[19] = "ModPelvisPrimary"sv;
        enumMap[20] = "DecapitateHead"sv;
        enumMap[21] = "Decapitate"sv;
        enumMap[22] = "ModPelvisSecondary"sv;
        enumMap[23] = "ModLegRight"sv;
        enumMap[24] = "ModLegLeft"sv;
        enumMap[25] = "ModFaceJewelry"sv;
        enumMap[26] = "ModChestSecondary"sv;
        enumMap[27] = "ModShoulder"sv;
        enumMap[28] = "ModArmLeft"sv;
        enumMap[29] = "ModArmRight"sv;
        enumMap[30] = "ModMisc2"sv;
        enumMap[31] = "FX01"sv;

        return enumMap;
    }

    std::map<int32_t, std::string_view> MovementSpeedCondition::GetEnumMap()
    {
        std::map<int32_t, std::string_view> enumMap;
        enumMap[0] = "Run"sv;
        enumMap[1] = "Jog"sv;
        enumMap[2] = "Fast walk"sv;
        enumMap[3] = "Walk"sv;
        return enumMap;
    }

    RE::BSString ScaleCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        return std::format("Scale {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString ScaleCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            return std::to_string(a_refr->GetScale()).data();
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool ScaleCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            const float scale = a_refr->GetScale();
            return comparisonComponent->GetComparisonResult(scale, numericComponent->GetNumericValue(a_refr));
        }

        return false;
    }

    RE::BSString HeightCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);
        return std::format("Height {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString HeightCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return std::to_string(actor->GetHeight()).data();
            }

            return std::to_string(a_refr->GetHeight()).data();
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool HeightCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return comparisonComponent->GetComparisonResult(actor->GetHeight(), numericComponent->GetNumericValue(actor));
            }

            return comparisonComponent->GetComparisonResult(a_refr->GetHeight(), numericComponent->GetNumericValue(a_refr));
        }

        return false;
    }

    RE::BSString WeightCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);
        return std::format("Weight {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString WeightCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return std::to_string(actor->GetWeight()).data();
            }

            return std::to_string(a_refr->GetWeight()).data();
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool WeightCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return comparisonComponent->GetComparisonResult(actor->GetWeight(), numericComponent->GetNumericValue(actor));
            }

            return comparisonComponent->GetComparisonResult(a_refr->GetWeight(), numericComponent->GetNumericValue(a_refr));
        }

        return false;
    }

    void MovementSpeedCondition::PostInitialize()
    {
        ConditionBase::PostInitialize();
        movementTypeComponent->value.getEnumMap = &MovementSpeedCondition::GetEnumMap;
    }

    RE::BSString MovementSpeedCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        std::string movementSpeedTypeName = "(Invalid)";
        const auto movementType = static_cast<uint32_t>(movementTypeComponent->GetNumericValue(nullptr));

        static auto map = GetEnumMap();
        if (const auto it = map.find(movementType); it != map.end()) {
            movementSpeedTypeName = it->second;
        }

        return std::format("{} speed {} {}", movementSpeedTypeName, separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString MovementSpeedCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return std::to_string(GetMovementSpeed(actor)).data();
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool MovementSpeedCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                const float movementSpeed = GetMovementSpeed(actor);
                return comparisonComponent->GetComparisonResult(movementSpeed, numericComponent->GetNumericValue(a_refr));
            }
        }

        return false;
    }

    float MovementSpeedCondition::GetMovementSpeed(RE::Actor* a_actor) const
    {
        if (a_actor) {
            const auto movementType = static_cast<int8_t>(movementTypeComponent->GetNumericValue(a_actor));
            switch (movementType) {
            case 0:
                return a_actor->GetRunSpeed();
            case 1:
                return a_actor->GetJogSpeed();
            case 2:
                return a_actor->GetFastWalkSpeed();
            case 3:
                return a_actor->GetWalkSpeed();
            }
        }

        return 0.f;
    }

    RE::BSString CurrentMovementSpeedCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        return std::format("Movement speed {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString CurrentMovementSpeedCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return std::to_string(actor->AsActorState()->DoGetMovementSpeed()).data();
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool CurrentMovementSpeedCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                const float movementSpeed = actor->AsActorState()->DoGetMovementSpeed();
                return comparisonComponent->GetComparisonResult(movementSpeed, numericComponent->GetNumericValue(a_refr));
            }
        }

        return false;
    }

    RE::BSString WindSpeedCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        return std::format("Wind speed {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString WindSpeedCondition::GetCurrent([[maybe_unused]] RE::TESObjectREFR* a_refr) const
    {
        return std::to_string(GetWindSpeed()).data();
    }

    bool WindSpeedCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        return comparisonComponent->GetComparisonResult(GetWindSpeed(), numericComponent->GetNumericValue(a_refr));
    }

    float WindSpeedCondition::GetWindSpeed()
    {
        if (const auto tes = RE::TES::GetSingleton()) {
            if (const auto sky = tes->sky) {
                return sky->windSpeed;
            }
        }

        return 0.f;
    }

    RE::BSString WindAngleDifferenceCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        return std::format("Angles difference {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString WindAngleDifferenceCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            return std::to_string(GetWindAngleDifference(a_refr)).data();
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool WindAngleDifferenceCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        return comparisonComponent->GetComparisonResult(GetWindAngleDifference(a_refr), numericComponent->GetNumericValue(a_refr));
    }

    float WindAngleDifferenceCondition::GetWindAngleDifference(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto tes = RE::TES::GetSingleton()) {
                if (const auto sky = tes->sky) {
                    const auto windAngle = sky->windAngle;
                    const auto refAngle = a_refr->GetAngleZ();

                    auto ret = Utils::NormalRelativeAngle(refAngle - windAngle);
                    if (degreesComponent->GetBoolValue()) {
                        ret = RE::rad_to_deg(ret);
                    }
                    if (absoluteComponent->GetBoolValue()) {
                        ret = std::fabs(ret);
                    }

                    return ret;
                }
            }
        }

        return 0.f;
    }

    RE::BSString CrimeGoldCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);
        const auto factionArgument = factionComponent->form.GetArgument();
        return std::format("Crime gold in {} {} {}", factionArgument, separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString CrimeGoldCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr && factionComponent->IsValid()) {
            if (const auto faction = factionComponent->GetTESFormValue()->As<RE::TESFaction>()) {
                if (const auto actor = a_refr->As<RE::Actor>()) {
                    return std::to_string(actor->GetCrimeGoldValue(faction)).data();
                }
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool CrimeGoldCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr && factionComponent->IsValid()) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (const auto faction = factionComponent->GetTESFormValue()->As<RE::TESFaction>()) {
                    const auto crimeGold = actor->GetCrimeGoldValue(faction);
                    return comparisonComponent->GetComparisonResult(static_cast<float>(crimeGold), numericComponent->GetNumericValue(a_refr));
                }
            }
        }

        return false;
    }

    bool IsBlockingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->IsBlocking();
            }
        }

        return false;
    }

    void IsCombatStateCondition::PostInitialize()
    {
        ConditionBase::PostInitialize();
        combatStateComponent->value.getEnumMap = &IsCombatStateCondition::GetEnumMap;
    }

    RE::BSString IsCombatStateCondition::GetArgument() const
    {
        const auto combatState = static_cast<RE::ACTOR_COMBAT_STATE>(combatStateComponent->GetNumericValue(nullptr));

        return GetCombatStateName(combatState);
    }

    RE::BSString IsCombatStateCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return GetCombatStateName(GetCombatState(actor));
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool IsCombatStateCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (auto actor = a_refr->As<RE::Actor>()) {
                return GetCombatState(actor) == static_cast<RE::ACTOR_COMBAT_STATE>(combatStateComponent->GetNumericValue(a_refr));
            }
        }

        return false;
    }

    RE::ACTOR_COMBAT_STATE IsCombatStateCondition::GetCombatState(RE::Actor* a_actor) const
    {
        if (a_actor) {
            return Actor_GetCombatState(a_actor);
        }

        return RE::ACTOR_COMBAT_STATE::kNone;
    }

    std::string_view IsCombatStateCondition::GetCombatStateName(RE::ACTOR_COMBAT_STATE a_state) const
    {
        static auto map = GetEnumMap();
        if (const auto it = map.find(static_cast<int32_t>(a_state)); it != map.end()) {
            return it->second;
        }

        return "(Invalid)"sv;
    }

    std::map<int32_t, std::string_view> IsCombatStateCondition::GetEnumMap()
    {
        std::map<int32_t, std::string_view> enumMap;
        enumMap[0] = "Not in combat"sv;
        enumMap[1] = "In combat "sv;
        enumMap[2] = "Searching"sv;

        return enumMap;
    }

    RE::BSString InventoryCountCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);
        const auto formArgument = formComponent->form.GetArgument();
        return std::format("{} count {} {}", formArgument, separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString InventoryCountCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (formComponent->IsValid() && a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                const auto inv = actor->GetInventory([this](const RE::TESBoundObject& a_object) {
                    return a_object.GetFormID() == formComponent->GetTESFormValue()->GetFormID();
                });

				int count = 0;
                for (const auto& invData : inv | std::views::values) {
                    const auto& [itemCount, entry] = invData;
					count += itemCount;
                }

				return std::to_string(count).data();
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool InventoryCountCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid() && a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                const auto inv = actor->GetInventory([this](const RE::TESBoundObject& a_object) {
                    return a_object.GetFormID() == formComponent->GetTESFormValue()->GetFormID();
                });

                for (const auto& invData : inv | std::views::values) {
                    const auto& [count, entry] = invData;

                    if (comparisonComponent->GetComparisonResult(static_cast<float>(count), numericComponent->GetNumericValue(a_refr))) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    RE::BSString FallDistanceCondition::GetArgument() const
    {
		const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

		return std::format("Fall distance {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString FallDistanceCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
		return std::to_string(GetFallDistance(a_refr)).data();
    }

    bool FallDistanceCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
		return comparisonComponent->GetComparisonResult(GetFallDistance(a_refr), numericComponent->GetNumericValue(a_refr));
    }

    float FallDistanceCondition::GetFallDistance(RE::TESObjectREFR* a_refr)
    {
		if (a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				if (auto charController = actor->GetCharController()) {
					if (charController->fallTime > 0.f) {
						return bhkCharacterController_CalcFallDistance(charController);
					}
				}
			}
		}

		return 0.f;
    }

    RE::BSString FallDamageCondition::GetArgument() const
    {
        const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

        return std::format("Fall damage {} {}", separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString FallDamageCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        return std::to_string(GetFallDamage(a_refr)).data();
    }

    bool FallDamageCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        return comparisonComponent->GetComparisonResult(GetFallDamage(a_refr), numericComponent->GetNumericValue(a_refr));
    }

    float FallDamageCondition::GetFallDamage(RE::TESObjectREFR* a_refr)
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (auto charController = actor->GetCharController()) {
                    if (charController->fallTime > 0.f) {
                        float fallDistance = bhkCharacterController_CalcFallDistance(charController);
						return TESObjectREFR_CalcFallDamage(a_refr, fallDistance, 0.25f);  // The game seems to use a 0.25f mult
                    }
                }
            }
        }

        return 0.f;
    }

    void CurrentPackageProcedureTypeCondition::PostInitialize()
    {
        ConditionBase::PostInitialize();
        packageProcedureTypeComponent->value.getEnumMap = &CurrentPackageProcedureTypeCondition::GetEnumMap;
    }

    RE::BSString CurrentPackageProcedureTypeCondition::GetArgument() const
    {
        const auto packageProcedureType = static_cast<RE::PACKAGE_PROCEDURE_TYPE>(packageProcedureTypeComponent->GetNumericValue(nullptr));

        return GetPackageProcedureTypeName(packageProcedureType);
    }

    RE::BSString CurrentPackageProcedureTypeCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return GetPackageProcedureTypeName(GetPackageProcedureType(actor));
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool CurrentPackageProcedureTypeCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (auto actor = a_refr->As<RE::Actor>()) {
                return GetPackageProcedureType(actor) == static_cast<RE::PACKAGE_PROCEDURE_TYPE>(packageProcedureTypeComponent->GetNumericValue(a_refr));
            }
        }

        return false;
    }

    RE::PACKAGE_PROCEDURE_TYPE CurrentPackageProcedureTypeCondition::GetPackageProcedureType(RE::Actor* a_actor) const
    {
        if (a_actor) {
            if (const auto currentPackage = a_actor->GetCurrentPackage()) {
                return *currentPackage->packData.packType;
            }
        }

        return RE::PACKAGE_PROCEDURE_TYPE::kNone;
    }

    std::string_view CurrentPackageProcedureTypeCondition::GetPackageProcedureTypeName(RE::PACKAGE_PROCEDURE_TYPE a_type) const
    {
        static auto map = GetEnumMap();
        if (const auto it = map.find(static_cast<int32_t>(a_type)); it != map.end()) {
            return it->second;
        }

        return "(Invalid)"sv;
    }

    std::map<int32_t, std::string_view> CurrentPackageProcedureTypeCondition::GetEnumMap()
    {
        std::map<int32_t, std::string_view> enumMap;
        enumMap[-1] = "None"sv;
        enumMap[0] = "Find"sv;
        enumMap[1] = "Follow"sv;
        enumMap[2] = "Escort"sv;
        enumMap[3] = "Eat"sv;
        enumMap[4] = "Sleep"sv;
        enumMap[5] = "Wander"sv;
        enumMap[6] = "Travel"sv;
        enumMap[7] = "Accompany"sv;
        enumMap[8] = "UseItemAt"sv;
        enumMap[9] = "Ambush"sv;
        enumMap[10] = "FleeNotCombat"sv;
        enumMap[11] = "CastMagic"sv;
        enumMap[12] = "Sandbox"sv;
        enumMap[13] = "Patrol"sv;
        enumMap[14] = "Guard"sv;
        enumMap[15] = "Dialogue"sv;
        enumMap[16] = "UseWeapon"sv;
        enumMap[17] = "Find2"sv;
        enumMap[18] = "Package"sv;
        enumMap[19] = "PackageTemplate"sv;
        enumMap[20] = "Activate"sv;
        enumMap[21] = "Alarm"sv;
        enumMap[22] = "Flee"sv;
        enumMap[23] = "Trespass"sv;
        enumMap[24] = "Spectator"sv;
        enumMap[25] = "ReactToDead"sv;
        enumMap[26] = "GetUpFromChairBed"sv;
        enumMap[27] = "DoNothing"sv;
        enumMap[28] = "InGameDialogue"sv;
        enumMap[29] = "Surface"sv;
        enumMap[30] = "SearchForAttacker"sv;
        enumMap[31] = "AvoidPlayer"sv;
        enumMap[32] = "ReactToDestroyedObject"sv;
        enumMap[33] = "ReactToGrenadeOrMine"sv;
        enumMap[34] = "StealWarning"sv;
        enumMap[35] = "PickPocketWarning"sv;
        enumMap[36] = "MovementBlocked"sv;
        enumMap[37] = "VampireFeed"sv;
        enumMap[38] = "Cannibal"sv;
        return enumMap;
    }
	
    bool IsOnMountCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                return actor->IsOnMount();
            }
        }

        return false;
    }

    RE::BSString IsRidingCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (RE::ActorPtr mountPtr = nullptr; actor->GetMount(mountPtr)) {
                    if (boolComponent->GetBoolValue()) {
                        if (const auto mountBase = mountPtr->GetActorBase()) {
                            return Utils::GetFormNameString(mountBase).data();
                        }
                    } else {
						return Utils::GetFormNameString(mountPtr.get()).data();
                    }
                }
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool IsRidingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid() && a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (RE::ActorPtr mountPtr = nullptr; actor->GetMount(mountPtr)) {
                    if (boolComponent->GetBoolValue()) {
                        if (const auto mountBase = mountPtr->GetActorBase()) {
                            return mountBase->GetFormID() == formComponent->GetTESFormValue()->GetFormID();
                        }
                    } else {
                        return mountPtr->GetFormID() == formComponent->GetTESFormValue()->GetFormID();
                    }
                }
            }
        }

        return false;
    }

    RE::BSString IsRidingHasKeywordCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (RE::ActorPtr mountPtr = nullptr; actor->GetMount(mountPtr)) {
                    return Utils::GetFormKeywords(mountPtr.get()).data();
                }
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool IsRidingHasKeywordCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (RE::ActorPtr mountPtr = nullptr; actor->GetMount(mountPtr)) {
                    if (const auto keywordForm = mountPtr->As<RE::BGSKeywordForm>()) {
                        return keywordComponent->HasKeyword(keywordForm);
                    }
                }
            }
        }

        return false;
    }

    bool IsBeingRiddenCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                RE::ActorPtr riderPtr = nullptr;
                return actor->GetMountedBy(riderPtr);
            }
        }

        return false;
    }

    RE::BSString IsBeingRiddenByCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (RE::ActorPtr riderPtr = nullptr; actor->GetMountedBy(riderPtr)) {
                    if (boolComponent->GetBoolValue()) {
                        if (const auto riderBase = riderPtr->GetActorBase()) {
                            return Utils::GetFormNameString(riderBase).data();
                        }
                    } else {
                        return Utils::GetFormNameString(riderPtr.get()).data();
                    }
                }
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool IsBeingRiddenByCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid() && a_refr) {
            if (const auto actor = a_refr->As<RE::Actor>()) {
                if (RE::ActorPtr riderPtr = nullptr; actor->GetMountedBy(riderPtr)) {
                    if (boolComponent->GetBoolValue()) {
                        if (const auto riderBase = riderPtr->GetActorBase()) {
                            return riderBase->GetFormID() == formComponent->GetTESFormValue()->GetFormID();
                        }
                    } else {
                        return riderPtr->GetFormID() == formComponent->GetTESFormValue()->GetFormID();
                    }
                }
            }
        }

        return false;
    }

    RE::BSString CurrentFurnitureCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto furniture = Utils::GetCurrentFurnitureForm(a_refr, boolComponent->GetBoolValue())) {
                const auto formNameString = Utils::GetFormNameString(furniture);
                const auto formIDString = std::format("[{:08X}]", furniture->GetFormID());
                if (formNameString == formIDString) {
                    return formNameString.data();
                } else {
                    return std::format("{} {}", formNameString, formIDString).data();
                }
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool CurrentFurnitureCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (formComponent->IsValid() && a_refr) {
            if (const auto furniture = Utils::GetCurrentFurnitureForm(a_refr, boolComponent->GetBoolValue())) {
                return furniture->GetFormID() == formComponent->GetTESFormValue()->GetFormID();
            }
        }

        return false;
    }

    RE::BSString CurrentFurnitureHasKeywordCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
        if (a_refr) {
            if (const auto furniture = Utils::GetCurrentFurnitureForm(a_refr, boolComponent->GetBoolValue())) {
                return Utils::GetFormKeywords(furniture).data();
            }
        }

        return ConditionBase::GetCurrent(a_refr);
    }

    bool CurrentFurnitureHasKeywordCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
        if (a_refr) {
            if (const auto furniture = Utils::GetCurrentFurnitureForm(a_refr, boolComponent->GetBoolValue())) {
                if (const auto keywordForm = furniture->As<RE::BGSKeywordForm>()) {
                    return keywordComponent->HasKeyword(keywordForm);
                }
            }
        }

        return false;
    }

    void TargetConditionBase::PostInitialize()
    {
        ConditionBase::PostInitialize();
		targetTypeComponent->value.getEnumMap = &CurrentTargetCondition::GetEnumMap;
    }

    std::string_view TargetConditionBase::GetTargetTypeName(Utils::TargetType a_targetType) const
    {
		static auto map = GetEnumMap();
		if (const auto it = map.find(static_cast<int32_t>(a_targetType)); it != map.end()) {
			return it->second;
		}

		return "(Invalid)"sv;
    }

    std::map<int32_t, std::string_view> TargetConditionBase::GetEnumMap()
    {
		std::map<int32_t, std::string_view> enumMap;
		enumMap[0] = "Target"sv;
		enumMap[1] = "Combat target"sv;
		enumMap[2] = "Dialogue target"sv;
		enumMap[3] = "Follow target"sv;
		enumMap[4] = "Headtrack target"sv;
		enumMap[5] = "Package target"sv;
		return enumMap;
    }

    RE::BSString HasTargetCondition::GetArgument() const
    {
		const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(nullptr));

		return GetTargetTypeName(targetType);
    }

    bool HasTargetCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
		if (a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				RE::TESObjectREFRPtr target = nullptr;
				const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(a_refr));
				return Utils::GetCurrentTarget(actor, targetType, target);
			}
		}

		return false;
    }

    RE::BSString CurrentTargetCondition::GetArgument() const
	{
		const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(nullptr));
		auto baseOptional = boolComponent->GetBoolValue() ? " [Base]" : "";

		return std::format("{}{} is {}", GetTargetTypeName(targetType), baseOptional, formComponent->form.GetArgument()).data();
	}

	RE::BSString CurrentTargetCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
	{
		if (a_refr) {
			if (const auto target = GetTargetForm(a_refr)) {
				return Utils::GetFormNameString(target).data();
			}
		}

		return ConditionBase::GetCurrent(a_refr);
	}

	bool CurrentTargetCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
	{
		if (formComponent->IsValid() && a_refr) {
			if (const auto target = GetTargetForm(a_refr)) {
				return target->GetFormID() == formComponent->GetTESFormValue()->GetFormID();
			}
		}

		return false;
	}

    RE::TESForm* CurrentTargetCondition::GetTargetForm(RE::TESObjectREFR* a_refr) const
    {
		if (a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				RE::TESObjectREFRPtr target = nullptr;
				const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(a_refr));
				if (Utils::GetCurrentTarget(actor, targetType, target)) {
					if (boolComponent->GetBoolValue()) {
						if (const auto targetBase = target->GetBaseObject()) {
							return targetBase;
						}
					} else {
						return target.get();
					}
				}
			}
		}

		return nullptr;
    }

    RE::BSString CurrentTargetHasKeywordCondition::GetArgument() const
    {
		const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(nullptr));
		auto baseOptional = boolComponent->GetBoolValue() ? " [Base]" : "";

		return std::format("{}{} has keyword {}", GetTargetTypeName(targetType), baseOptional, keywordComponent->keyword.GetArgument()).data();
    }

    RE::BSString CurrentTargetHasKeywordCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
		if (a_refr) {
			if (const auto target = GetTargetForm(a_refr)) {
				return Utils::GetFormKeywords(target).data();
			}
		}

		return ConditionBase::GetCurrent(a_refr);
    }

    bool CurrentTargetHasKeywordCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
		if (a_refr) {
			if (const auto target = GetTargetForm(a_refr)) {
				if (const auto bgsKeywordForm = target->As<RE::BGSKeywordForm>()) {
				    return keywordComponent->HasKeyword(bgsKeywordForm);
				}
			}
		}

		return false;
    }

    RE::TESForm* CurrentTargetHasKeywordCondition::GetTargetForm(RE::TESObjectREFR* a_refr) const
    {
		if (a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				RE::TESObjectREFRPtr target = nullptr;
				const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(a_refr));
				if (Utils::GetCurrentTarget(actor, targetType, target)) {
					if (boolComponent->GetBoolValue()) {
						if (const auto targetBase = target->GetBaseObject()) {
							return targetBase;
						}
					} else {
						return target.get();
					}
				}
			}
		}

		return nullptr;
    }

    RE::BSString CurrentTargetDistanceCondition::GetArgument() const
    {
		const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(nullptr));
		const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

		return std::format("Distance to {} {} {}", GetTargetTypeName(targetType), separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString CurrentTargetDistanceCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
		if (a_refr) {
			if (const auto target = GetTarget(a_refr)) {
				const auto refrPos = a_refr->GetPosition();
				const auto targetPos = target->GetPosition();
			    return std::to_string(refrPos.GetDistance(targetPos)).data();
			}
		}

        return TargetConditionBase::GetCurrent(a_refr);
    }

    bool CurrentTargetDistanceCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
		if (a_refr) {
			if (const auto target = GetTarget(a_refr)) {
				const auto refrPos = a_refr->GetPosition();
				const auto targetPos = target->GetPosition();
				const auto numericVal = numericComponent->GetNumericValue(a_refr);
				return comparisonComponent->GetComparisonResult(refrPos.GetSquaredDistance(targetPos), numericVal * numericVal);  // compare squared to avoid sqrt
			}
		}

		return false;
    }

    RE::TESObjectREFRPtr CurrentTargetDistanceCondition::GetTarget(RE::TESObjectREFR* a_refr) const
    {
		if (a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				RE::TESObjectREFRPtr target = nullptr;
				const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(a_refr));
				if (Utils::GetCurrentTarget(actor, targetType, target)) {
					return target;
				}
			}
		}

		return nullptr;
    }

    void CurrentTargetRelationshipCondition::PostInitialize()
    {
        TargetConditionBase::PostInitialize();
		numericComponent->value.getEnumMap = &CurrentTargetRelationshipCondition::GetRelationshipRankEnumMap;
    }

    RE::BSString CurrentTargetRelationshipCondition::GetArgument() const
    {
		const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(nullptr));
		const auto relationshipLevel = static_cast<int32_t>(numericComponent->GetNumericValue(nullptr));

		const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

		return std::format("{} relationship rank {} {}", GetTargetTypeName(targetType), separator, GetRelationshipRankName(relationshipLevel)).data();
    }

    RE::BSString CurrentTargetRelationshipCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
		if (a_refr) {
			if (const auto target = GetTarget(a_refr)) {
				const auto refrNPC = GetActorBase(a_refr);
				const auto targetNPC = GetActorBase(target.get());
				int32_t rank = 0;
				if (Utils::GetRelationshipRank(refrNPC, targetNPC, rank)) {
				    return GetRelationshipRankName(rank);
				}
			}
		}

        return TargetConditionBase::GetCurrent(a_refr);
    }

    bool CurrentTargetRelationshipCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
		if (a_refr) {
			if (const auto target = GetTarget(a_refr)) {
				const auto refrNPC = GetActorBase(a_refr);
				const auto targetNPC = GetActorBase(target.get());
				int32_t rank = 0;
				if (Utils::GetRelationshipRank(refrNPC, targetNPC, rank)) {
					return comparisonComponent->GetComparisonResult(static_cast<float>(rank), numericComponent->GetNumericValue(a_refr));
				}
			}
		}

		return false;
    }

    RE::TESObjectREFRPtr CurrentTargetRelationshipCondition::GetTarget(RE::TESObjectREFR* a_refr) const
    {
		if (a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				RE::TESObjectREFRPtr target = nullptr;
				const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(a_refr));
				if (Utils::GetCurrentTarget(actor, targetType, target)) {
					return target;
				}
			}
		}

		return nullptr;
    }

    RE::TESNPC* CurrentTargetRelationshipCondition::GetActorBase(RE::TESObjectREFR* a_refr) const
    {
		if (const auto actor = a_refr->As<RE::Actor>()) {
		    return actor->GetActorBase();
		}

		return nullptr;
    }

    std::string_view CurrentTargetRelationshipCondition::GetRelationshipRankName(int32_t a_relationshipRank) const
    {
		static auto map = GetRelationshipRankEnumMap();
		if (const auto it = map.find(a_relationshipRank); it != map.end()) {
			return it->second;
		}

		return "(Invalid)"sv;
    }

    std::map<int32_t, std::string_view> CurrentTargetRelationshipCondition::GetRelationshipRankEnumMap()
    {
		std::map<int32_t, std::string_view> enumMap;
		enumMap[-4] = "Archnemesis"sv;
		enumMap[-3] = "Enemy"sv;
		enumMap[-2] = "Foe"sv;
		enumMap[-1] = "Rival"sv;
		enumMap[0] = "Acquaintance"sv;
		enumMap[1] = "Friend"sv;
		enumMap[2] = "Confidant"sv;
		enumMap[3] = "Ally"sv;
		enumMap[4] = "Lover"sv;
		return enumMap;
    }

    RE::BSString CurrentTargetFactionRankCondition::GetArgument() const
    {
		const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(nullptr));
		const auto separator = ComparisonConditionComponent::GetOperatorString(comparisonComponent->comparisonOperator);

		return std::format("{} rank in {} {} {}", GetTargetTypeName(targetType), factionComponent->form.GetArgument(), separator, numericComponent->value.GetArgument()).data();
    }

    RE::BSString CurrentTargetFactionRankCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
    {
		if (a_refr) {
			if (const auto target = GetTarget(a_refr)) {
				return std::to_string(GetFactionRank(target.get())).data();
			}
		}

        return TargetConditionBase::GetCurrent(a_refr);
    }

    bool CurrentTargetFactionRankCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const
    {
		if (factionComponent->IsValid() && a_refr) {
			if (const auto target = GetTarget(a_refr)) {
				const int32_t rank = GetFactionRank(target.get());
			    const float v = numericComponent->GetNumericValue(a_refr);
			    return comparisonComponent->GetComparisonResult(static_cast<float>(rank), v);
			}
		}

		return false;
    }

    RE::TESObjectREFRPtr CurrentTargetFactionRankCondition::GetTarget(RE::TESObjectREFR* a_refr) const
    {
		if (a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				RE::TESObjectREFRPtr target = nullptr;
				const auto targetType = static_cast<Utils::TargetType>(targetTypeComponent->GetNumericValue(a_refr));
				if (Utils::GetCurrentTarget(actor, targetType, target)) {
					return target;
				}
			}
		}

		return nullptr;
    }

    int32_t CurrentTargetFactionRankCondition::GetFactionRank(RE::TESObjectREFR* a_refr) const
    {
		int32_t rank = -2;

		if (factionComponent->IsValid() && a_refr) {
			if (auto faction = factionComponent->GetTESFormValue()->As<RE::TESFaction>()) {
				if (auto actor = a_refr->As<RE::Actor>()) {
					bool bIsPlayer = actor->IsPlayerRef();

					if (actor->IsInFaction(faction)) {
						rank = Actor_GetFactionRank(actor, faction, bIsPlayer);
					}
				}
			}
		}

		return rank;
    }
}
