#include "BaseConditions.h"
#include "OpenAnimationReplacer.h"
#include "UI/UICommon.h"
#include "Utils.h"

#include <imgui_stdlib.h>
#undef GetObject  // fix windows sdk definition issue

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

	constexpr uint32_t operator"" _h(const char* a_str, const size_t a_size) noexcept
	{
		return Hash(a_str, a_size);
	}

	void ConditionBase::Initialize(void* a_value)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		const auto object = value.GetObj();

		// disabled
		if (const auto disabledIt = object.FindMember("disabled"); disabledIt != object.MemberEnd() && disabledIt->value.IsBool()) {
			_bDisabled = disabledIt->value.GetBool();
		}

		// negated
		if (const auto negatedIt = object.FindMember("negated"); negatedIt != object.MemberEnd() && negatedIt->value.IsBool()) {
			_bNegated = negatedIt->value.GetBool();
		}

		// essential
		if (const auto essentialIt = object.FindMember("essential"); essentialIt != object.MemberEnd() && essentialIt->value.IsInt()) {
			_essentialState = static_cast<EssentialState>(essentialIt->value.GetInt());
		}

		for (const auto& component : _components) {
			component->InitializeComponent(a_value);
		}
	}

	void ConditionBase::Serialize(void* a_value, void* a_allocator, ICondition* a_outerCustomCondition)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		if (!a_outerCustomCondition) {
			a_outerCustomCondition = this;
		}

		// condition name
		const auto conditionName = a_outerCustomCondition->GetName();
		rapidjson::Value conditionValue(conditionName.data(), (conditionName.length()), allocator);
		value.AddMember("condition", conditionValue, allocator);

		// required plugin
		auto requiredPlugin = std::string(a_outerCustomCondition->GetRequiredPluginName());
		if (!requiredPlugin.empty()) {
			rapidjson::Value requiredPluginValue(requiredPlugin.data(), static_cast<rapidjson::SizeType>(requiredPlugin.length()), allocator);
			value.AddMember("requiredPlugin", requiredPluginValue, allocator);
		}

		// required version
		REL::Version version = a_outerCustomCondition->GetRequiredVersion();
		auto versionString = version.string("."sv);
		rapidjson::Value versionValue(versionString.data(), static_cast<rapidjson::SizeType>(versionString.length()), allocator);
		value.AddMember("requiredVersion", versionValue, allocator);

		// disabled
		if (_bDisabled) {
			value.AddMember("disabled", _bDisabled, allocator);
		}

		// negated
		if (_bNegated) {
			value.AddMember("negated", _bNegated, allocator);
		}

		// essential
		if (_essentialState != EssentialState::kEssential) {
			value.AddMember("essential", static_cast<uint8_t>(_essentialState), allocator);
		}

		// components
		for (auto& component : _components) {
			component->SerializeComponent(&value, a_allocator);
		}
	}

	void ConditionBase::PostInitialize()
	{
		for (const auto& component : _components) {
			component->PostInitialize();
		}
	}

	IConditionComponent* ConditionBase::GetComponent(uint32_t a_index) const
	{
		if (a_index < _components.size()) {
			return _components[a_index].get();
		}

		return nullptr;
	}

	IConditionComponent* ConditionBase::AddComponent(ConditionComponentFactory a_factory, const char* a_name, const char* a_description /* = ""*/)
	{
		const auto& result = _components.emplace_back(std::unique_ptr<IConditionComponent>(a_factory(this, a_name, a_description)));
		return result.get();
	}

	bool ConditionSet::EvaluateAll(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, SubMod* a_parentSubMod, bool a_bForceTrace) const
	{
		ReadLocker locker(_lock);

		auto& animationLog = AnimationLog::GetSingleton();
		ReplacementTrace* trace = a_bForceTrace || animationLog.ShouldLogAnimationsForRefr(a_refr) ? OpenAnimationReplacer::GetSingleton().GetTrace(a_refr, a_clipGenerator) : nullptr;

		if (trace) {
			// if we're tracing, we need to do it in a classic for loop to know where it failed
			for (const auto& condition : _entries) {
				bool bHasMultiComponent = Utils::ConditionHasMultiComponent(condition.get());
				if (bHasMultiComponent) {
					trace->StartTracingMultiCondition();
				}
				bool bSuccess = condition->Evaluate(a_refr, a_clipGenerator, a_parentSubMod);
				ReplacementTrace::Step::StepResult result;
				if (condition->IsDisabled()) {
					result = ReplacementTrace::Step::StepResult::kDisabled;
				} else {
					result = bSuccess ? ReplacementTrace::Step::StepResult::kSuccess : ReplacementTrace::Step::StepResult::kFail;
				}
				if (bHasMultiComponent) {
					trace->EndTracingMultiCondition(condition.get(), result);
				} else {
					trace->TraceCondition(condition.get(), result);
				}
				
				if (!bSuccess) {
					return false;
				}
			}
			return true;
		} else {
			return std::ranges::all_of(_entries, [&](auto& a_condition) { return a_condition->Evaluate(a_refr, a_clipGenerator, a_parentSubMod); });
		}
	}

	bool ConditionSet::EvaluateAny(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, SubMod* a_parentSubMod, bool a_bForceTrace) const
	{
		ReadLocker locker(_lock);

		//return std::ranges::any_of(_conditions, [&](auto& a_condition) { return a_condition->Evaluate(a_refr, a_clipGenerator); });

		auto& animationLog = AnimationLog::GetSingleton();
		ReplacementTrace* trace = a_bForceTrace || animationLog.ShouldLogAnimationsForRefr(a_refr) ? OpenAnimationReplacer::GetSingleton().GetTrace(a_refr, a_clipGenerator) : nullptr;

		// skip disabled conditions and also return true when all conditions are disabled
		bool bAnyMet = false;
		bool bAllDisabled = true;
		for (const auto& condition : _entries) {
			if (!condition->IsDisabled()) {
				bAllDisabled = false;
				bool bHasMultiComponent = false;
				if (trace) {
					bHasMultiComponent = Utils::ConditionHasMultiComponent(condition.get());
				}
				if (trace && bHasMultiComponent) {
					trace->StartTracingMultiCondition();
				}
				bool bSuccess = condition->Evaluate(a_refr, a_clipGenerator, a_parentSubMod);
				if (trace) {
					ReplacementTrace::Step::StepResult result = bSuccess ? ReplacementTrace::Step::StepResult::kSuccess : ReplacementTrace::Step::StepResult::kFail;
					if (bHasMultiComponent) {
						trace->EndTracingMultiCondition(condition.get(), result);
					} else {
						trace->TraceCondition(condition.get(), result);
					}
					
				}
				if (bSuccess) {
					bAnyMet = true;
					break;
				}
			} else if (trace) {
				trace->TraceCondition(condition.get(), ReplacementTrace::Step::StepResult::kDisabled);
			}
		}

		return bAnyMet || bAllDisabled;
	}

	void ConditionSet::SetAsParentImpl(std::unique_ptr<ICondition>& a_condition)
	{
		a_condition->SetParentConditionSet(this);

		if (a_condition->GetConditionType() == ConditionType::kPreset) {
			if (const auto numComponents = a_condition->GetNumComponents(); numComponents > 0) {
				for (uint32_t i = 0; i < numComponents; i++) {
					const auto component = a_condition->GetComponent(i);
					if (component->GetType() == ConditionComponentType::kPreset) {
						const auto conditionPresetComponent = static_cast<ConditionPresetComponent*>(component);
						conditionPresetComponent->TryFindPreset();
					}
				}
			}
		}
	}

	bool ConditionSet::IsChildOfImpl(ICondition* a_condition)
	{
		if (const auto numComponents = a_condition->GetNumComponents(); numComponents > 0) {
			for (uint32_t i = 0; i < numComponents; i++) {
				const auto component = a_condition->GetComponent(i);
				if (component->GetType() == ConditionComponentType::kMulti) {
					const auto multiConditionComponent = static_cast<IMultiConditionComponent*>(component);
					if (!multiConditionComponent->GetConditions()) {
						continue;
					}

					const auto& conditionSet = multiConditionComponent->GetConditions();
					if (conditionSet == this) {
						return true;
					}

					for (const auto& childCondition : multiConditionComponent->GetConditions()->_entries) {
						if (IsChildOf(childCondition.get())) {
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	SubMod* ConditionSet::GetParentSubModImpl() const
	{
		if (_parentSubMod) {
			return _parentSubMod;
		}

		if (_parentMultiConditionComponent) {
			if (const auto parentCondition = _parentMultiConditionComponent->GetParentCondition()) {
				if (const auto parentConditionSet = parentCondition->GetParentConditionSet()) {
					return parentConditionSet->GetParentSubMod();
				}
			}
		}

		return nullptr;
	}

	std::string ConditionSet::NumTextImpl() const
	{
		if (Num() == 1) {
			return "1 child condition";
		}
		return std::format("{} child conditions", Num()).data();
	}

	bool ConditionSet::IsDirtyRecursiveImpl() const
	{
		ReadLocker locker(_lock);

		if (IsDirty()) {
			return true;
		}

		for (const auto& condition : _entries) {
			if (const auto numComponents = condition->GetNumComponents(); numComponents > 0) {
				for (uint32_t i = 0; i < numComponents; i++) {
					const auto component = condition->GetComponent(i);
					if (component->GetType() == ConditionComponentType::kMulti) {
						const auto multiConditionComponent = static_cast<IMultiConditionComponent*>(component);
						if (const auto conditionSet = multiConditionComponent->GetConditions()) {
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

	void ConditionSet::SetDirtyRecursiveImpl(bool a_bDirty)
	{
		ReadLocker locker(_lock);

		for (const auto& condition : _entries) {
			if (const auto numComponents = condition->GetNumComponents(); numComponents > 0) {
				for (uint32_t i = 0; i < numComponents; i++) {
					const auto component = condition->GetComponent(i);
					if (component->GetType() == ConditionComponentType::kMulti) {
						const auto multiConditionComponent = static_cast<IMultiConditionComponent*>(component);
						if (const auto conditionSet = multiConditionComponent->GetConditions()) {
							conditionSet->SetDirtyRecursiveImpl(a_bDirty);
						}
					}
				}
			}
		}

		SetDirty(a_bDirty);
	}

	const ICondition* ConditionSet::GetParentCondition() const
	{
		if (_parentMultiConditionComponent) {
			return _parentMultiConditionComponent->GetParentCondition();
		}

		return nullptr;
	}

	void MultiConditionComponent::InitializeComponent(void* a_value)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		const auto object = value.GetObj();
		if (const auto conditionsIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); conditionsIt != object.MemberEnd() && conditionsIt->value.IsArray()) {
			for (const auto conditionsArray = conditionsIt->value.GetArray(); auto& conditionValue : conditionsArray) {
				if (auto condition = CreateConditionFromJson(conditionValue)) {
					conditionSet->Add(condition);
				}
			}
		}
	}

	void MultiConditionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		rapidjson::Value conditionArrayValue = conditionSet->Serialize(allocator);

		value.AddMember(rapidjson::StringRef(_name.data(), _name.length()), conditionArrayValue, allocator);
	}

	RE::BSString MultiConditionComponent::GetArgument() const
	{
		return conditionSet->NumText().data();
	}

	bool MultiConditionComponent::EvaluateAll(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const
	{
		return conditionSet->EvaluateAll(a_refr, a_clipGenerator, static_cast<SubMod*>(a_parentSubMod));
	}

	bool MultiConditionComponent::EvaluateAny(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const
	{
		return conditionSet->EvaluateAny(a_refr, a_clipGenerator, static_cast<SubMod*>(a_parentSubMod));
	}

	RE::BSVisit::BSVisitControl MultiConditionComponent::ForEachCondition(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<ICondition>&)>& a_func) const
	{
		return conditionSet->ForEach(a_func);
	}

	void FormConditionComponent::InitializeComponent(void* a_value)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);

		const auto object = value.GetObj();
		if (const auto formIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); formIt != object.MemberEnd() && formIt->value.IsObject()) {
			form.Parse(formIt->value);
		}
	}

	void FormConditionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		value.AddMember(rapidjson::StringRef(_name.data(), _name.length()), form.Serialize(allocator), allocator);
	}

	void NumericConditionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto valueIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); valueIt != object.MemberEnd() && valueIt->value.IsObject()) {
			value.Parse(valueIt->value);
		}
	}

	void NumericConditionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), value.Serialize(allocator), allocator);
	}

	void NiPoint3ConditionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto valueIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); valueIt != object.MemberEnd() && valueIt->value.IsArray()) {
			value.Parse(valueIt->value);
		}
	}

	void NiPoint3ConditionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), value.Serialize(allocator), allocator);
	}

	void KeywordConditionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto keywordIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); keywordIt != object.MemberEnd() && keywordIt->value.IsObject()) {
			keyword.Parse(keywordIt->value);
		}
	}

	void KeywordConditionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), keyword.Serialize(allocator), allocator);
	}

	void LocRefTypeConditionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto locRefTypeIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); locRefTypeIt != object.MemberEnd() && locRefTypeIt->value.IsObject()) {
			locRefType.Parse(locRefTypeIt->value);
		}
	}

	void LocRefTypeConditionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), locRefType.Serialize(allocator), allocator);
	}

	void TextConditionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto textIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); textIt != object.MemberEnd() && textIt->value.IsString()) {
			text.Parse(textIt->value);
		}
	}

	void TextConditionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), text.Serialize(allocator), allocator);
	}

	void BoolConditionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();
		if (const auto boolIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); boolIt != object.MemberEnd() && boolIt->value.IsBool()) {
			bValue = boolIt->value.GetBool();
		}
	}

	void BoolConditionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), bValue, allocator);
	}

	bool BoolConditionComponent::DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
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

	void ComparisonConditionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();

		if (const auto comparisonIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); comparisonIt != object.MemberEnd() && comparisonIt->value.IsString()) {
			const std::string comparisonString = comparisonIt->value.GetString();
			if (comparisonString == GetOperatorString(ComparisonOperator::kEqual)) {
				comparisonOperator = ComparisonOperator::kEqual;
			} else if (comparisonString == GetOperatorString(ComparisonOperator::kNotEqual)) {
				comparisonOperator = ComparisonOperator::kNotEqual;
			} else if (comparisonString == GetOperatorString(ComparisonOperator::kGreater)) {
				comparisonOperator = ComparisonOperator::kGreater;
			} else if (comparisonString == GetOperatorString(ComparisonOperator::kGreaterEqual)) {
				comparisonOperator = ComparisonOperator::kGreaterEqual;
			} else if (comparisonString == GetOperatorString(ComparisonOperator::kLess)) {
				comparisonOperator = ComparisonOperator::kLess;
			} else if (comparisonString == GetOperatorString(ComparisonOperator::kLessEqual)) {
				comparisonOperator = ComparisonOperator::kLessEqual;
			}
		}
	}

	void ComparisonConditionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), rapidjson::StringRef(GetOperatorString(comparisonOperator).data(), GetOperatorString(comparisonOperator).length()), allocator);
	}

	bool ComparisonConditionComponent::DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
	{
		bool bEdited = false;

		if (a_bEditable) {
			ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
			ImGui::PushID(&comparisonOperator);
			const auto currentEnumName = GetOperatorString(comparisonOperator);

			if (ImGui::BeginCombo("##Enum value", currentEnumName.data())) {
				for (auto i = ComparisonOperator::kEqual; i < ComparisonOperator::kInvalid; i = static_cast<ComparisonOperator>(static_cast<std::underlying_type_t<ComparisonOperator>>(i) + 1)) {
					auto enumName = GetOperatorString(i);
					const bool bSelected = currentEnumName == enumName;
					if (ImGui::Selectable(enumName.data(), bSelected)) {
						if (!bSelected) {
							comparisonOperator = i;
							bEdited = true;
						}
					}
					if (bSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();

			UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
			ImGui::TextUnformatted(GetOperatorFullName(comparisonOperator).data());
		} else {
			ImGui::TextUnformatted(GetOperatorString(comparisonOperator).data());

			UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
			ImGui::TextUnformatted(GetOperatorFullName(comparisonOperator).data());
		}

		return bEdited;
	}

	bool ComparisonConditionComponent::GetComparisonResult(float a_valueA, float a_valueB) const
	{
		switch (comparisonOperator) {
		case ComparisonOperator::kEqual:
			return a_valueA == a_valueB;
		case ComparisonOperator::kNotEqual:
			return a_valueA != a_valueB;
		case ComparisonOperator::kGreater:
			return a_valueA > a_valueB;
		case ComparisonOperator::kGreaterEqual:
			return a_valueA >= a_valueB;
		case ComparisonOperator::kLess:
			return a_valueA < a_valueB;
		case ComparisonOperator::kLessEqual:
			return a_valueA <= a_valueB;
		}

		return false;
	}

	void ConditionStateComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();

		if (const auto componentIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); componentIt != object.MemberEnd() && componentIt->value.IsObject()) {
			const auto componentObj = componentIt->value.GetObj();

			if (CanSelectScope()) {
				if (const auto scopeIt = componentObj.FindMember("scope"); scopeIt != componentObj.MemberEnd() && scopeIt->value.IsString()) {
					scope = GetStateDataScopeFromName(scopeIt->value.GetString());
				}
			}

			if (CanResetOnLoopOrEcho()) {
				if (const auto shouldResetIt = componentObj.FindMember("shouldResetOnLoopOrEcho"); shouldResetIt != componentObj.MemberEnd() && shouldResetIt->value.IsBool()) {
					bShouldResetOnLoopOrEcho = shouldResetIt->value.GetBool();
				}
			}
		}
	}

	void ConditionStateComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		rapidjson::Value object(rapidjson::kObjectType);

		if (CanSelectScope()) {
			object.AddMember("scope", rapidjson::StringRef(GetStateDataScopeName(scope).data(), GetStateDataScopeName(scope).length()), allocator);
		}
		if (CanResetOnLoopOrEcho()) {
			object.AddMember("shouldResetOnLoopOrEcho", bShouldResetOnLoopOrEcho, allocator);
		}

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), object, allocator);
	}

	bool ConditionStateComponent::DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
	{
		bool bEdited = false;

		if (CanSelectScope() && a_bEditable) {
			ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
			ImGui::PushID(&scope);
			const auto currentEnumName = GetStateDataScopeName(scope);

			if (ImGui::BeginCombo("State data scope##Enum value", currentEnumName.data())) {
				for (StateDataScope i = StateDataScope::kLocal; i <= StateDataScope::kReference; i = static_cast<StateDataScope>(static_cast<int32_t>(i) << 1)) {
					if ((GetAllowedDataScopes() & i) != StateDataScope::kNone) {
						const bool bIsCurrent = i == scope;
						if (ImGui::Selectable(GetStateDataScopeName(i).data(), bIsCurrent)) {
							if (!bIsCurrent) {
								scope = i;
								bEdited = true;
							}
						}
						if (bIsCurrent) {
							ImGui::SetItemDefaultFocus();
						}
						UI::UICommon::AddTooltip(GetStateDataScopeTooltip(i).data());
					}
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();
			UI::UICommon::AddTooltip(GetStateDataScopeTooltip(scope).data());
		} else {
			const auto scopeText = std::format("State data scope: {}", GetStateDataScopeName(scope));
			ImGui::TextUnformatted(scopeText.data());
			UI::UICommon::AddTooltip(GetStateDataScopeTooltip(scope).data());
		}

		if (CanResetOnLoopOrEcho() && scope < StateDataScope::kReplacerMod) {
			ImGui::PushID(&bShouldResetOnLoopOrEcho);
			ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
			ImGui::BeginDisabled(!a_bEditable);
			if (ImGui::Checkbox("Reset on loop/echo##bShouldResetOnLoopOrEcho", &bShouldResetOnLoopOrEcho)) {
				bEdited = true;
			}
			UI::UICommon::AddTooltip("Enable if you want the data to be reset on animation clip loop or echo.");
			ImGui::PopID();
			ImGui::EndDisabled();
		}

		return bEdited;
	}

	RE::BSString ConditionStateComponent::GetArgument() const
	{
		std::string retString = std::string(GetStateDataScopeName(scope));
		retString.append(" scope");

		if (bShouldResetOnLoopOrEcho) {
			retString.append(" | Reset on loop/echo"sv);
		}

		return retString.data();
	}

	IStateData* ConditionStateComponent::CreateStateData(ConditionStateDataFactory a_stateDataFactory)
	{
		return a_stateDataFactory();
	}

	IStateData* ConditionStateComponent::GetStateData(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const
	{
		return OpenAnimationReplacer::GetSingleton().GetConditionStateData(this, a_refr, a_clipGenerator, a_parentSubMod);
	}

	IStateData* ConditionStateComponent::AddStateData(IStateData* a_stateData, RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod)
	{
		return OpenAnimationReplacer::GetSingleton().AddConditionStateData(a_stateData, this, a_refr, a_clipGenerator, a_parentSubMod);
	}

	bool ConditionStateComponent::CanSelectScope() const
	{
		// check if there's more than one flag set
		const int flags = static_cast<int>(GetAllowedDataScopes());
		return flags && ((flags & (flags - 1)) != 0);  // Check if val is not a power of 2
	}

	void ConditionPresetComponent::InitializeComponent(void* a_value)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		const auto object = value.GetObj();
		if (const auto presetNameIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); presetNameIt != object.MemberEnd() && presetNameIt->value.IsString()) {
			_presetName = presetNameIt->value.GetString();
		}
	}

	void ConditionPresetComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		const auto presetName = conditionPreset ? conditionPreset->GetName() : std::string{};

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), rapidjson::StringRef(presetName.data(), presetName.length()), allocator);
	}

	bool ConditionPresetComponent::DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
	{
		bool bEdited = false;

		if (a_bEditable) {
			if (const auto parentMod = GetParentMod()) {
				ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
				ImGui::PushID(&conditionPreset);
				const auto currentEnumName = conditionPreset ? conditionPreset->GetName() : ""sv;

				if (ImGui::BeginCombo("##Enum value", currentEnumName.data())) {
					parentMod->ForEachConditionPreset([&](const auto a_conditionPreset) {
						const auto enumName = a_conditionPreset->GetName();
						const bool bSelected = currentEnumName == enumName;
						if (ImGui::Selectable(enumName.data(), bSelected)) {
							if (!bSelected) {
								conditionPreset = a_conditionPreset;
								_presetName = conditionPreset->GetName();
								bEdited = true;
							}
						}
						if (bSelected) {
							ImGui::SetItemDefaultFocus();
						}

						return RE::BSVisit::BSVisitControl::kContinue;
					});
					ImGui::EndCombo();
				}
				ImGui::PopID();

				if (conditionPreset) {
					UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
					UI::UICommon::TextUnformattedEllipsis(conditionPreset->GetDescription().data());
				}
			}
		}

		return bEdited;
	}

	RE::BSString ConditionPresetComponent::GetArgument() const
	{
		if (conditionPreset) {
			return conditionPreset->GetName();
		}

		return ""sv;
	}

	bool ConditionPresetComponent::EvaluateAll(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const
	{
		if (conditionPreset) {
			return conditionPreset->EvaluateAll(a_refr, a_clipGenerator, static_cast<SubMod*>(a_parentSubMod));
		}

		return false;
	}

	bool ConditionPresetComponent::EvaluateAny(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod) const
	{
		if (conditionPreset) {
			return conditionPreset->EvaluateAny(a_refr, a_clipGenerator, static_cast<SubMod*>(a_parentSubMod));
		}

		return false;
	}

	RE::BSVisit::BSVisitControl ConditionPresetComponent::ForEachCondition(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<ICondition>&)>& a_func) const
	{
		if (conditionPreset) {
			return conditionPreset->ForEach(a_func);
		}

		return RE::BSVisit::BSVisitControl::kContinue;
	}

	void ConditionPresetComponent::TryFindPreset()
	{
		if (const auto parentMod = GetParentMod()) {
			conditionPreset = parentMod->GetConditionPreset(_presetName);
		}
	}

	ReplacerMod* ConditionPresetComponent::GetParentMod() const
	{
		if (const auto parentCondition = GetParentCondition()) {
			if (const auto parentConditionSet = parentCondition->GetParentConditionSet()) {
				if (const auto parentSubMod = parentConditionSet->GetParentSubMod()) {
					return parentSubMod->GetParentMod();
				}
			}
		}

		return nullptr;
	}
}
