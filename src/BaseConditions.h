#pragma once
#include <imgui.h>
#include <imgui_stdlib.h>
#include <rapidjson/document.h>

#include "API/OpenAnimationReplacer-ConditionTypes.h"
#include "API/OpenAnimationReplacer-SharedTypes.h"
#include "Containers.h"
#include "SharedTypes.h"
#include "Utils.h"

namespace Conditions
{
	class ConditionBase : public ICondition
	{
	public:
		bool Evaluate(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const override
		{
			if (_bDisabled) {
				return true;
			}

			return _bNegated ? !EvaluateImpl(a_refr, a_clipGenerator, a_parentSubMod) : EvaluateImpl(a_refr, a_clipGenerator, a_parentSubMod);
		}

		void Initialize(void* a_value) override;
		void Serialize(void* a_value, void* a_allocator, ICondition* a_outerCustomCondition = nullptr) override;

		void PostInitialize() override;

		[[nodiscard]] RE::BSString GetArgument() const override { return ""sv; }
		[[nodiscard]] RE::BSString GetCurrent([[maybe_unused]] RE::TESObjectREFR* a_refr) const override { return ""sv.data(); }

		[[nodiscard]] RE::BSString GetRequiredPluginName() const override { return ""sv.data(); }
		[[nodiscard]] RE::BSString GetRequiredPluginAuthor() const override { return ""sv.data(); }

		[[nodiscard]] bool IsDisabled() const override { return _bDisabled; }
		void SetDisabled(bool a_bDisabled) override { _bDisabled = a_bDisabled; }
		[[nodiscard]] bool IsNegated() const override { return _bNegated; }
		void SetNegated(bool a_bNegated) override { _bNegated = a_bNegated; }

		[[nodiscard]] uint32_t GetNumComponents() const override { return static_cast<uint32_t>(_components.size()); }
		[[nodiscard]] IConditionComponent* GetComponent(uint32_t a_index) const override;
		IConditionComponent* AddComponent(ConditionComponentFactory a_factory, const char* a_name, const char* a_description = "") override;

		[[nodiscard]] ConditionAPIVersion GetConditionAPIVersion() const override { return ConditionAPIVersion::kNew; }
		[[nodiscard]] ICondition* GetWrappedCondition() const override { return nullptr; }

		[[nodiscard]] ConditionType GetConditionTypeImpl() const override { return ConditionType::kNormal; }

		[[nodiscard]] EssentialState GetEssentialImpl() const override { return _essentialState; }
		void SetEssential(EssentialState a_essentialState) override { _essentialState = a_essentialState; };

		template <typename T>
		T* AddComponent(std::string_view a_name, std::string_view a_description = ""sv)
		{
			auto& component = _components.emplace_back(std::make_unique<T>(this, a_name.data(), a_description.data()));
			return static_cast<T*>(component.get());
		}

		template <typename T>
		std::vector<T*> GetComponentsByType() const
		{
			std::vector<T*> result;
			for (auto& component : _components) {
				if (auto castedComponent = dynamic_cast<T*>(component.get())) {
					result.emplace_back(castedComponent);
				}
			}
			return result;
		}

	protected:
		ConditionBase() = default;

		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const override = 0;

		bool _bDisabled = false;
		bool _bNegated = false;
		EssentialState _essentialState = EssentialState::kEssential;

		std::vector<std::unique_ptr<IConditionComponent>> _components;
	};

	class ConditionSet : public Set<ICondition, ConditionSet>
	{
	public:
		using Set::Set;

		ConditionSet(IMultiConditionComponent* a_parentMultiConditionComponent) :
			_parentMultiConditionComponent(a_parentMultiConditionComponent) {}

		[[nodiscard]] bool EvaluateAll(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, SubMod* a_parentSubMod, bool a_bForceTrace = false) const;
		[[nodiscard]] bool EvaluateAny(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, SubMod* a_parentSubMod, bool a_bForceTrace = false) const;

		void SetAsParentImpl(std::unique_ptr<ICondition>& a_condition);
		[[nodiscard]] bool IsChildOfImpl(ICondition* a_condition);
		[[nodiscard]] SubMod* GetParentSubModImpl() const;
		[[nodiscard]] std::string NumTextImpl() const;
		[[nodiscard]] bool IsDirtyRecursiveImpl() const;
		void SetDirtyRecursiveImpl(bool a_bDirty);

		[[nodiscard]] const ICondition* GetParentCondition() const;

	private:
		IMultiConditionComponent* _parentMultiConditionComponent = nullptr;
	};

	class ConditionPreset : public ConditionSet
	{
	public:
		ConditionPreset(std::string_view a_name, std::string_view a_description) :
			_name(a_name), _description(a_description) {}

