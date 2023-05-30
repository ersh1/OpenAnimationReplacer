#pragma once
#include <rapidjson/document.h>

#include "BaseConditions.h"

namespace Conditions
{
    [[nodiscard]] std::string_view CorrectLegacyConditionName(std::string_view a_conditionName);
    [[nodiscard]] std::unique_ptr<ICondition> CreateConditionFromString(std::string_view a_line);
    [[nodiscard]] std::unique_ptr<ICondition> CreateConditionFromJson(rapidjson::Value& a_value);
    [[nodiscard]] std::unique_ptr<ICondition> CreateCondition(std::string_view a_conditionName);
    [[nodiscard]] std::unique_ptr<ICondition> DuplicateCondition(const std::unique_ptr<ICondition>& a_conditionToDuplicate);
    [[nodiscard]] std::unique_ptr<ConditionSet> DuplicateConditionSet(ConditionSet* a_conditionSetToDuplicate);

    class MultiConditionBase : public ConditionBase
    {
    public:
        MultiConditionBase()
        {
            conditionsComponent = AddComponent<MultiConditionComponent>("Conditions");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;

        MultiConditionComponent* conditionsComponent;
    };

    class FormConditionBase : public ConditionBase
    {
    public:
        FormConditionBase()
        {
            formComponent = AddComponent<FormConditionComponent>("Form");
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;

        FormConditionComponent* formComponent;
    };

    class KeywordConditionBase : public ConditionBase
    {
    public:
        KeywordConditionBase()
        {
            keywordComponent = AddComponent<KeywordConditionComponent>("Keyword");
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;

        KeywordConditionComponent* keywordComponent;
    };

    class ORCondition : public MultiConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "OR"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if any of the child conditions are true."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class ANDCondition : public MultiConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "AND"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if all of the child conditions are true."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsFormCondition : public FormConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsForm"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref matches the specified form."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsEquippedCondition : public ConditionBase
    {
    public:
        IsEquippedCondition(bool a_bIsLeft = false)
        {
            formComponent = AddComponent<FormConditionComponent>("Form");
            boolComponent = AddComponent<BoolConditionComponent>("Left hand");
            boolComponent->bValue = a_bIsLeft;
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "IsEquipped"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified form equipped in the right or left hand."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;
        BoolConditionComponent* boolComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        RE::TESForm* GetEquippedForm(RE::TESObjectREFR* a_refr) const;
    };

    class IsEquippedTypeCondition : public ConditionBase
    {
    public:
        IsEquippedTypeCondition(bool a_bIsLeft = false)
        {
            numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
            boolComponent = AddComponent<BoolConditionComponent>("Left hand");
            boolComponent->bValue = a_bIsLeft;
        }

        void InitializeLegacy(const char* a_argument) override;

        void PostInitialize() override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "IsEquippedType"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has an item of the specified type equipped in the right or left hand."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        NumericConditionComponent* numericComponent;
        BoolConditionComponent* boolComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        RE::TESForm* GetEquippedForm(RE::TESObjectREFR* a_refr) const;
        int8_t GetEquippedType(RE::TESObjectREFR* a_refr) const;

        static std::map<int32_t, std::string_view> GetEnumMap();
    };

    class IsEquippedHasKeywordCondition : public ConditionBase
    {
    public:
        IsEquippedHasKeywordCondition(bool a_bIsLeft = false)
        {
            keywordComponent = AddComponent<KeywordConditionComponent>("Keyword");
            boolComponent = AddComponent<BoolConditionComponent>("Left hand");
            boolComponent->bValue = a_bIsLeft;
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "IsEquippedHasKeyword"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has an item equipped in the right or left hand that has the specified keyword."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        KeywordConditionComponent* keywordComponent;
        BoolConditionComponent* boolComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        RE::BGSKeywordForm* GetEquippedKeywordForm(RE::TESObjectREFR* a_refr) const;
    };

