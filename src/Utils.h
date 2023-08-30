#pragma once

#include "API/OpenAnimationReplacerAPI-Conditions.h"
#include "MergeMapperPluginAPI.h"

namespace Utils
{
    enum class TargetType : int32_t
    {
        kTarget,
        kCombatTarget,
		kDialogueTarget,
        kFollowTarget,
        kHeadtrackTarget,
        kPackageTarget,

		kAnyTarget
    };

    [[nodiscard]] std::string_view TrimWhitespace(std::string_view a_s);
    [[nodiscard]] std::string_view TrimQuotes(std::string_view a_s);
    [[nodiscard]] std::string_view TrimSquareBrackets(std::string_view a_s);
    [[nodiscard]] std::string_view TrimHexPrefix(std::string_view a_s);
	[[nodiscard]] std::string_view TrimPrefix(std::string_view a_s, std::string_view a_prefix);
    [[nodiscard]] std::string_view GetFileNameWithExtension(std::string_view a_s);
    [[nodiscard]] std::string_view GetFileNameWithoutExtension(std::string_view a_s);

    [[nodiscard]] bool CompareStringsIgnoreCase(std::string_view a_lhs, std::string_view a_rhs);
    [[nodiscard]] bool ContainsStringIgnoreCase(std::string_view a_string, std::string_view a_substring);

    [[nodiscard]] std::string GetFormNameString(const RE::TESForm* a_form);
    [[nodiscard]] std::string GetFormKeywords(RE::TESForm* a_form);
    [[nodiscard]] std::string GetFormKeywords(RE::BGSKeywordForm* a_keywordForm);

	[[nodiscard]] inline float GetRandomFloat(float a_min, float a_max) { return effolkronium::random_static::get<float>(a_min, a_max); }

    [[nodiscard]] inline RE::Actor* GetActorFromHkbCharacter(RE::hkbCharacter* a_hkbCharacter)
    {
        if (a_hkbCharacter) {
            const RE::BShkbAnimationGraph* animGraph = SKSE::stl::adjust_pointer<RE::BShkbAnimationGraph>(a_hkbCharacter, -0xC0);
            return animGraph->holder;
        }

        return nullptr;
    }

    [[nodiscard]] inline RE::hkbCharacterStringData* GetStringDataFromHkbCharacter(RE::hkbCharacter* a_hkbCharacter)
    {
        if (a_hkbCharacter) {
            if (const auto& setup = a_hkbCharacter->setup) {
                if (const auto& characterData = setup->data) {
                    return characterData->stringData.get();
                }
            }
        }

        return nullptr;
    }

    [[nodiscard]] inline std::string_view GetOriginalAnimationName(RE::hkbCharacterStringData* a_stringData, uint16_t a_originalIndex)
    {
        if (a_stringData) {
            auto& animationBundleNames = a_stringData->animationNames;
            if (animationBundleNames.size() > a_originalIndex) {
                return animationBundleNames[a_originalIndex].data();
            }
        }

        return "";
    }

    [[nodiscard]] inline float InterpEaseIn(const float& A, const float& B, float a_alpha, float a_exp)
    {
        const float modifiedAlpha = std::pow(a_alpha, a_exp);
        return std::lerp(A, B, modifiedAlpha);
    }

    [[nodiscard]] inline float InterpEaseOut(const float& A, const float& B, float a_alpha, float a_exp)
    {
        const float modifiedAlpha = 1.f - pow(1.f - a_alpha, a_exp);
        return std::lerp(A, B, modifiedAlpha);
    }

    [[nodiscard]] inline float InterpEaseInOut(const float& A, const float& B, float a_alpha, float a_exp)
    {
        return std::lerp(A, B, (a_alpha < 0.5f) ? InterpEaseIn(0.f, 1.f, a_alpha * 2.f, a_exp) * 0.5f : InterpEaseOut(0.f, 1.f, a_alpha * 2.f - 1.f, a_exp) * 0.5f + 0.5f);
    }

    [[nodiscard]] inline float NormalRelativeAngle(float a_angle)
    {
        while (a_angle > RE::NI_PI)
            a_angle -= RE::NI_TWO_PI;
        while (a_angle < -RE::NI_PI)
            a_angle += RE::NI_TWO_PI;
        return a_angle;
    }

    [[nodiscard]] std::string_view GetActorValueName(RE::ActorValue a_actorValue);

	RE::TESObjectREFR* GetRefrFromObject(RE::NiAVObject* a_object);

    inline void ErrorTooManyAnimations()
    {
        util::report_and_fail("Too many animations! Either increase the animation limit or remove some animations."sv);
    }

    bool ConditionHasRandomResult(Conditions::ICondition* a_condition);
    bool ConditionSetHasRandomResult(Conditions::ConditionSet* a_conditionSet);

    bool GetCurrentTarget(RE::Actor* a_actor, TargetType a_targetType, RE::TESObjectREFRPtr& a_outPtr);
	bool GetTarget(RE::Actor* a_actor, RE::TESObjectREFRPtr& a_outPtr);
	bool GetCombatTarget(RE::Actor* a_actor, RE::TESObjectREFRPtr& a_outPtr);
	bool GetDialogueTarget(RE::Actor* a_actor, RE::TESObjectREFRPtr& a_outPtr);
	bool GetFollowTarget(RE::Actor* a_actor, RE::TESObjectREFRPtr& a_outPtr);
	bool GetHeadtrackTarget(RE::Actor* a_actor, RE::TESObjectREFRPtr& a_outPtr);
	bool GetPackageTarget(RE::Actor* a_actor, RE::TESObjectREFRPtr& a_outPtr);

	bool GetRelationshipRank(RE::TESNPC* a_npc1, RE::TESNPC* a_npc2, int32_t& a_outRank);

    RE::TESForm* GetCurrentFurnitureForm(RE::TESObjectREFR* a_refr, bool a_bCheckBase);

    RE::TESForm* LookupForm(RE::FormID a_localFormID, std::string_view a_modName);

    template <class T>
    T* LookupForm(RE::FormID a_localFormID, std::string_view a_modName)
    {
        auto form = LookupForm(a_localFormID, a_modName);
        if (!form) {
            return nullptr;
        }

        if constexpr (T::FORMTYPE == RE::FormType::None) {
            return static_cast<T*>(form);
        } else {
            return form->Is(T::FORMTYPE) ? static_cast<T*>(form) : nullptr;
        }
    }
}
