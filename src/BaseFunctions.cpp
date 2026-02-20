#include "BaseFunctions.h"

#include "BaseConditions.h"
#include "Conditions.h"
#include "DetectedProblems.h"
#include "Functions.h"

namespace Functions
{
	void FunctionBase::Initialize(void* a_value)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		const auto object = value.GetObj();

		// disabled
		if (const auto disabledIt = object.FindMember("disabled"); disabledIt != object.MemberEnd() && disabledIt->value.IsBool()) {
			_bDisabled = disabledIt->value.GetBool();
		}

		// essential
		if (const auto essentialIt = object.FindMember("essential"); essentialIt != object.MemberEnd() && essentialIt->value.IsInt()) {
			_essentialState = static_cast<EssentialState>(essentialIt->value.GetInt());
		}

		for (const auto& component : _components) {
			component->InitializeComponent(a_value);
		}

		// triggers
		if (const auto triggersIt = object.FindMember("triggers"); triggersIt != object.MemberEnd() && triggersIt->value.IsArray()) {
			for (const auto& triggerObject : triggersIt->value.GetArray()) {
				if (triggerObject.IsObject()) {
					Trigger trigger;
					if (const auto eventIt = triggerObject.FindMember("event"); eventIt != triggerObject.MemberEnd() && eventIt->value.IsString()) {
						trigger.event = eventIt->value.GetString();
					}
					if (const auto payloadIt = triggerObject.FindMember("payload"); payloadIt != triggerObject.MemberEnd() && payloadIt->value.IsString()) {
						trigger.payload = payloadIt->value.GetString();
					}
					_triggers.emplace(trigger);
				}
			}
		}
	}

	void FunctionBase::Serialize(void* a_value, void* a_allocator, IFunction* a_outerCustomFunction)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		if (!a_outerCustomFunction) {
			a_outerCustomFunction = this;
		}

		// function name
		const auto functionName = a_outerCustomFunction->GetName();
		rapidjson::Value functionValue(functionName.data(), (functionName.length()), allocator);
		value.AddMember("function", functionValue, allocator);

		// required plugin
		auto requiredPlugin = std::string(a_outerCustomFunction->GetRequiredPluginName());
		if (!requiredPlugin.empty()) {
			rapidjson::Value requiredPluginValue(requiredPlugin.data(), static_cast<rapidjson::SizeType>(requiredPlugin.length()), allocator);
			value.AddMember("requiredPlugin", requiredPluginValue, allocator);
		}

		// required version
		REL::Version version = a_outerCustomFunction->GetRequiredVersion();
		auto versionString = version.string("."sv);
		rapidjson::Value versionValue(versionString.data(), static_cast<rapidjson::SizeType>(versionString.length()), allocator);
		value.AddMember("requiredVersion", versionValue, allocator);

		// disabled
		if (_bDisabled) {
			value.AddMember("disabled", _bDisabled, allocator);
		}

		// essential
		if (_essentialState != EssentialState::kEssential) {
			value.AddMember("essential", static_cast<uint8_t>(_essentialState), allocator);
		}

		// components
		for (auto& component : _components) {
			component->SerializeComponent(&value, a_allocator);
		}

		// triggers
		if (!_triggers.empty()) {
			if (!GetParentSet() || GetParentSet()->GetFunctionSetType() == Functions::FunctionSetType::kOnTrigger || GetParentSet()->GetFunctionSetType() == Functions::FunctionSetType::kNone) {
				rapidjson::Value triggersValue = rapidjson::Value(rapidjson::kArrayType);

				for (auto& trigger : _triggers) {
					rapidjson::Value triggerValue = rapidjson::Value(rapidjson::kObjectType);

					rapidjson::Value eventValue(trigger.event.data(), static_cast<rapidjson::SizeType>(trigger.event.length()), allocator);
					triggerValue.AddMember("event", eventValue, allocator);

					if (trigger.payload.length() > 0) {
						rapidjson::Value payloadValue(trigger.payload.data(), static_cast<rapidjson::SizeType>(trigger.payload.length()), allocator);
						triggerValue.AddMember("payload", payloadValue, allocator);
					}

					triggersValue.PushBack(triggerValue, allocator);
				}

				value.AddMember("triggers", triggersValue, allocator);
			}
		}
	}

	void FunctionBase::PostInitialize()
	{
		for (const auto& component : _components) {
			component->PostInitialize();
		}
	}

	IFunctionComponent* FunctionBase::GetComponent(uint32_t a_index) const
	{
		if (a_index < _components.size()) {
			return _components[a_index].get();
		}

		return nullptr;
	}

	IFunctionComponent* FunctionBase::AddComponent(FunctionComponentFactory a_factory, const char* a_name, const char* a_description)
	{
		const auto& result = _components.emplace_back(std::unique_ptr<IFunctionComponent>(a_factory(this, a_name, a_description)));
		return result.get();
	}

	bool FunctionBase::HasTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload) const
	{
		ReadLocker locker(_triggersLock);

		return _triggers.contains({ a_trigger, a_payload });
	}

	void FunctionBase::AddTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload)
	{
		WriteLocker locker(_triggersLock);

		_triggers.emplace(Trigger(a_trigger, a_payload));
	}

	void FunctionBase::RemoveTrigger(const RE::BSString& a_trigger, const RE::BSString& a_payload)
	{
		WriteLocker locker(_triggersLock);

		_triggers.erase({ a_trigger, a_payload });
	}

	RE::BSVisit::BSVisitControl FunctionBase::ForEachTrigger(const std::function<RE::BSVisit::BSVisitControl(const Trigger&)>& a_func) const
	{
		using Result = RE::BSVisit::BSVisitControl;

		ReadLocker locker(_triggersLock);

		auto result = Result::kContinue;

		for (auto& trigger : _triggers) {
			result = a_func(trigger);
			if (result == Result::kStop) {
				return result;
			}
		}

		return result;
	}

	bool FunctionSet::Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, SubMod* a_parentSubMod, Trigger* a_trigger) const
	{
		ReadLocker locker(_lock);

		bool bRanFunction = false;

		for (auto& function : _entries) {
			if (GetFunctionSetType() == FunctionSetType::kOnTrigger) {
				if (a_trigger && function->HasTrigger(a_trigger->event, a_trigger->payload)) {
					if (function->Run(a_refr, a_clipGenerator, a_parentSubMod, a_trigger)) {
						bRanFunction = true;
					}
				}
			} else {
				if (function->Run(a_refr, a_clipGenerator, a_parentSubMod, a_trigger)) {
					bRanFunction = true;
				}
			}
		}

		return bRanFunction;
	}

	void FunctionSet::SetAsParentImpl(std::unique_ptr<IFunction>& a_function)
	{
		a_function->SetParentSet(this);
	}

	bool FunctionSet::IsChildOfImpl([[maybe_unused]] IFunction* a_function)
	{
		if (const auto numComponents = a_function->GetNumComponents(); numComponents > 0) {
			for (uint32_t i = 0; i < numComponents; i++) {
				const auto component = a_function->GetComponent(i);
				if (component->GetType() == FunctionComponentType::kCondition) {
					const auto conditionFunctionComponent = static_cast<IConditionFunctionComponent*>(component);
					if (!conditionFunctionComponent->GetFunctions()) {
						continue;
					}

					const auto& functionSet = conditionFunctionComponent->GetFunctions();
					if (functionSet == this) {
						return true;
					}

					for (const auto& childFunction : conditionFunctionComponent->GetFunctions()->_entries) {
						if (IsChildOf(childFunction.get())) {
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	SubMod* FunctionSet::GetParentSubModImpl() const
	{
		if (_parentSubMod) {
			return _parentSubMod;
		}

		if (_parentMultiFunctionComponent) {
			if (const auto parentFunction = _parentMultiFunctionComponent->GetParentFunction()) {
				if (const auto parentFunctionSet = parentFunction->GetParentSet()) {
					return parentFunctionSet->GetParentSubMod();
				}
			}
		}

		return nullptr;
	}

	std::string FunctionSet::NumTextImpl() const
	{
		if (Num() == 1) {
			return "1 function";
		}
		return std::format("{} functions", Num()).data();
	}

	bool FunctionSet::IsDirtyRecursiveImpl() const
	{
		ReadLocker locker(_lock);

		if (IsDirty()) {
			return true;
		}

		for (const auto& condition : _entries) {
			if (const auto numComponents = condition->GetNumComponents(); numComponents > 0) {
				for (uint32_t i = 0; i < numComponents; i++) {
					const auto component = condition->GetComponent(i);
					if (component->GetType() == FunctionComponentType::kCondition) {
						const auto conditionComponent = static_cast<IConditionFunctionComponent*>(component);
						if (const auto conditionSet = conditionComponent->GetConditions()) {
							if (conditionSet->IsDirtyRecursive()) {
								return true;
							}
						}
					}
				}
			}
		}

		return false;
	}

	void FunctionSet::SetDirtyRecursiveImpl(bool a_bDirty)
	{
		ReadLocker locker(_lock);

		for (const auto& condition : _entries) {
			if (const auto numComponents = condition->GetNumComponents(); numComponents > 0) {
				for (uint32_t i = 0; i < numComponents; i++) {
					const auto component = condition->GetComponent(i);
					if (component->GetType() == FunctionComponentType::kCondition) {
						const auto conditionComponent = static_cast<IConditionFunctionComponent*>(component);
						if (const auto conditionSet = conditionComponent->GetConditions()) {
							conditionSet->SetDirtyRecursiveImpl(a_bDirty);
						}
					}
				}
			}
		}

		SetDirty(a_bDirty);
	}

	const Functions::IFunction* FunctionSet::GetParentFunction() const
	{
		if (_parentMultiFunctionComponent) {
			return _parentMultiFunctionComponent->GetParentFunction();
		}

		return nullptr;
	}

	void MultiFunctionComponent::InitializeComponent(void* a_value)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		const auto object = value.GetObj();
		if (const auto functionsIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); functionsIt != object.MemberEnd() && functionsIt->value.IsArray()) {
			for (const auto functionsArray = functionsIt->value.GetArray(); auto& functionValue : functionsArray) {
				if (auto function = CreateFunctionFromJson(functionValue)) {
					functionSet->Add(function);
				}
			}
		}
	}

	void MultiFunctionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		rapidjson::Value functionArrayValue = functionSet->Serialize(allocator);

		value.AddMember(rapidjson::StringRef(_name.data(), _name.length()), functionArrayValue, allocator);
	}

	RE::BSString MultiFunctionComponent::GetArgument() const
	{
		return functionSet->NumText().data();
	}

	bool MultiFunctionComponent::Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, void* a_trigger) const
	{
		return functionSet->Run(a_refr, a_clipGenerator, static_cast<SubMod*>(a_parentSubMod), static_cast<Trigger*>(a_trigger));
	}

	RE::BSVisit::BSVisitControl MultiFunctionComponent::ForEachFunction(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<IFunction>&)>& a_func) const
	{
		return functionSet->ForEach(a_func);
	}

	void FormFunctionComponent::InitializeComponent(void* a_value)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);

		const auto object = value.GetObj();
		if (const auto formIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); formIt != object.MemberEnd() && formIt->value.IsObject()) {
			form.Parse(formIt->value);
		}
	}

	void FormFunctionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		value.AddMember(rapidjson::StringRef(_name.data(), _name.length()), form.Serialize(allocator), allocator);
	}

	void NumericFunctionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto valueIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); valueIt != object.MemberEnd() && valueIt->value.IsObject()) {
			value.Parse(valueIt->value);
		}
	}

	void NumericFunctionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), value.Serialize(allocator), allocator);
	}

	void NiPoint3FunctionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto valueIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); valueIt != object.MemberEnd() && valueIt->value.IsArray()) {
			value.Parse(valueIt->value);
		}
	}

	void NiPoint3FunctionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), value.Serialize(allocator), allocator);
	}

	void KeywordFunctionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto keywordIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); keywordIt != object.MemberEnd() && keywordIt->value.IsObject()) {
			keyword.Parse(keywordIt->value);
		}
	}

	void KeywordFunctionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), keyword.Serialize(allocator), allocator);
	}

	void TextFunctionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto textIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); textIt != object.MemberEnd() && textIt->value.IsString()) {
			text.Parse(textIt->value);
		}
	}

	void TextFunctionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), text.Serialize(allocator), allocator);
	}

	void BoolFunctionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto boolIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); boolIt != object.MemberEnd() && boolIt->value.IsBool()) {
			bValue = boolIt->value.GetBool();
		}
	}

	void BoolFunctionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), bValue, allocator);
	}

	bool BoolFunctionComponent::DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
	{
		bool bEdited = false;

		ImGui::PushID(&bValue);
		ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
		ImGui::BeginDisabled(!a_bEditable);
		if (ImGui::Checkbox("##value", &bValue)) {
			bEdited = true;
		}
		ImGui::PopID();
		ImGui::EndDisabled();

		return bEdited;
	}

	void ConditionFunctionComponent::InitializeComponent(void* a_value)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		const auto object = value.GetObj();
		if (const auto componentIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); componentIt != object.MemberEnd() && componentIt->value.IsObject()) {
			const auto componentObject = componentIt->value.GetObj();
			if (const auto conditionsIt = componentObject.FindMember("conditions"); conditionsIt != componentObject.MemberEnd() && conditionsIt->value.IsArray()) {
				for (const auto conditionsArray = conditionsIt->value.GetArray(); auto& conditionValue : conditionsArray) {
					if (auto condition = Conditions::CreateConditionFromJson(conditionValue)) {
						conditionSet->Add(condition);
					}
				}
			}
			if (const auto functionsIt = componentObject.FindMember("functions"); functionsIt != componentObject.MemberEnd() && functionsIt->value.IsArray()) {
				for (const auto functionsArray = functionsIt->value.GetArray(); auto& functionValue : functionsArray) {
					if (auto function = Functions::CreateFunctionFromJson(functionValue)) {
						functionSet->Add(function);
					}
				}
			}
		}
	}

	void ConditionFunctionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		auto componentObjectValue = rapidjson::Value(rapidjson::kObjectType);

		rapidjson::Value conditionArrayValue = conditionSet->Serialize(allocator);
		rapidjson::Value functionArrayValue = functionSet->Serialize(allocator);

		componentObjectValue.AddMember("conditions", conditionArrayValue, allocator);
		componentObjectValue.AddMember("functions", functionArrayValue, allocator);

		value.AddMember(rapidjson::StringRef(_name.data(), _name.length()), componentObjectValue, allocator);
	}

	RE::BSString ConditionFunctionComponent::GetArgument() const
	{
		return std::format("{} | {}", conditionSet->NumText().data(), functionSet->NumText().data()).data();
	}

	bool ConditionFunctionComponent::IsValid() const
	{
		return !conditionSet->IsEmpty() && conditionSet->IsValid() && !functionSet->IsEmpty() && functionSet->IsValid();
	}

	bool ConditionFunctionComponent::TryRun(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger /*= nullptr*/) const
	{
		if (conditionSet->EvaluateAll(a_refr, a_clipGenerator, static_cast<SubMod*>(a_parentSubMod))) {
			return functionSet->Run(a_refr, a_clipGenerator, static_cast<SubMod*>(a_parentSubMod), a_trigger);
		}

		return false;
	}

	RE::BSVisit::BSVisitControl ConditionFunctionComponent::ForEachCondition(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<Conditions::ICondition>&)>& a_func) const
	{
		return conditionSet->ForEach(a_func);
	}

	RE::BSVisit::BSVisitControl ConditionFunctionComponent::ForEachFunction(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<IFunction>&)>& a_func) const
	{
		return functionSet->ForEach(a_func);
	}
}