		std::string_view GetName() const { return _name; }
		void SetName(std::string_view a_name) { _name = a_name; }

		std::string_view GetDescription() const { return _description; }
		void SetDescription(std::string_view a_description) { _description = a_description; }

	private:
		std::string _name;
		std::string _description;
	};

	class MultiConditionComponent : public IMultiConditionComponent
	{
	public:
		MultiConditionComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			IMultiConditionComponent(a_parentCondition, a_name, a_description)
		{
			conditionSet = std::make_unique<ConditionSet>(this);
		}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI([[maybe_unused]] bool a_bEditable, [[maybe_unused]] float a_firstColumnWidthPercent) override { return false; }

		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] bool IsValid() const override { return !conditionSet->IsEmpty() && conditionSet->IsValid(); }

		[[nodiscard]] ConditionSet* GetConditions() const override { return conditionSet.get(); }
		[[nodiscard]] bool EvaluateAll(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const override;
		[[nodiscard]] bool EvaluateAny(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const override;
		RE::BSVisit::BSVisitControl ForEachCondition(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<ICondition>&)>& a_func) const override;

		[[nodiscard]] bool GetShouldDrawEvaluateResultForChildConditions() const override { return bShouldDrawEvaluateResultForChildConditions; }
		void SetShouldDrawEvaluateResultForChildConditions(bool a_bShouldDraw) override { bShouldDrawEvaluateResultForChildConditions = a_bShouldDraw; }

		std::unique_ptr<ConditionSet> conditionSet;
		bool bShouldDrawEvaluateResultForChildConditions = true;
	};

	class FormConditionComponent : public IFormConditionComponent
	{
	public:
		FormConditionComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			IFormConditionComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override { return form.DisplayInUI(a_bEditable, a_firstColumnWidthPercent); }

		[[nodiscard]] RE::BSString GetArgument() const override { return form.GetArgument().data(); }

		[[nodiscard]] bool IsValid() const override { return form.IsValid(); }

		[[nodiscard]] RE::TESForm* GetTESFormValue() const override { return form.GetValue(); }
		void SetTESFormValue(RE::TESForm* a_form) override { form.SetValue(a_form); }

		Components::TESFormValue<RE::TESForm> form;
	};

	class NumericConditionComponent : public INumericConditionComponent
	{
	public:
		NumericConditionComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			INumericConditionComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override { return value.DisplayInUI(a_bEditable, a_firstColumnWidthPercent); }

		[[nodiscard]] RE::BSString GetArgument() const override { return value.GetArgument().data(); }

		[[nodiscard]] bool IsValid() const override { return value.IsValid(); }

		[[nodiscard]] float GetNumericValue(RE::TESObjectREFR* a_refr) const override { return value.GetValue(a_refr); }

		void SetStaticValue(float a_value) override { value.SetStaticValue(a_value); }
		void SetGlobalVariable(RE::TESGlobal* a_global) override { value.SetGlobalVariable(a_global); }
		void SetActorValue(RE::ActorValue a_actorValue, Components::ActorValueType a_valueType) override { value.SetActorValue(a_actorValue, a_valueType); }
		void SetGraphVariable(const char* a_graphVariableName, Components::GraphVariableType a_valueType) override { value.SetGraphVariable(a_graphVariableName, a_valueType); }

		void SetStaticRange(float a_min, float a_max) { value.SetStaticRange(a_min, a_max); }
		void SetForcedType(Components::NumericValue::Type a_forcedType) { value.SetForcedType(a_forcedType); }

		Components::NumericValue value;
	};

	class NiPoint3ConditionComponent : public INiPoint3ConditionComponent
	{
	public:
		NiPoint3ConditionComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			INiPoint3ConditionComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override { return value.DisplayInUI(a_bEditable, a_firstColumnWidthPercent); }

		[[nodiscard]] RE::BSString GetArgument() const override { return value.GetArgument().data(); }

		[[nodiscard]] bool IsValid() const override { return value.IsValid(); }

		[[nodiscard]] const RE::NiPoint3& GetNiPoint3Value() const override { return value.GetValue(); }
		void SetNiPoint3Value(const RE::NiPoint3& a_point) override { value.SetValue(a_point); }

		Components::NiPoint3Value value;
	};

	class KeywordConditionComponent : public IKeywordConditionComponent
	{
	public:
		KeywordConditionComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			IKeywordConditionComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override { return keyword.DisplayInUI(a_bEditable, a_firstColumnWidthPercent); }

		[[nodiscard]] RE::BSString GetArgument() const override { return keyword.GetArgument().data(); }

		[[nodiscard]] bool IsValid() const override { return keyword.IsValid(); }

