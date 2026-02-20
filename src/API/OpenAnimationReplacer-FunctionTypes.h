#pragma once

#include "OpenAnimationReplacer-SharedTypes.h"

// a lot of the code is unfortunately duplicated from ConditionTypes, ideally a lot of it should be refactored, but I don't want to break compatibility with older plugins
namespace Conditions
{
	class ICondition;
	class ConditionSet;
}

namespace Functions
{
	class IFunction;
	class IFunctionComponent;

	struct Trigger;

	class FunctionSet;
	
	using FunctionFactory = IFunction* (*)();
	using FunctionComponentFactory = IFunctionComponent* (*)(const IFunction* a_parentFunction, const char* a_name, const char* a_description);

	enum class FunctionType : uint8_t
	{
		kNormal,
		kCustom
	};

	enum class FunctionComponentType : uint8_t
	{
		kMulti,
		kForm,
		kNumeric,
		kNiPoint3,
		kKeyword,
		kText,
		kBool,
		kCondition,

		kCustom
	};

	enum class FunctionAPIVersion : uint8_t
	{
		V1 = 1,

		Latest = V1
	};

	enum class EssentialState : uint8_t
	{
		kEssential,
		kNonEssential_True,
		kNonEssential_False
	};

	struct Trigger
	{
		RE::BSString event{};
		RE::BSString payload{};

		Trigger() = default;

		Trigger(const RE::BSString& a_event, const RE::BSString& a_payload) : 
			event(a_event), payload(a_payload)
		{}

		bool operator<(const Trigger& a_other) const {
			if (std::string_view(event.data()) == std::string_view(a_other.event.data())) {
				return std::string_view(payload.data()) < std::string_view(a_other.payload.data());
			}
			return std::string_view(event.data()) < std::string_view(a_other.event.data());
		}
	};

	// the parent class of all OAR "functions"
	// some arguments are kept as void* pointers to avoid including rapidjson headers. You only need to handle json serialization if you're adding some configurable parameters to your "function"
	// some member functions return a RE::BSString because std::string is unreliable over DLL boundaries
	class IFunction
	{
	public:
		virtual ~IFunction() = default;

		virtual bool Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger = nullptr) const = 0;

		virtual void Initialize(void* a_value) = 0;  // rapidjson::Value* a_value
		virtual void Serialize(void* a_value, void* a_allocator, IFunction* a_outerCustomFunction = nullptr) = 0;  // rapidjson::Value* a_value, rapidjson::Document::AllocatorType* a_allocator

		virtual void PreInitialize() {}
		virtual void PostInitialize() {}

		[[nodiscard]] virtual RE::BSString GetArgument() const = 0;
		[[nodiscard]] virtual RE::BSString GetName() const = 0;
		[[nodiscard]] virtual RE::BSString GetDescription() const = 0;
		[[nodiscard]] virtual REL::Version GetRequiredVersion() const = 0;
		[[nodiscard]] virtual RE::BSString GetRequiredPluginName() const = 0;
		[[nodiscard]] virtual RE::BSString GetRequiredPluginAuthor() const = 0;

		virtual bool DisplayInUI([[maybe_unused]] bool a_bEditable, [[maybe_unused]] float a_firstColumnWidthPercent) { return false; }

		[[nodiscard]] virtual bool IsDisabled() const = 0;
		virtual void SetDisabled(bool a_bDisabled) = 0;
		[[nodiscard]] virtual EssentialState GetEssential() const = 0;
		virtual void SetEssential(EssentialState a_essentialState) = 0;
		[[nodiscard]] virtual bool IsValid() const { return true; }

		[[nodiscard]] virtual uint32_t GetNumComponents() const = 0;
		[[nodiscard]] virtual IFunctionComponent* GetComponent(uint32_t a_index) const = 0;
		virtual IFunctionComponent* AddComponent(FunctionComponentFactory a_factory, const char* a_name, const char* a_description = "") = 0;

		[[nodiscard]] virtual FunctionAPIVersion GetFunctionAPIVersion() const = 0;
		[[nodiscard]] virtual FunctionType GetFunctionType() const = 0;
		[[nodiscard]] virtual IFunction* GetWrappedFunction() const = 0;

		[[nodiscard]] virtual bool IsDeprecated() const { return false; }
		[[nodiscard]] virtual RE::TESObjectREFR* GetRefrToEvaluate(RE::TESObjectREFR* a_refr) const { return a_refr; }

