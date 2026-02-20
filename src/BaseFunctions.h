#pragma once

#include <rapidjson/document.h>

#include "Containers.h"
#include "SharedTypes.h"

#include "API/OpenAnimationReplacer-FunctionTypes.h"

class SubMod;

namespace Functions
{
	class FunctionBase : public IFunction
	{
	public:
		bool Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger = nullptr) const override
		{
			if (!_bDisabled) {
				return RunImpl(a_refr, a_clipGenerator, a_parentSubMod, a_trigger);
			}

			return false;
		}

		void Initialize(void* a_value) override;
		void Serialize(void* a_value, void* a_allocator, IFunction* a_outerCustomFunction = nullptr) override;

		void PostInitialize() override;

		[[nodiscard]] RE::BSString GetArgument() const override { return ""sv; }

		[[nodiscard]] RE::BSString GetRequiredPluginName() const override { return ""sv.data(); }
		[[nodiscard]] RE::BSString GetRequiredPluginAuthor() const override { return ""sv.data(); }

		[[nodiscard]] bool IsDisabled() const override { return _bDisabled; }
		void SetDisabled(bool a_bDisabled) override { _bDisabled = a_bDisabled; }
		[[nodiscard]] EssentialState GetEssential() const override { return _essentialState; };
		void SetEssential(EssentialState a_essentialState) override { _essentialState = a_essentialState; };

		[[nodiscard]] uint32_t GetNumComponents() const override { return static_cast<uint32_t>(_components.size()); }
		[[nodiscard]] IFunctionComponent* GetComponent(uint32_t a_index) const override;
		IFunctionComponent* AddComponent(FunctionComponentFactory a_factory, const char* a_name, const char* a_description = "") override;

		[[nodiscard]] FunctionAPIVersion GetFunctionAPIVersion() const override { return FunctionAPIVersion::Latest; }
		[[nodiscard]] FunctionType GetFunctionType() const override { return FunctionType::kNormal; }
		[[nodiscard]] IFunction* GetWrappedFunction() const override { return nullptr; }