		[[nodiscard]] bool HasKeyword(const RE::BGSKeywordForm* a_form) const override { return keyword.HasKeyword(a_form); }
		void SetKeyword(RE::BGSKeyword* a_keyword) override { keyword.SetKeyword(a_keyword); }
		void SetLiteral(const char* a_literal) override { keyword.SetLiteral(a_literal); }

		Components::KeywordValue<RE::BGSKeyword> keyword;
	};

	class LocRefTypeConditionComponent : public IKeywordConditionComponent
	{
	public:
		LocRefTypeConditionComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			IKeywordConditionComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override { return locRefType.DisplayInUI(a_bEditable, a_firstColumnWidthPercent); }

		[[nodiscard]] RE::BSString GetArgument() const override { return locRefType.GetArgument().data(); }

		[[nodiscard]] bool IsValid() const override { return locRefType.IsValid(); }

		[[nodiscard]] bool HasKeyword(const RE::BGSKeywordForm* a_form) const override { return locRefType.HasKeyword(a_form); }
		void SetKeyword(RE::BGSKeyword* a_keyword) override { locRefType.SetKeyword(static_cast<RE::BGSLocationRefType*>(a_keyword)); }
		void SetLiteral(const char* a_literal) override { locRefType.SetLiteral(a_literal); }

		Components::KeywordValue<RE::BGSLocationRefType> locRefType;
	};

	class TextConditionComponent : public ITextConditionComponent
	{
	public:
		TextConditionComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			ITextConditionComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override { return text.DisplayInUI(a_bEditable, a_firstColumnWidthPercent); }

		[[nodiscard]] RE::BSString GetArgument() const override { return text.GetValue(); }

		[[nodiscard]] bool IsValid() const override { return true; }

		[[nodiscard]] RE::BSString GetTextValue() const override { return text.GetValue(); }
		void SetTextValue(const char* a_text) override { text.SetValue(a_text); }
		void SetAllowSpaces(bool a_bAllowSpaces) override { text.SetAllowSpaces(a_bAllowSpaces); }

		Components::TextValue text;
	};

	class BoolConditionComponent : public IBoolConditionComponent
	{
	public:
		BoolConditionComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			IBoolConditionComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override;

		[[nodiscard]] RE::BSString GetArgument() const override { return bValue ? "true"sv : "false"sv; }

		[[nodiscard]] bool IsValid() const override { return true; }

		[[nodiscard]] bool GetBoolValue() const override { return bValue; }
		void SetBoolValue(bool a_value) override { bValue = a_value; }

		bool bValue = false;
	};

	class ComparisonConditionComponent : public IComparisonConditionComponent
	{
	public:
		ComparisonConditionComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			IComparisonConditionComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override;

		[[nodiscard]] RE::BSString GetArgument() const override { return GetOperatorString(comparisonOperator); }

		[[nodiscard]] bool IsValid() const override { return true; }

		[[nodiscard]] bool GetComparisonResult(float a_valueA, float a_valueB) const override;
		[[nodiscard]] ComparisonOperator GetComparisonOperator() const override { return comparisonOperator; }
		void SetComparisonOperator(ComparisonOperator a_operator) override { comparisonOperator = a_operator; }

		[[nodiscard]] RE::BSString GetComparisonOperatorFullName() const override { return GetOperatorFullName(comparisonOperator); }

		ComparisonOperator comparisonOperator = ComparisonOperator::kEqual;

		static constexpr std::string_view GetOperatorString(ComparisonOperator a_operator)
		{
			switch (a_operator) {
			case ComparisonOperator::kEqual:
				return "=="sv;
			case ComparisonOperator::kNotEqual:
				return "!="sv;
			case ComparisonOperator::kGreater:
				return ">"sv;
			case ComparisonOperator::kGreaterEqual:
				return ">="sv;
			case ComparisonOperator::kLess:
				return "<"sv;
			case ComparisonOperator::kLessEqual:
				return "<="sv;
			default:
				return ""sv;
			}
		}

		static constexpr std::string_view GetOperatorFullName(ComparisonOperator a_operator)
		{
			switch (a_operator) {
			case ComparisonOperator::kEqual:
				return "Equal"sv;
			case ComparisonOperator::kNotEqual:
				return "Not equal"sv;
			case ComparisonOperator::kGreater:
				return "Greater"sv;
			case ComparisonOperator::kGreaterEqual:
				return "Greater or equal"sv;
			case ComparisonOperator::kLess:
				return "Less"sv;
			case ComparisonOperator::kLessEqual:
				return "Less or equal"sv;
			default:
				return "(Invalid)"sv;
			}
		}
	};

	class ConditionStateComponent : public IConditionStateComponent
	{
	public:
		ConditionStateComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			IConditionStateComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override;

		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] bool IsValid() const override { return true; }

