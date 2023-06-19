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

    class InvalidCondition : public ConditionBase
    {
    public:
        InvalidCondition() = default;

        InvalidCondition(std::string_view a_argument)
        {
            _argument = a_argument;
        }

        [[nodiscard]] RE::BSString GetArgument() const override { return _argument.data(); }
        [[nodiscard]] RE::BSString GetName() const override { return "! INVALID !"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return _argument.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 0, 0, 0 }; }

        [[nodiscard]] bool IsValid() const override { return false; }

    protected:
        bool EvaluateImpl([[maybe_unused]] RE::TESObjectREFR* a_refr, [[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator) const override { return false; }
        std::string _argument;
    };

    class ORCondition : public ConditionBase
    {
    public:
        ORCondition()
        {
            conditionsComponent = AddComponent<MultiConditionComponent>("Conditions");
        }

        [[nodiscard]] RE::BSString GetArgument() const override { return conditionsComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetName() const override { return "OR"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if any of the child conditions are true."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        [[nodiscard]] bool IsValid() const override { return conditionsComponent->IsValid(); }

        MultiConditionComponent* conditionsComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class ANDCondition : public ConditionBase
    {
    public:
        ANDCondition()
        {
            conditionsComponent = AddComponent<MultiConditionComponent>("Conditions");
        }

        [[nodiscard]] RE::BSString GetArgument() const override { return conditionsComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetName() const override { return "AND"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if all of the child conditions are true."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        [[nodiscard]] bool IsValid() const override { return conditionsComponent->IsValid(); }

        MultiConditionComponent* conditionsComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsFormCondition : public ConditionBase
    {
    public:
        IsFormCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Form");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetName() const override { return "IsForm"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref matches the specified form."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsEquippedCondition : public ConditionBase
    {
    public:
        IsEquippedCondition(bool a_bIsLeft = false)
        {
            formComponent = AddComponent<FormConditionComponent>("Form", "The form that should be equipped.");
            boolComponent = AddComponent<BoolConditionComponent>("Left hand", "Enable to check left hand. Disable to check right hand.");
            boolComponent->bValue = a_bIsLeft;
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            numericComponent = AddComponent<NumericConditionComponent>("Type", "The type that should be equipped in the right or left hand.");
            boolComponent = AddComponent<BoolConditionComponent>("Left hand", "Enable to check left hand. Disable to check right hand.");
            boolComponent->bValue = a_bIsLeft;
        }

        void InitializeLegacy(const char* a_argument) override;

        void PostInitialize() override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            boolComponent = AddComponent<BoolConditionComponent>("Left hand", "Enable to check left hand. Disable to check right hand.");
            boolComponent->bValue = a_bIsLeft;
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "IsEquippedHasKeyword"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has an item equipped in the right or left hand that has the specified keyword."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        KeywordConditionComponent* keywordComponent;
        BoolConditionComponent* boolComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        RE::BGSKeywordForm* GetEquippedKeywordForm(RE::TESObjectREFR* a_refr) const;
    };

    class IsEquippedPowerCondition : public ConditionBase
    {
    public:
        IsEquippedPowerCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Power", "The power that should be matched by the equipped one.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsEquippedPower"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified spell equipped in the power slot."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESForm* GetEquippedPower(RE::TESObjectREFR* a_refr);
    };

    class IsWornCondition : public ConditionBase
    {
    public:
        IsWornCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Form", "The form that should be worn.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetName() const override { return "IsWorn"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified form equipped in any slot."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsWornHasKeywordCondition : public ConditionBase
    {
    public:
        IsWornHasKeywordCondition()
        {
            keywordComponent = AddComponent<KeywordConditionComponent>("Keyword");
        }

        void InitializeLegacy(const char* a_argument) override { keywordComponent->keyword.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return keywordComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetName() const override { return "IsWornHasKeyword"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has an item equipped in any slot that has the specified keyword."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        KeywordConditionComponent* keywordComponent;

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

    class IsInFactionCondition : public ConditionBase
    {
    public:
        IsInFactionCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Faction", "The faction that the evaluated actor should be a member of.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetName() const override { return "IsInFaction"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is in the specified faction."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class HasKeywordCondition : public ConditionBase
    {
    public:
        HasKeywordCondition()
        {
            keywordComponent = AddComponent<KeywordConditionComponent>("Keyword");
        }

        void InitializeLegacy(const char* a_argument) override { keywordComponent->keyword.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return keywordComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetName() const override { return "HasKeyword"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified keyword."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        KeywordConditionComponent* keywordComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class HasMagicEffectCondition : public ConditionBase
    {
    public:
        HasMagicEffectCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Magic effect", "The magic effect to check.");
            boolComponent = AddComponent<BoolConditionComponent>("Active effects only", "Enable to check active magic effects only.");
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
            boolComponent = AddComponent<BoolConditionComponent>("Active effects only", "Enable to check active magic effects only.");
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

    class HasPerkCondition : public ConditionBase
    {
    public:
        HasPerkCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Perk", "The perk that the evaluated ref should have.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetName() const override { return "HasPerk"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified perk."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class HasSpellCondition : public ConditionBase
    {
    public:
        HasSpellCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Spell", "The spell or shout that the evaluated ref should have.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetName() const override { return "HasSpell"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified spell or shout."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class CompareValues : public ConditionBase
    {
    public:
        CompareValues(ComparisonOperator a_comparisonOperator = ComparisonOperator::kEqual)
        {
            numericComponentA = AddComponent<NumericConditionComponent>("Value A");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponentB = AddComponent<NumericConditionComponent>("Value B");

            comparisonComponent->comparisonOperator = a_comparisonOperator;
        }

        CompareValues(ActorValueType a_actorValueType, ComparisonOperator a_comparisonOperator)
        {
            numericComponentA = AddComponent<NumericConditionComponent>("Value A");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponentB = AddComponent<NumericConditionComponent>("Value B");

            numericComponentA->value.SetActorValueType(a_actorValueType);
            comparisonComponent->comparisonOperator = a_comparisonOperator;
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "CompareValues"sv.data(); }
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
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");

            comparisonComponent->comparisonOperator = a_comparisonOperator;
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "Level"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Tests the actor's level against the specified value."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        ComparisonConditionComponent* comparisonComponent;
        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsActorBaseCondition : public ConditionBase
    {
    public:
        IsActorBaseCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Actor base", "The actor base form that should be matched by the evaluated actor.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsActorBase"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's actor base form is the specified form."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESNPC* GetActorBase(RE::TESObjectREFR* a_refr);
    };

    class IsRaceCondition : public ConditionBase
    {
    public:
        IsRaceCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Race", "The race that should be matched by the evaluated actor.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsRace"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's race is the specified race."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESRace* GetRace(RE::TESObjectREFR* a_refr);
    };

    class CurrentWeatherCondition : public ConditionBase
    {
    public:
        CurrentWeatherCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Weather", "The weather (TESWeather) or a form list of weathers (BGSListForm) that should be matched by the current weather.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "CurrentWeather"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the current weather is the specified weather."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class CurrentGameTimeCondition : public ConditionBase
    {
    public:
        CurrentGameTimeCondition(ComparisonOperator a_comparisonOperator = ComparisonOperator::kEqual)
        {
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");

            comparisonComponent->comparisonOperator = a_comparisonOperator;
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            randomComponent = AddComponent<RandomConditionComponent>("Random value");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");

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

    class IsClassCondition : public ConditionBase
    {
    public:
        IsClassCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Class", "The class that should be matched by the evaluated actor.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsClass"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's class is the specified class."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESClass* GetTESClass(RE::TESObjectREFR* a_refr);
    };

    class IsCombatStyleCondition : public ConditionBase
    {
    public:
        IsCombatStyleCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Combat style", "The combat style that should be matched by the evaluated actor.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsCombatStyle"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's combat style is the specified combat style."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESCombatStyle* GetCombatStyle(RE::TESObjectREFR* a_refr);
    };

    class IsVoiceTypeCondition : public ConditionBase
    {
    public:
        IsVoiceTypeCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Voice type", "The voice type that should be matched by the evaluated actor.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsVoiceType"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's voice type is the specified voice type."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

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

    class IsInLocationCondition : public ConditionBase
    {
    public:
        IsInLocationCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Location", "The location that the evaluated ref should be in.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsInLocation"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is in the specified location."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static bool IsLocation(RE::BGSLocation* a_location, RE::BGSLocation* a_locationToCompare);
    };

    class HasRefTypeCondition : public ConditionBase
    {
    public:
        HasRefTypeCondition()
        {
            locRefTypeComponent = AddComponent<LocRefTypeConditionComponent>("Location ref type", "The BGSLocationRefType form that should be checked.");
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "HasRefType"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified LocRefType attached."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        LocRefTypeConditionComponent* locRefTypeComponent;

        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsParentCellCondition : public ConditionBase
    {
    public:
        IsParentCellCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Cell", "The cell that the evaluated ref should be in.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsParentCell"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently in the specified cell."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsWorldSpaceCondition : public ConditionBase
    {
    public:
        IsWorldSpaceCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Worldspace", "The worldspace that the evaluated ref should be in.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsWorldSpace"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently in the specified worldspace."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class FactionRankCondition : public ConditionBase
    {
    public:
        FactionRankCondition(ComparisonOperator a_comparisonOperator = ComparisonOperator::kEqual)
        {
            factionComponent = AddComponent<FormConditionComponent>("Faction", "The faction that should be checked.");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");

            comparisonComponent->comparisonOperator = a_comparisonOperator;
        }

        void InitializeLegacy(const char* a_argument) override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
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
            numericComponent = AddComponent<NumericConditionComponent>("Direction", "The direction that the ref should be moving in.");
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

    class IsEquippedShoutCondition : public ConditionBase
    {
    public:
        IsEquippedShoutCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Shout", "The shout that should be matched by the equipped one.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsEquippedShout"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has the specified shout equipped."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESShout* GetEquippedShout(RE::TESObjectREFR* a_refr);
    };

    class HasGraphVariableCondition : public ConditionBase
    {
    public:
        HasGraphVariableCondition()
        {
            textComponent = AddComponent<TextConditionComponent>("Graph variable name", "The name of the graph variable to check.");
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
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            textComponentMod = AddComponent<TextConditionComponent>("Replacer mod name", "The name of the replacer mod to check.");
            textComponentSubmod = AddComponent<TextConditionComponent>("Submod name", "If empty, any enabled submods of the replacer mod specified above will match the condition.");

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

    class IsCurrentPackageCondition : public ConditionBase
    {
    public:
        IsCurrentPackageCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Package", "The package that should be matched by the evaluated actor's current package.");
        }

        void InitializeLegacy(const char* a_argument) override { formComponent->form.ParseLegacy(a_argument); }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;
        [[nodiscard]] RE::BSString GetName() const override { return "IsCurrentPackage"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's currently running the specified package."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* formComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static RE::TESPackage* GetCurrentPackage(RE::TESObjectREFR* a_refr);
    };

    class IsWornInSlotHasKeywordCondition : public ConditionBase
    {
    public:
        IsWornInSlotHasKeywordCondition()
        {
            slotComponent = AddComponent<NumericConditionComponent>("Slot", "The slot that should be checked.");
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
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            movementTypeComponent = AddComponent<NumericConditionComponent>("Movement type", "The movement type whose speed should be read.");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
        }

        void PostInitialize() override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
            degreesComponent = AddComponent<BoolConditionComponent>("Degrees", "Whether the angle values are in degrees or radians.");
            absoluteComponent = AddComponent<BoolConditionComponent>("Absolute", "Whether the angle values are absolute. Disable to be able to differentiate between clockwise and counterclockwise. Enable if you don't care about that, it should make the comparison easier to do in one condition.");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

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
            factionComponent = AddComponent<FormConditionComponent>("Faction", "The crime faction to check.");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "CrimeGold"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Tests the actor's current crime gold against a numeric value."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

        FormConditionComponent* factionComponent;
        ComparisonConditionComponent* comparisonComponent;
        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsBlockingCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsBlocking"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is blocking."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsCombatStateCondition : public ConditionBase
    {
    public:
        IsCombatStateCondition()
        {
            combatStateComponent = AddComponent<NumericConditionComponent>("Combat state", "The required combat state.");
        }

        void PostInitialize() override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "IsCombatState"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref's current combat state matches the given state."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

        NumericConditionComponent* combatStateComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        RE::ACTOR_COMBAT_STATE GetCombatState(RE::Actor* a_actor) const;
        std::string_view GetCombatStateName(RE::ACTOR_COMBAT_STATE a_state) const;

        static std::map<int32_t, std::string_view> GetEnumMap();
    };

    class InventoryCountCondition : public ConditionBase
    {
    public:
        InventoryCountCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Form", "The form that should be searched for in the inventory.");
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value", "The value to compare the inventory count with.");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "InventoryCount"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Tests the actor's current inventory count of a specified form against a numeric value."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

        FormConditionComponent* formComponent;
        ComparisonConditionComponent* comparisonComponent;
        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		int GetItemCount(RE::TESObjectREFR* a_refr) const;
    };

	class FallDistanceCondition : public ConditionBase
	{
	public:
		FallDistanceCondition()
		{
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "FallDistance"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Gets the actor's current fall distance and tests it against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		static float GetFallDistance(RE::TESObjectREFR* a_refr);
	};

    class FallDamageCondition : public ConditionBase
    {
    public:
        FallDamageCondition()
        {
            comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
            numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
        }

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "FallDamage"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Calculates the actor's fall damage if they landed at this moment and tests it against a numeric value."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

        ComparisonConditionComponent* comparisonComponent;
        NumericConditionComponent* numericComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        static float GetFallDamage(RE::TESObjectREFR* a_refr);
    };

    class CurrentPackageProcedureTypeCondition : public ConditionBase
    {
    public:
        CurrentPackageProcedureTypeCondition()
        {
            packageProcedureTypeComponent = AddComponent<NumericConditionComponent>("Package procedure type", "The required type of the current package procedure.");
        }

        void PostInitialize() override;

        [[nodiscard]] RE::BSString GetArgument() const override;
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "CurrentPackageProcedureType"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the actor's current package procedure is of a given type."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

        NumericConditionComponent* packageProcedureTypeComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
        RE::PACKAGE_PROCEDURE_TYPE GetPackageProcedureType(RE::Actor* a_actor) const;
        std::string_view GetPackageProcedureTypeName(RE::PACKAGE_PROCEDURE_TYPE a_type) const;

        static std::map<int32_t, std::string_view> GetEnumMap();
    };
	
    class IsOnMountCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsOnMount"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is riding a mount."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsRidingCondition : public ConditionBase
    {
    public:
        IsRidingCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Mount", "The form that the actor should be riding.");
            boolComponent = AddComponent<BoolConditionComponent>("Base", "Enable to check the mount's base actor.");
            boolComponent->SetBoolValue(true);
        }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "IsRiding"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is riding the specified form."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

        FormConditionComponent* formComponent;
        BoolConditionComponent* boolComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsRidingHasKeywordCondition : public ConditionBase
    {
    public:
        IsRidingHasKeywordCondition()
        {
            keywordComponent = AddComponent<KeywordConditionComponent>("Keyword");
        }

        [[nodiscard]] RE::BSString GetArgument() const override { return keywordComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "IsRidingHasKeyword"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is riding a form with the specified keyword."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

        KeywordConditionComponent* keywordComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsBeingRiddenCondition : public ConditionBase
    {
    public:
        [[nodiscard]] RE::BSString GetName() const override { return "IsBeingRidden"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently mounted by someone."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class IsBeingRiddenByCondition : public ConditionBase
    {
    public:
        IsBeingRiddenByCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Mount", "The form that the actor should be riding.");
            boolComponent = AddComponent<BoolConditionComponent>("Base", "Enable to check the rider's base actor.");
            boolComponent->SetBoolValue(true);
        }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "IsBeingRiddenBy"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently mounted by the specified form."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

        FormConditionComponent* formComponent;
        BoolConditionComponent* boolComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class CurrentFurnitureCondition : public ConditionBase
    {
    public:
        CurrentFurnitureCondition()
        {
            formComponent = AddComponent<FormConditionComponent>("Furniture", "The furniture form that the actor should be occupying.");
            boolComponent = AddComponent<BoolConditionComponent>("Base", "Enable to check the furniture base object Disable to check the current furniture ref.");
            boolComponent->SetBoolValue(true);
        }

        [[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "CurrentFurniture"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently occupying the specified furniture."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

        FormConditionComponent* formComponent;
        BoolConditionComponent* boolComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

    class CurrentFurnitureHasKeywordCondition : public ConditionBase
    {
    public:
        CurrentFurnitureHasKeywordCondition()
        {
            keywordComponent = AddComponent<KeywordConditionComponent>("Keyword");
            boolComponent = AddComponent<BoolConditionComponent>("Base", "Enable to check the furniture base object. Disable to check the current furniture ref.");
            boolComponent->SetBoolValue(true);
        }

        [[nodiscard]] RE::BSString GetArgument() const override { return keywordComponent->GetArgument(); }
        [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

        [[nodiscard]] RE::BSString GetName() const override { return "CurrentFurnitureHasKeyword"sv.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently occupying furniture with the specified keyword."sv.data(); }
        [[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

        KeywordConditionComponent* keywordComponent;
        BoolConditionComponent* boolComponent;

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
    };

	class TargetConditionBase : public ConditionBase
	{
	public:
		TargetConditionBase()
		{
			targetTypeComponent = AddComponent<NumericConditionComponent>("Target type", "The required target type.");
		}

		void PostInitialize() override;

		NumericConditionComponent* targetTypeComponent;

	protected:
		std::string_view GetTargetTypeName(Utils::TargetType a_targetType) const;
		static std::map<int32_t, std::string_view> GetEnumMap();
	};

	class HasTargetCondition : public TargetConditionBase
	{
	public:
		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] RE::BSString GetName() const override { return "HasTarget"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the actor has a target of a given type."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class CurrentTargetCondition : public TargetConditionBase 
	{
	public:
		CurrentTargetCondition()
		{
			formComponent = AddComponent<FormConditionComponent>("Form");
			boolComponent = AddComponent<BoolConditionComponent>("Base", "Enable to check the form's base object.");
			boolComponent->SetBoolValue(true);
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentTarget"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the actor's current target matches the specified form."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

		FormConditionComponent* formComponent;
		BoolConditionComponent* boolComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

		RE::TESForm* GetTargetForm(RE::TESObjectREFR* a_refr) const;
	};

	class CurrentTargetHasKeywordCondition : public TargetConditionBase
	{
	public:
		CurrentTargetHasKeywordCondition()
		{
			keywordComponent = AddComponent<KeywordConditionComponent>("Keyword");
			boolComponent = AddComponent<BoolConditionComponent>("Base", "Enable to check the form's base object.");
			boolComponent->SetBoolValue(true);
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentTargetHasKeyword"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the actor's current target has the specified keyword."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

		KeywordConditionComponent* keywordComponent;
		BoolConditionComponent* boolComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

		RE::TESForm* GetTargetForm(RE::TESObjectREFR* a_refr) const;
	};

	class CurrentTargetDistanceCondition : public TargetConditionBase
	{
	public:
		CurrentTargetDistanceCondition()
		{
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentTargetDistance"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the distance to an actor's current target against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

		RE::TESObjectREFRPtr GetTarget(RE::TESObjectREFR* a_refr) const;
	};

	class CurrentTargetRelationshipCondition : public TargetConditionBase
	{
	public:
		CurrentTargetRelationshipCondition()
		{
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
		}

		void PostInitialize() override;

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentTargetRelationship"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the relationship between the actor and their current target against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

		RE::TESObjectREFRPtr GetTarget(RE::TESObjectREFR* a_refr) const;
		RE::TESNPC* GetActorBase(RE::TESObjectREFR* a_refr) const;

		std::string_view GetRelationshipRankName(int32_t a_relationshipRank) const;
		static std::map<int32_t, std::string_view> GetRelationshipRankEnumMap();
	};

	class CurrentTargetFactionRankCondition : public TargetConditionBase
	{
	public:
		CurrentTargetFactionRankCondition()
		{
			factionComponent = AddComponent<FormConditionComponent>("Faction", "The faction that should be checked.");
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentTargetFactionRank"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the faction rank of the actor's current target against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 1, 0 }; }

		FormConditionComponent* factionComponent;
		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

		RE::TESObjectREFRPtr GetTarget(RE::TESObjectREFR* a_refr) const;
		int32_t GetFactionRank(RE::TESObjectREFR* a_refr) const;
	};

	class EquippedObjectWeightCondition : public ConditionBase
	{
	public:
		EquippedObjectWeightCondition()
		{
			boolComponent = AddComponent<BoolConditionComponent>("Left hand", "Enable to check left hand. Disable to check right hand.");
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "EquippedObjectWeight"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the weight of the object currently equipped in the right or left hand against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		BoolConditionComponent* boolComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		RE::TESForm* GetEquippedForm(RE::TESObjectREFR* a_refr) const;
	};

	class CastingSourceConditionBase : public ConditionBase
	{
	public:
		CastingSourceConditionBase()
		{
			castingSourceComponent = AddComponent<NumericConditionComponent>("Casting source", "The casting source to check.");
		}

		void PostInitialize() override;

		NumericConditionComponent* castingSourceComponent;

	protected:
		std::string_view GetCastingSourceName(RE::MagicSystem::CastingSource a_source) const;
		static std::map<int32_t, std::string_view> GetCastingSourceEnumMap();
	};

	class CurrentCastingTypeCondition : public CastingSourceConditionBase
	{
	public:
		CurrentCastingTypeCondition()
		{
			castingTypeComponent = AddComponent<NumericConditionComponent>("Casting type", "The required casting type.");
		}

		void PostInitialize() override;

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentCastingType"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the actor's current casting type of the given casting source is the required type."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		NumericConditionComponent* castingTypeComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		bool GetCastingType(RE::Actor* a_actor, RE::MagicSystem::CastingSource a_source, RE::MagicSystem::CastingType& a_outType) const;
		std::string_view GetCastingTypeName(RE::MagicSystem::CastingType a_type) const;

		static std::map<int32_t, std::string_view> GetCastingTypeEnumMap();
	};

	class CurrentDeliveryTypeCondition : public CastingSourceConditionBase
	{
	public:
		CurrentDeliveryTypeCondition()
		{
			deliveryTypeComponent = AddComponent<NumericConditionComponent>("Delivery type", "The required delivery type.");
		}

		void PostInitialize() override;

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentDeliveryType"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the actor's current delivery type of the given casting source is the required type."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		NumericConditionComponent* deliveryTypeComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		bool GetDeliveryType(RE::Actor* a_actor, RE::MagicSystem::CastingSource a_source, RE::MagicSystem::Delivery& a_outDeliveryType) const;
		std::string_view GetDeliveryTypeName(RE::MagicSystem::Delivery a_deliveryType) const;

		static std::map<int32_t, std::string_view> GetDeliveryTypeEnumMap();
	};

	class IsQuestStageDoneCondition : public ConditionBase
	{
	public:
		IsQuestStageDoneCondition()
		{
			questComponent = AddComponent<FormConditionComponent>("Quest form", "The quest to check.");
			stageIndexComponent = AddComponent<NumericConditionComponent>("Stage index", "The quest stage index that should be completed for the given quest.");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] RE::BSString GetName() const override { return "IsQuestStageDone"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the specified stage in the given quest has been completed."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		FormConditionComponent* questComponent;
		NumericConditionComponent* stageIndexComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class CurrentWeatherHasFlagCondition : public ConditionBase
	{
	public:
		CurrentWeatherHasFlagCondition()
		{
			weatherFlagComponent = AddComponent<NumericConditionComponent>("Flag", "The current weather must have this flag enabled.");
			weatherFlagComponent->SetStaticValue(1);
		}

		void PostInitialize() override;

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentWeatherHasFlag"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the current weather has the specified flag enabled."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		NumericConditionComponent* weatherFlagComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		std::string_view GetFlagName(int32_t a_index) const;

		static std::map<int32_t, std::string_view> GetEnumMap();
	};

	class InventoryCountHasKeywordCondition : public ConditionBase
	{
	public:
		InventoryCountHasKeywordCondition()
		{
			keywordComponent = AddComponent<KeywordConditionComponent>("Keyword");
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric value", "The value to compare the inventory count with.");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "InventoryCountHasKeyword"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the actor's current inventory count of all items with a specified keyword against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		KeywordConditionComponent* keywordComponent;
		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
		int GetItemCount(RE::TESObjectREFR* a_refr) const;
	};

	class CurrentTargetRelativeAngleCondition : public TargetConditionBase
	{
	public:
		CurrentTargetRelativeAngleCondition()
		{
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
			degreesComponent = AddComponent<BoolConditionComponent>("Degrees", "Whether the angle values are in degrees or radians.");
			absoluteComponent = AddComponent<BoolConditionComponent>("Absolute", "Whether the angle values are absolute. Disable to be able to differentiate between clockwise and counterclockwise. Enable if you don't care about that, it should make the comparison easier to do in one condition.");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentTargetRelativeAngle"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the relative angle between an actor and their current target."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;
		BoolConditionComponent* degreesComponent;
		BoolConditionComponent* absoluteComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

		RE::TESObjectREFRPtr GetTarget(RE::TESObjectREFR* a_refr) const;
		float GetRelativeAngle(RE::TESObjectREFR* a_refr) const;
	};

	class CurrentTargetLineOfSightCondition : public TargetConditionBase
	{
	public:
		CurrentTargetLineOfSightCondition()
		{
			boolComponent = AddComponent<BoolConditionComponent>("Evaluate target", "Enable to check the target's line of sight to the actor. Disable to check the actor's line of sight to the target.");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentTargetLineOfSight"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the actor's current target is in their line of sight, or if the actor is in their current target's line of sight."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		BoolConditionComponent* boolComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;

		RE::TESObjectREFRPtr GetTarget(RE::TESObjectREFR* a_refr) const;
	};

	class CurrentRotationSpeedCondition : public ConditionBase
	{
	public:
		CurrentRotationSpeedCondition()
		{
			comparisonComponent = AddComponent<ComparisonConditionComponent>("Comparison");
			numericComponent = AddComponent<NumericConditionComponent>("Numeric value");
		}

		[[nodiscard]] RE::BSString GetArgument() const override;
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "CurrentRotationSpeed"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Tests the ref's current rotation speed against a numeric value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		ComparisonConditionComponent* comparisonComponent;
		NumericConditionComponent* numericComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class IsTalkingCondition : public ConditionBase
	{
	public:
		[[nodiscard]] RE::BSString GetName() const override { return "IsTalking"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is talking either in monologue or in dialogue."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class IsGreetingPlayerCondition : public ConditionBase
	{
	public:
		[[nodiscard]] RE::BSString GetName() const override { return "IsGreetingPlayer"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is greeting the player."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class IsInSceneCondition : public ConditionBase
	{
	public:
		[[nodiscard]] RE::BSString GetName() const override { return "IsInScene"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently in a scene."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class IsInSpecifiedSceneCondition : public ConditionBase
	{
	public:
		IsInSpecifiedSceneCondition()
		{
			formComponent = AddComponent<FormConditionComponent>("Scene", "The scene to check.");
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }
		[[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

		[[nodiscard]] RE::BSString GetName() const override { return "IsInSpecifiedScene"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref is currently in the specified scene."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		FormConditionComponent* formComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class IsScenePlayingCondition : public ConditionBase
	{
	public:
		IsScenePlayingCondition()
		{
			formComponent = AddComponent<FormConditionComponent>("Scene", "The scene to check.");
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }

		[[nodiscard]] RE::BSString GetName() const override { return "IsScenePlaying"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if a specific scene is currently playing."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

		FormConditionComponent* formComponent;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};

	class IsDoingFavorCondition : public ConditionBase
	{
	public:
		[[nodiscard]] RE::BSString GetName() const override { return "IsDoingFavor "sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Checks if the ref has been asked to do something by the player."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 1, 2, 0 }; }

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const override;
	};
}