    class IsEquippedPowerCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsEquippedPower"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified spell equipped in the power slot."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESForm* GetEquippedPower(RE::TESObjectREFR* a_refr);
    };

    class IsWornCondition : public FormConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsWorn"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified form equipped in any slot."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsWornHasKeywordCondition : public KeywordConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsWornHasKeyword"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has an item equipped in any slot that has the specified keyword."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsFemaleCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsFemale"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is female."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsChildCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsChild"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is a child."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsPlayerTeammateCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsPlayerTeammate"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is a teammate of the player."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsInInteriorCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsInInterior"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is in an interior cell."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsInFactionCondition : public FormConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsInFaction"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is in the specified faction."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class HasKeywordCondition : public KeywordConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "HasKeyword"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified keyword."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class HasMagicEffectCondition : public ConditionBase
    {
    public:
        HasMagicEffectCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Form");
            boolComponent = AddComponent<BoolConditionComponent>("Active Effects Only");
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetName() const override { return "HasMagicEffect"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently affected by the specified magic effect."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;
        BoolConditionComponent* boolComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class HasMagicEffectWithKeywordCondition : public ConditionBase
    {
    public:
        HasMagicEffectWithKeywordCondition()
        {
            keywordComponent = AddComponent<KeywordConditionComponent>("Keyword");
            boolComponent = AddComponent<BoolConditionComponent>("Active Effects Only");
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetName() const override { return "HasMagicEffectWithKeyword"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently affected by a magic effect that has the specified keyword."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        KeywordConditionComponent* keywordComponent;
        BoolConditionComponent* boolComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class HasPerkCondition : public FormConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "HasPerk"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified perk."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class HasSpellCondition : public FormConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "HasSpell"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified spell or shout."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class CompareValue : public ConditionBase
    {
    public:
        CompareValue(ComparisonOperator a_comparisonOperator = ComparisonOperator::kEqual)
        {
            numericComponentA = AddComponent<NumericConditionComponent>("Value A");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponentB = AddComponent<NumericConditionComponent>("Value B");

            comparisonComponent->comparisonOperator = a_comparisonOperator;
        }

        CompareValue(ActorValueType a_actorValueType, ComparisonOperator a_comparisonOperator)
        {
            numericComponentA = AddComponent<NumericConditionComponent>("Value A");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponentB = AddComponent<NumericConditionComponent>("Value B");

            numericComponentA->value.SetActorValueType(a_actorValueType);
            comparisonComponent->comparisonOperator = a_comparisonOperator;
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "CompareValue"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Compares two values."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        NumericConditionComponent* numericComponentA;
        NumericConditionComponent* numericComponentB;
        ComparisonConditionComponent* comparisonComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class LevelCondition : public ConditionBase
    {
    public:
        LevelCondition(ComparisonOperator a_comparisonOperator = ComparisonOperator::kEqual)
        {
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");

            comparisonComponent->comparisonOperator = a_comparisonOperator;
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "Level"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Tests the actor's level against the specified value."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        ComparisonConditionComponent* comparisonComponent;
        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsActorBaseCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsActorBase"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's actor base form is the specified form."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESNPC* GetActorBase(RE::TESObjectREFR* a_refr);
    };

    class IsRaceCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsRace"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's race is the specified race."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESRace* GetRace(RE::TESObjectREFR* a_refr);
    };

    class CurrentWeatherCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "CurrentWeather"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the current weather is the specified weather."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class CurrentGameTimeCondition : public ConditionBase
    {
    public:
        CurrentGameTimeCondition(ComparisonOperator a_comparisonOperator = ComparisonOperator::kEqual)
        {
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");

            comparisonComponent->comparisonOperator = a_comparisonOperator;
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "CurrentGameTime"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Tests the current game time against the specified time."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        ComparisonConditionComponent* comparisonComponent;
        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        [[nodiscard]] static float GetHours();
    };

    class RandomCondition : public ConditionBase
    {
    public:
        RandomCondition()
        {
            randomComponent = AddComponent<RandomConditionComponent>("Random Value");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");

            comparisonComponent->SetComparisonOperator(ComparisonOperator::kLess);
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetName() const override { return "Random"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Compares a random value with a numeric value."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        RandomConditionComponent* randomComponent;
        ComparisonConditionComponent* comparisonComponent;
        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsUniqueCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsUnique"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is flagged as unique."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsClassCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsClass"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's class is the specified class."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESClass* GetTESClass(RE::TESObjectREFR* a_refr);
    };

    class IsCombatStyleCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsCombatStyle"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's combat style is the specified combat style."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESCombatStyle* GetCombatStyle(RE::TESObjectREFR* a_refr);
    };

    class IsVoiceTypeCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsVoiceType"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's voice type is the specified voice type."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::BGSVoiceType* GetVoiceType(RE::TESObjectREFR* a_refr);
    };

    class IsAttackingCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsAttacking"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is attacking."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsRunningCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsRunning"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is running."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsSneakingCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsSneaking"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is sneaking."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsSprintingCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsSprinting"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is sprinting."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsInAirCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsInAir"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is in the air."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsInCombatCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsInCombat"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is in combat."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsWeaponDrawnCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsWeaponDrawn"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has a weapon drawn."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsInLocationCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsInLocation"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is in the specified location."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static bool IsLocation(RE::BGSLocation* a_location, RE::BGSLocation* a_locationToCompare);
    };

    class HasRefTypeCondition : public ConditionBase
    {
    public:
        HasRefTypeCondition()
        {
            locRefTypeComponent = AddComponent<LocRefTypeConditionComponent>("Location Ref Type");
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "HasRefType"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified LocRefType attached."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        LocRefTypeConditionComponent* locRefTypeComponent;

        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsParentCellCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsParentCell"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently in the specified cell."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsWorldSpaceCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsWorldSpace"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently in the specified worldspace."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class FactionRankCondition : public ConditionBase
    {
    public:
        FactionRankCondition(ComparisonOperator a_comparisonOperator = ComparisonOperator::kEqual)
        {
            factionComponent = AddComponent<FormConditionComponent>("Faction");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");

            comparisonComponent->comparisonOperator = a_comparisonOperator;
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "FactionRank"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Tests the ref's faction rank against the specified rank."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* factionComponent;
        ComparisonConditionComponent* comparisonComponent;
        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

        int32_t GetFactionRank(RE::TESObjectREFR* a_refr) const;
    };

    class IsMovementDirectionCondition : public ConditionBase
    {
    public:
        IsMovementDirectionCondition()
        {
            numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
        }

        void InitializeLegacy(const char* a_argument) override;
        void PostInitialize() override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsMovementDirection"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is moving in the specified direction."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

        static std::map<int32_t, std::string_view> GetEnumMap();
    };

    // ========== END OF LEGACY CONDITIONS ==========

    class IsEquippedShoutCondition : public FormConditionBase
    {
    public:
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsEquippedShout"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified shout equipped."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESShout* GetEquippedShout(RE::TESObjectREFR* a_refr);
    };

    class HasGraphVariableCondition : public ConditionBase
    {
    public:
        HasGraphVariableCondition()
        {
            textComponent = AddComponent<TextConditionComponent>("Graph Variable Name");
            //numericComponent = AddComponent<NumericConditionComponent>("Graph Variable Type");
        }

        //void PostInitialize() override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetName() const override { return "HasGraphVariable"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified graph variable."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        TextConditionComponent* textComponent;
        //NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

        //static std::map<int32_t, std::string_view> GetEnumMap();
    };

    class SubmergeLevelCondition : public ConditionBase
    {
    public:
        SubmergeLevelCondition()
        {
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "SubmergeLevel"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Tests the ref's water submerge level (0-1) against a numeric value."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        ComparisonConditionComponent* comparisonComponent;
        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsReplacerEnabledCondition : public ConditionBase
    {
    public:
        IsReplacerEnabledCondition()
        {
            textComponentMod = AddComponent<TextConditionComponent>("Replacer mod name");
            textComponentSubmod = AddComponent<TextConditionComponent>("Submod name");

            textComponentMod->SetAllowSpaces(true);
            textComponentSubmod->SetAllowSpaces(true);
        }

        [[nodiscard]] RE::BSString GetArgument() const override;

        [[nodiscard]] RE::BSString GetName() const override { return "IsReplacerEnabled"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if there's a replacer submod enabled with the given name. Leave the submod name empty to check if any submods are enabled in the replacer mod."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        TextConditionComponent* textComponentMod;
        TextConditionComponent* textComponentSubmod;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

	class IsCurrentPackageCondition : public FormConditionBase
	{
	public:
		RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
		[[nodiscard]] RE::BSString GetName() const override { return "IsCurrentPackage"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's currently running the specified package."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		static RE::TESPackage* GetCurrentPackage(RE::TESObjectREFR* a_refr);
	};

	class IsWornInSlotHasKeywordCondition : public ConditionBase
	{
	public:
		IsWornInSlotHasKeywordCondition()
		{
			slotComponent = AddComponent<NumericConditionComponent>("Slot");
			keywordComponent = AddComponent<KeywordConditionComponent>("Keyword");
		}

		void PostInitialize() override;

		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] RE::BSString GetName() const override { return "IsWornInSlotHasKeyword"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has an item worn in the specified slot that has the specified keyword."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		NumericConditionComponent* slotComponent;
		KeywordConditionComponent* keywordComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

		static std::map<int32_t, std::string_view> GetEnumMap();
	};

    class ScaleCondition : public ConditionBase
    {
    public:
        ScaleCondition()
        {
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "Scale"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Tests the ref's scale against a numeric value."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        ComparisonConditionComponent* comparisonComponent;
        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

	class HeightCondition : public ConditionBase
	{
	public:
		HeightCondition()
		{
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "Height"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the actor's height against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class WeightCondition : public ConditionBase
	{
	public:
		WeightCondition()
		{
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "Weight"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the actor's weight against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class MovementSpeedCondition : public ConditionBase
	{
	public:
		MovementSpeedCondition()
		{
			movementTypeComponent = AddComponent<NumericConditionComponent>("Movement Type");
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
		}

		void PostInitialize() override;

		[[nodiscard]] RE::BSString GetArgument() const override;
		RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "MovementSpeed"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the ref's movement speed of a given type against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		NumericConditionComponent* movementTypeComponent;
		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		float GetMovementSpeed(RE::Actor* a_actor) const;

		static std::map<int32_t, std::string_view> GetEnumMap();
	};

	class CurrentMovementSpeedCondition : public ConditionBase
	{
	public:
		CurrentMovementSpeedCondition()
		{
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
		}
		
		[[nodiscard]] RE::BSString GetArgument() const override;
		RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentMovementSpeed"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the ref's current movement speed against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class WindSpeedCondition : public ConditionBase
	{
	public:
		WindSpeedCondition()
		{
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "WindSpeed"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the current weather's wind speed against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		static float GetWindSpeed();
	};

	class WindAngleDifferenceCondition : public ConditionBase
	{
	public:
		WindAngleDifferenceCondition()
		{
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
			degreesComponent = AddComponent<BoolConditionComponent>("Degrees");
			absoluteComponent = AddComponent<BoolConditionComponent>("Absolute");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "WindAngleDifference"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the difference between current weather's wind angle and the ref's angle against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;
		BoolConditionComponent* degreesComponent;
		BoolConditionComponent* absoluteComponent;

	protected:
	    bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		float GetWindAngleDifference(RE::TESObjectREFR* a_refr) const;
	};

	class CrimeGoldCondition : public ConditionBase
    {
	public:
		CrimeGoldCondition()
		{
			factionComponent = AddComponent<FormConditionComponent>("Faction");
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric Value");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CrimeGold"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the actor's current crime gold against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		FormConditionComponent* factionComponent;
		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };
}