		[[nodiscard]] bool HasTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload) const override;
		void AddTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload) override;
		void RemoveTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload) override;
		RE::BSVisit::BSVisitControl ForEachTrigger(const std::function<RE::BSVisit::BSVisitControl(const Trigger&)>& a_func) const override;

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
		FunctionBase() = default;

		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override = 0;

		bool _bDisabled = false;
		EssentialState _essentialState = EssentialState::kEssential;

		std::vector<std::unique_ptr<IFunctionComponent>> _components;

		mutable SharedLock _triggersLock;
		std::set<Trigger> _triggers;
	};

	enum class FunctionSetType : uint8_t
	{
		kNone,
		kOnActivate,
		kOnDeactivate,
		kOnTrigger
	};

	class FunctionSet : public Set<IFunction, FunctionSet>
	{
	public:
		using Set::Set;

		FunctionSet(FunctionSetType a_type) :
			_type(a_type) {}

		FunctionSet(SubMod* a_parentSubMod, FunctionSetType a_type) :
			Set(a_parentSubMod), _type(a_type) {}

		FunctionSet(IMultiFunctionComponent* a_parentMultiFunctionComponent) :
			_parentMultiFunctionComponent(a_parentMultiFunctionComponent) {}

		bool Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, SubMod* a_parentSubMod, Trigger* a_trigger = nullptr) const;

		void SetAsParentImpl(std::unique_ptr<IFunction>& a_function);
		[[nodiscard]] bool IsChildOfImpl(IFunction* a_function);
		[[nodiscard]] SubMod* GetParentSubModImpl() const;
		[[nodiscard]] std::string NumTextImpl() const;
		[[nodiscard]] bool IsDirtyRecursiveImpl() const;
		void SetDirtyRecursiveImpl(bool a_bDirty);

		[[nodiscard]] const IFunction* GetParentFunction() const;
		FunctionSetType GetFunctionSetType() const { return _type; }

	protected:
		FunctionSetType _type = FunctionSetType::kNone;

	private:
		IMultiFunctionComponent* _parentMultiFunctionComponent = nullptr;
	};

	class MultiFunctionComponent : public IMultiFunctionComponent
	{
	public:
		MultiFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IMultiFunctionComponent(a_parentFunction, a_name, a_description)
		{
			functionSet = std::make_unique<FunctionSet>(this);
		}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool, float) override { return false; }

		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] bool IsValid() const override { return !functionSet->IsEmpty() && functionSet->IsValid(); }

		[[nodiscard]] FunctionSet* GetFunctions() const override { return functionSet.get(); }
		bool Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, void* a_trigger) const override;
		RE::BSVisit::BSVisitControl ForEachFunction(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<IFunction>&)>& a_func) const override;

		std::unique_ptr<FunctionSet> functionSet;
	};

	class FormFunctionComponent : public IFormFunctionComponent
	{
	public:
		FormFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IFormFunctionComponent(a_parentFunction, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override { return form.DisplayInUI(a_bEditable, a_firstColumnWidthPercent); }

		[[nodiscard]] RE::BSString GetArgument() const override { return form.GetArgument().data(); }

		[[nodiscard]] bool IsValid() const override { return form.IsValid(); }

		[[nodiscard]] RE::TESForm* GetTESFormValue() const override { return form.GetValue(); }
		void SetTESFormValue(RE::TESForm* a_form) override { form.SetValue(a_form); }

		Components::TESFormValue<RE::TESForm> form;
	};

	class NumericFunctionComponent : public INumericFunctionComponent
	{
	public:
		NumericFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			INumericFunctionComponent(a_parentFunction, a_name, a_description) {}

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
		[[nodiscard]] Components::ActorValueType GetActorValueType() const override { return value.GetActorValueType(); }
		[[nodiscard]] RE::ActorValue GetActorValue() const override { return value.GetActorValue(); }
		[[nodiscard]] Components::GraphVariableType GetGraphVariableType() const override { return value.GetGraphVariableType(); }
		[[nodiscard]] RE::BSString GetGraphVariableName() const override { return value.GetGraphVariableName(); }

		void SetStaticRange(float a_min, float a_max) { value.SetStaticRange(a_min, a_max); }
		void SetStaticInteger(bool a_bIsInteger) { value.SetStaticInteger(a_bIsInteger); }
		void SetForcedType(Components::NumericValue::Type a_forcedType) { value.SetForcedType(a_forcedType); }

		Components::NumericValue value;
	};

	class NiPoint3FunctionComponent : public INiPoint3FunctionComponent
	{
	public:
		NiPoint3FunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			INiPoint3FunctionComponent(a_parentFunction, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override { return value.DisplayInUI(a_bEditable, a_firstColumnWidthPercent); }

		[[nodiscard]] RE::BSString GetArgument() const override { return value.GetArgument().data(); }

		[[nodiscard]] bool IsValid() const override { return value.IsValid(); }

		[[nodiscard]] const RE::NiPoint3& GetNiPoint3Value() const override { return value.GetValue(); }
		void SetNiPoint3Value(const RE::NiPoint3& a_point) override { value.SetValue(a_point); }

		Components::NiPoint3Value value;
	};

	class KeywordFunctionComponent : public IKeywordFunctionComponent
	{
	public:
		KeywordFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IKeywordFunctionComponent(a_parentFunction, a_name, a_description) {}

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

	class TextFunctionComponent : public ITextFunctionComponent
	{
	public:
		TextFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			ITextFunctionComponent(a_parentFunction, a_name, a_description) {}

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

	class BoolFunctionComponent : public IBoolFunctionComponent
	{
	public:
		BoolFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IBoolFunctionComponent(a_parentFunction, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override;

		[[nodiscard]] RE::BSString GetArgument() const override { return bValue ? "true"sv : "false"sv; }

		[[nodiscard]] bool IsValid() const override { return true; }

		[[nodiscard]] bool GetBoolValue() const override { return bValue; }
		void SetBoolValue(bool a_value) override { bValue = a_value; }

		bool bValue = false;
	};

	class ConditionFunctionComponent : public IConditionFunctionComponent
	{
	public:
		ConditionFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IConditionFunctionComponent(a_parentFunction, a_name, a_description)
		{
			conditionSet = std::make_unique<Conditions::ConditionSet>();
			functionSet = std::make_unique<FunctionSet>(FunctionSetType::kNone);
		}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI([[maybe_unused]] bool a_bEditable, [[maybe_unused]] float a_firstColumnWidthPercent) override { return false; }

		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] bool IsValid() const override;

		[[nodiscard]] Conditions::ConditionSet* GetConditions() const { return conditionSet.get(); }
		[[nodiscard]] FunctionSet* GetFunctions() const { return functionSet.get(); }
		bool TryRun(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger = nullptr) const override;
		RE::BSVisit::BSVisitControl ForEachCondition(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<Conditions::ICondition>&)>& a_func) const override;
		RE::BSVisit::BSVisitControl ForEachFunction(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<IFunction>&)>& a_func) const override;

		std::unique_ptr<Conditions::ConditionSet> conditionSet;
		std::unique_ptr<FunctionSet> functionSet;
	};
}