		[[nodiscard]] StateDataScope GetAllowedDataScopes() const override { return allowedScopes; };
		void SetAllowedDataScopes(StateDataScope a_scopes) override { allowedScopes = a_scopes; };
		[[nodiscard]] StateDataScope GetStateDataScope() const override { return scope; }
		void SetStateDataScope(StateDataScope a_scope) override { scope = a_scope; }

		[[nodiscard]] bool CanResetOnLoopOrEcho() const override { return bCanResetOnLoopOrEcho; }
		void SetCanResetOnLoopOrEcho(bool a_bCanResetOnLoopOrEcho) override { bCanResetOnLoopOrEcho = a_bCanResetOnLoopOrEcho; }
		[[nodiscard]] bool ShouldResetOnLoopOrEcho() const override { return bShouldResetOnLoopOrEcho; }
		void SetShouldResetOnLoopOrEcho(bool a_bShouldReset) override { bShouldResetOnLoopOrEcho = a_bShouldReset; }

		[[nodiscard]] IStateData* CreateStateData(ConditionStateDataFactory a_stateDataFactory) override;

		[[nodiscard]] IStateData* GetStateData(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const override;
		[[nodiscard]] IStateData* AddStateData(IStateData* a_stateData, RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) override;

		StateDataScope allowedScopes = StateDataScope::kLocal | StateDataScope::kSubMod;
		StateDataScope scope = StateDataScope::kLocal;
		bool bCanResetOnLoopOrEcho = true;
		bool bShouldResetOnLoopOrEcho = false;

		[[nodiscard]] static constexpr std::string_view GetStateDataScopeName(StateDataScope a_scope)
		{
			switch (a_scope) {
			case StateDataScope::kLocal:
				return "Local"sv;
			case StateDataScope::kSubMod:
				return "Submod"sv;
			case StateDataScope::kReplacerMod:
				return "Replacer mod"sv;
			case StateDataScope::kReference:
				return "Reference"sv;
			}

			return "INVALID"sv;
		}

		[[nodiscard]] static constexpr std::string_view GetStateDataScopeTooltip(StateDataScope a_scope)
		{
			switch (a_scope) {
			case StateDataScope::kLocal:
				return "The state data is unique for the combination of this reference, this instance of the condition, and the relevant animation clip."sv;
			case StateDataScope::kSubMod:
				return "The state data is unique for this reference, and is shared between all instances of the condition in the same submod."sv;
			case StateDataScope::kReplacerMod:
				return "The state data is unique for this reference, and is shared between all instances of the condition in all submods in the same replacer mod."sv;
			case StateDataScope::kReference:
				return "The state data is unique for this reference, and is shared between all instances of the condition."sv;
			}

			return "INVALID"sv;
		}

		[[nodiscard]] static constexpr StateDataScope GetStateDataScopeFromName(std::string_view a_name)
		{
			if (a_name == "Local"sv) {
				return StateDataScope::kLocal;
			}
			if (a_name == "Submod"sv) {
				return StateDataScope::kSubMod;
			}
			if (a_name == "Replacer mod"sv) {
				return StateDataScope::kReplacerMod;
			}
			if (a_name == "Reference"sv) {
				return StateDataScope::kReference;
			}

			return StateDataScope::kLocal;
		}

	protected:
		bool CanSelectScope() const;
	};

	class ConditionPresetComponent : public IMultiConditionComponent
	{
	public:
		ConditionPresetComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			IMultiConditionComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI([[maybe_unused]] bool a_bEditable, [[maybe_unused]] float a_firstColumnWidthPercent) override;

		[[nodiscard]] ConditionComponentType GetType() const override { return ConditionComponentType::kPreset; }
		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] bool IsValid() const override { return conditionPreset && !conditionPreset->IsEmpty() && conditionPreset->IsValid(); }

		[[nodiscard]] ConditionSet* GetConditions() const override { return conditionPreset; }
		[[nodiscard]] bool EvaluateAll(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const override;
		[[nodiscard]] bool EvaluateAny(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const override;
		RE::BSVisit::BSVisitControl ForEachCondition(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<ICondition>&)>& a_func) const override;

		[[nodiscard]] bool GetShouldDrawEvaluateResultForChildConditions() const override { return bShouldDrawEvaluateResultForChildConditions; }
		void SetShouldDrawEvaluateResultForChildConditions(bool a_bShouldDraw) override { bShouldDrawEvaluateResultForChildConditions = a_bShouldDraw; }

		void TryFindPreset();

		ConditionPreset* conditionPreset = nullptr;
		bool bShouldDrawEvaluateResultForChildConditions = true;

	protected:
		ReplacerMod* GetParentMod() const;

		std::string _presetName{};
	};
}