		[[nodiscard]] virtual bool HasTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload = nullptr) const = 0;
		virtual void AddTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload = nullptr) = 0;
		virtual void RemoveTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload = nullptr) = 0;
		virtual RE::BSVisit::BSVisitControl ForEachTrigger(const std::function<RE::BSVisit::BSVisitControl(const Trigger&)>& a_func) const = 0;

		[[nodiscard]] FunctionSet* GetParentSet() const { return _parentSet; }
		void SetParentSet(FunctionSet* a_set) { _parentSet = a_set; }

	protected:
		virtual bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_subMod, Trigger* a_trigger = nullptr) const = 0;

		FunctionSet* _parentSet = nullptr;
	};

	// a function can have many function components
	// these are the configurable variables inside an function (e.g. a form, a number, a keyword, etc.)
	// the existing components should be enough for most functions
	// however you can add your own custom function components by inheriting from ICustomFunctionComponent
	// this will require you to do the serialization yourself however, so it's a bit complex and possibly not safe across DLL boundaries with different compilers, I'm not sure how safe rapidjson is in such cases
	class IFunctionComponent
	{
	public:
		IFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			_parentFunction(a_parentFunction),
			_name(a_name),
			_description(a_description) {}

		virtual ~IFunctionComponent() = default;

		virtual void InitializeComponent(void* a_value) = 0;                    // rapidjson::Value* a_value
		virtual void SerializeComponent(void* a_value, void* a_allocator) = 0;  // rapidjson::Value* a_value, rapidjson::Document::AllocatorType* a_allocator

		virtual void PostInitialize() {}

		virtual bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) = 0;

		[[nodiscard]] virtual FunctionComponentType GetType() const = 0;
		[[nodiscard]] virtual RE::BSString GetArgument() const = 0;
		[[nodiscard]] virtual RE::BSString GetName() const { return _name.data(); }

		[[nodiscard]] virtual RE::BSString GetDescription() const
		{
			if (_description.empty()) {
				return GetDefaultDescription();
			}
			return _description.data();
		}

		[[nodiscard]] virtual RE::BSString GetDefaultDescription() const = 0;
		[[nodiscard]] virtual bool IsValid() const = 0;

		[[nodiscard]] const IFunction* GetParentFunction() const { return _parentFunction; }

	protected:
		const IFunction* _parentFunction = nullptr;

		const std::string _name;
		const std::string _description;
	};

	class IMultiFunctionComponent : public IFunctionComponent
	{
	public:
		inline static constexpr auto FUNCTION_COMPONENT_TYPE = FunctionComponentType::kMulti;
		inline static constexpr auto DEFAULT_DESCRIPTION = "A list of child functions."sv;

		IMultiFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IFunctionComponent(a_parentFunction, a_name, a_description) {}

		[[nodiscard]] FunctionComponentType GetType() const override { return FUNCTION_COMPONENT_TYPE; }
		[[nodiscard]] RE::BSString GetDefaultDescription() const override { return DEFAULT_DESCRIPTION; }

		[[nodiscard]] virtual FunctionSet* GetFunctions() const = 0;
		[[nodiscard]] virtual bool Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, void* a_trigger) const = 0;
		virtual RE::BSVisit::BSVisitControl ForEachFunction(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<IFunction>&)>& a_func) const = 0;
	};

	class IFormFunctionComponent : public IFunctionComponent
	{
	public:
		inline static constexpr auto FUNCTION_COMPONENT_TYPE = FunctionComponentType::kForm;
		inline static constexpr auto DEFAULT_DESCRIPTION = "A reference to a form."sv;

		IFormFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IFunctionComponent(a_parentFunction, a_name, a_description) {}

		[[nodiscard]] FunctionComponentType GetType() const override { return FUNCTION_COMPONENT_TYPE; }
		[[nodiscard]] RE::BSString GetDefaultDescription() const override { return DEFAULT_DESCRIPTION; }

		[[nodiscard]] virtual RE::TESForm* GetTESFormValue() const = 0;
		virtual void SetTESFormValue(RE::TESForm* a_form) = 0;
	};

	class INumericFunctionComponent : public IFunctionComponent
	{
	public:
		inline static constexpr auto FUNCTION_COMPONENT_TYPE = FunctionComponentType::kNumeric;
		inline static constexpr auto DEFAULT_DESCRIPTION = "A numeric value - a static value, a reference to a global variable, an Actor Value, or a behavior graph variable."sv;

		INumericFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IFunctionComponent(a_parentFunction, a_name, a_description) {}

		[[nodiscard]] FunctionComponentType GetType() const override { return FUNCTION_COMPONENT_TYPE; }
		[[nodiscard]] RE::BSString GetDefaultDescription() const override { return DEFAULT_DESCRIPTION; }

		[[nodiscard]] virtual float GetNumericValue(RE::TESObjectREFR* a_refr) const = 0;
		virtual void SetStaticValue(float a_value) = 0;
		virtual void SetGlobalVariable(RE::TESGlobal* a_global) = 0;
		virtual void SetActorValue(RE::ActorValue a_actorValue, Components::ActorValueType a_valueType) = 0;
		virtual void SetGraphVariable(const char* a_graphVariableName, Components::GraphVariableType a_valueType) = 0;
		[[nodiscard]] virtual Components::ActorValueType GetActorValueType() const = 0;
		[[nodiscard]] virtual RE::ActorValue GetActorValue() const = 0;
		virtual Components::GraphVariableType GetGraphVariableType() const = 0;
		[[nodiscard]] virtual RE::BSString GetGraphVariableName() const = 0;
	};

	class INiPoint3FunctionComponent : public IFunctionComponent
	{
	public:
		inline static constexpr auto FUNCTION_COMPONENT_TYPE = FunctionComponentType::kNiPoint3;
		inline static constexpr auto DEFAULT_DESCRIPTION = "A user defined static NiPoint3 value (x, y, z vector)."sv;

		INiPoint3FunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IFunctionComponent(a_parentFunction, a_name, a_description) {}

		[[nodiscard]] FunctionComponentType GetType() const override { return FUNCTION_COMPONENT_TYPE; }
		[[nodiscard]] RE::BSString GetDefaultDescription() const override { return DEFAULT_DESCRIPTION; }

		[[nodiscard]] virtual const RE::NiPoint3& GetNiPoint3Value() const = 0;
		virtual void SetNiPoint3Value(const RE::NiPoint3& a_point) = 0;
	};

	class IKeywordFunctionComponent : public IFunctionComponent
	{
	public:
		inline static constexpr auto FUNCTION_COMPONENT_TYPE = FunctionComponentType::kKeyword;
		inline static constexpr auto DEFAULT_DESCRIPTION = "A keyword value - a reference to a BGSKeyword form, or a literal EditorID of the keyword."sv;

		IKeywordFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IFunctionComponent(a_parentFunction, a_name, a_description) {}

		[[nodiscard]] FunctionComponentType GetType() const override { return FUNCTION_COMPONENT_TYPE; }
		[[nodiscard]] RE::BSString GetDefaultDescription() const override { return DEFAULT_DESCRIPTION; }

		[[nodiscard]] virtual bool HasKeyword(const RE::BGSKeywordForm* a_form) const = 0;
		virtual void SetKeyword(RE::BGSKeyword* a_keyword) = 0;
		virtual void SetLiteral(const char* a_literal) = 0;
	};

	class ITextFunctionComponent : public IFunctionComponent
	{
	public:
		inline static constexpr auto FUNCTION_COMPONENT_TYPE = FunctionComponentType::kText;
		inline static constexpr auto DEFAULT_DESCRIPTION = "A text value."sv;

		ITextFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IFunctionComponent(a_parentFunction, a_name, a_description) {}

		[[nodiscard]] FunctionComponentType GetType() const override { return FUNCTION_COMPONENT_TYPE; }
		[[nodiscard]] RE::BSString GetDefaultDescription() const override { return DEFAULT_DESCRIPTION; }

		[[nodiscard]] virtual RE::BSString GetTextValue() const = 0;
		virtual void SetTextValue(const char* a_text) = 0;
		virtual void SetAllowSpaces(bool a_bAllowSpaces) = 0;
	};

	class IBoolFunctionComponent : public IFunctionComponent
	{
	public:
		inline static constexpr auto FUNCTION_COMPONENT_TYPE = FunctionComponentType::kBool;
		inline static constexpr auto DEFAULT_DESCRIPTION = "A boolean value."sv;

		IBoolFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IFunctionComponent(a_parentFunction, a_name, a_description) {}

		[[nodiscard]] FunctionComponentType GetType() const override { return FUNCTION_COMPONENT_TYPE; }
		[[nodiscard]] RE::BSString GetDefaultDescription() const override { return DEFAULT_DESCRIPTION; }

		[[nodiscard]] virtual bool GetBoolValue() const = 0;
		virtual void SetBoolValue(bool a_value) = 0;
	};

	class IConditionFunctionComponent : public IFunctionComponent
	{
	public:
		inline static constexpr auto FUNCTION_COMPONENT_TYPE = FunctionComponentType::kCondition;
		inline static constexpr auto DEFAULT_DESCRIPTION = "A set of functions that will only run if the given condition evaluates to true."sv;

		IConditionFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IFunctionComponent(a_parentFunction, a_name, a_description) {}

		[[nodiscard]] FunctionComponentType GetType() const override { return FUNCTION_COMPONENT_TYPE; }
		[[nodiscard]] RE::BSString GetDefaultDescription() const override { return DEFAULT_DESCRIPTION; }

		[[nodiscard]] virtual Conditions::ConditionSet* GetConditions() const = 0;
		[[nodiscard]] virtual FunctionSet* GetFunctions() const = 0;
		virtual bool TryRun(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger = nullptr) const = 0;
		virtual RE::BSVisit::BSVisitControl ForEachCondition(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<Conditions::ICondition>&)>& a_func) const = 0;
		virtual RE::BSVisit::BSVisitControl ForEachFunction(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<IFunction>&)>& a_func) const = 0;
	};

	class ICustomFunctionComponent : public IFunctionComponent
	{
	public:
		inline static constexpr auto FUNCTION_COMPONENT_TYPE = FunctionComponentType::kCustom;

		ICustomFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			IFunctionComponent(a_parentFunction, a_name, a_description) {}

		[[nodiscard]] FunctionComponentType GetType() const override { return FUNCTION_COMPONENT_TYPE; }
	};

	// use this class as your parent class for a custom OAR function
	// this one has a wrapped object that will be internally used for serialization and internal things like that, which don't concern you
	// you just need to implement the few other member functions
	// this approach should save a lot of headaches and should be safe across DLL boundaries with different compilers
	class CustomFunction : public IFunction
	{
	public:
		CustomFunction();

		bool Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger = nullptr) const override;

		void Initialize(void* a_value) override { return _wrappedFunction->Initialize(a_value); }
		void Serialize(void* a_value, void* a_allocator, [[maybe_unused]] IFunction* a_outerCustomFunction) override { _wrappedFunction->Serialize(a_value, a_allocator, this); }

		void PreInitialize() override { _wrappedFunction->PostInitialize(); }
		void PostInitialize() override { _wrappedFunction->PostInitialize(); }

		[[nodiscard]] RE::BSString GetArgument() const override { return _wrappedFunction->GetArgument(); }

		[[nodiscard]] RE::BSString GetRequiredPluginName() const override { return SKSE::PluginDeclaration::GetSingleton()->GetName().data(); }
		[[nodiscard]] RE::BSString GetRequiredPluginAuthor() const override { return SKSE::PluginDeclaration::GetSingleton()->GetAuthor().data(); }

		[[nodiscard]] bool IsDisabled() const override { return _wrappedFunction->IsDisabled(); }
		void SetDisabled(bool a_bDisabled) override { _wrappedFunction->SetDisabled(a_bDisabled); }
		[[nodiscard]] EssentialState GetEssential() const override { return _wrappedFunction->GetEssential(); }
		void SetEssential(EssentialState a_essentialState) override { _wrappedFunction->SetEssential(a_essentialState); }

		[[nodiscard]] uint32_t GetNumComponents() const override { return _wrappedFunction->GetNumComponents(); }
		[[nodiscard]] IFunctionComponent* GetComponent(uint32_t a_index) const override { return _wrappedFunction->GetComponent(a_index); }
		IFunctionComponent* AddComponent(FunctionComponentFactory a_factory, const char* a_name, const char* a_description = "") override { return _wrappedFunction->AddComponent(a_factory, a_name, a_description); }

		[[nodiscard]] FunctionAPIVersion GetFunctionAPIVersion() const override { return FunctionAPIVersion::Latest; }
		[[nodiscard]] FunctionType GetFunctionType() const override { return FunctionType::kCustom; }
		[[nodiscard]] IFunction* GetWrappedFunction() const override { return _wrappedFunction.get(); }

		[[nodiscard]] bool HasTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload) const override { return _wrappedFunction->HasTrigger(a_trigger, a_payload); }
		void AddTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload) override { _wrappedFunction->AddTrigger(a_trigger, a_payload); }
		void RemoveTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload) override { _wrappedFunction->RemoveTrigger(a_trigger, a_payload); }
		RE::BSVisit::BSVisitControl ForEachTrigger(const std::function<RE::BSVisit::BSVisitControl(const Trigger&)>& a_func) const override { return _wrappedFunction->ForEachTrigger(a_func); }

		IFunctionComponent* AddBaseComponent(FunctionComponentType a_componentType, const char* a_name, const char* a_description = "");

		template <typename T>
		static IFunction* ConstructFunction()
		{
			return new T();
		}

		template <typename T>
		static FunctionFactory GetFactory()
		{
			return &ConstructFunction<T>;
		}

	protected:
		std::unique_ptr<IFunction> _wrappedFunction = nullptr;
	};
}
