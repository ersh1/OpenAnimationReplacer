#include "BaseConditions.h"
#include "OpenAnimationReplacer.h"
#include "UI/UICommon.h"
#include "Utils.h"

#include <imgui_stdlib.h>

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

	float NumericValue::GetValue(RE::TESObjectREFR* a_refr) const
	{
		switch (_type) {
		case Type::kStaticValue:
			return _floatValue;
		case Type::kGlobalVariable:
			return _globalVariable.IsValid() ? _globalVariable.GetValue()->value : 0.f;
		case Type::kActorValue:
			{
				float outValue = 0.f;
				if (a_refr) {
					switch (_actorValueType) {
					case ActorValueType::kActorValue:
						GetActorValue(a_refr, outValue);
						break;
					case ActorValueType::kBase:
						GetActorValueBase(a_refr, outValue);
						break;
					case ActorValueType::kMax:
						GetActorValueMax(a_refr, outValue);
						break;
					case ActorValueType::kPercentage:
						GetActorValuePercentage(a_refr, outValue);
						break;
					}
				}
				return outValue;
			}
		case Type::kGraphVariable:
			{
				if (a_refr) {
					switch (_graphVariableType) {
					case GraphVariableType::kFloat:
						{
							float outValue = 0.f;
							a_refr->GetGraphVariableFloat(_graphVariableName.GetValue(), outValue);
							return outValue;
						}
					case GraphVariableType::kInt:
						{
							int32_t outValue = 0;
							a_refr->GetGraphVariableInt(_graphVariableName.GetValue(), outValue);
							return static_cast<float>(outValue);
						}
					case GraphVariableType::kBool:
						{
							bool outValue;
							a_refr->GetGraphVariableBool(_graphVariableName.GetValue(), outValue);
							return outValue;
						}
					}
				}
			}
		}

		return 0.f;
	}

	bool NumericValue::DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
	{
		bool bEdited = false;

		if (a_bEditable) {
			if (!(getEnumMap && _type == Type::kStaticValue)) {
				ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
				ImGui::PushID(&_type);
				if (ImGui::SliderInt("", reinterpret_cast<int*>(&_type), 0, static_cast<int>(Type::kTotal) - 1, GetTypeName().data())) {
					bEdited = true;
				}
				ImGui::PopID();
			}

			switch (_type) {
			case Type::kStaticValue:
				{
					if (!getEnumMap) {
						ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
						ImGui::PushID(&_floatValue);
						if (ImGui::InputFloat("", &_floatValue, 0.01f, 1.0f, "%.3f")) {
							bEdited = true;
						}
						ImGui::PopID();

						UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
						ImGui::TextUnformatted(GetArgument().data());
					} else {
						if (DisplayComboBox<float>(getEnumMap(), _floatValue, a_firstColumnWidthPercent)) {
							bEdited = true;
						}

						UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
						ImGui::TextUnformatted(GetArgument().data());
					}
					break;
				}

			case Type::kGlobalVariable:
				{
					if (_globalVariable.DisplayInUI(a_bEditable, a_firstColumnWidthPercent)) {
						bEdited = true;
					}
					break;
				}
			case Type::kActorValue:
				{
					std::map<int32_t, std::string_view> actorValueTypeEnumMap;
					actorValueTypeEnumMap[0] = "Actor Value"sv;
					actorValueTypeEnumMap[1] = "Base Actor Value"sv;
					actorValueTypeEnumMap[2] = "Max Actor Value"sv;
					actorValueTypeEnumMap[3] = "Actor Value Percentage (0-1)"sv;

					if (DisplayComboBox<int32_t>(actorValueTypeEnumMap, *reinterpret_cast<int32_t*>(&_actorValueType), a_firstColumnWidthPercent)) {
						bEdited = true;
					}

					ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
					ImGui::PushID(&_actorValue);
					if (ImGui::InputInt("", reinterpret_cast<int*>(&_actorValue))) {
						bEdited = true;
					}
					ImGui::PopID();

					UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
					ImGui::TextUnformatted(GetArgument().data());
					break;
				}
			case Type::kGraphVariable:
				{
					std::map<int32_t, std::string_view> graphVariableTypeEnumMap;
					graphVariableTypeEnumMap[0] = "Float"sv;
					graphVariableTypeEnumMap[1] = "Int"sv;
					graphVariableTypeEnumMap[2] = "Bool"sv;

					if (DisplayComboBox<int32_t>(graphVariableTypeEnumMap, *reinterpret_cast<int32_t*>(&_graphVariableType), a_firstColumnWidthPercent)) {
						bEdited = true;
					}

					if (_graphVariableName.DisplayInUI(a_bEditable, a_firstColumnWidthPercent)) {
						bEdited = true;
					}
					break;
				}
			}
		} else {
			switch (_type) {
			case Type::kStaticValue:
				if (!getEnumMap) {
					ImGui::TextUnformatted("Static value");

					UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
					ImGui::TextUnformatted(GetArgument().data());
				} else {
					std::string currentEnumName;
					auto enumMap = getEnumMap();
					const auto it = enumMap.find(static_cast<int32_t>(_floatValue));
					if (it != enumMap.end()) {
						currentEnumName = it->second;
					} else {
						currentEnumName = std::format("Unknown ({})", _floatValue);
					}
					ImGui::TextUnformatted(currentEnumName.data());

					UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
					ImGui::TextUnformatted(GetArgument().data());
				}
				break;
			case Type::kGlobalVariable:
				_globalVariable.DisplayInUI(a_bEditable, a_firstColumnWidthPercent);
				break;
			case Type::kActorValue:
				ImGui::TextUnformatted(GetArgument().data());
				break;
			case Type::kGraphVariable:
				_graphVariableName.DisplayInUI(a_bEditable, a_firstColumnWidthPercent);
				break;
			}
		}

		return bEdited;
	}

	std::string NumericValue::GetArgument() const
	{
		switch (_type) {
		case Type::kStaticValue:
			{
				if (getEnumMap) {
					auto enumMap = getEnumMap();
					if (const auto nameIt = enumMap.find(static_cast<int32_t>(_floatValue)); nameIt != enumMap.end()) {
						return nameIt->second.data();
					}
				}
				return std::format("{:.3}", _floatValue);
			}
		case Type::kGlobalVariable:
			{
				if (_globalVariable.IsValid()) {
					return _globalVariable.GetValue()->GetFormEditorID();
				}
				return "(Not found)";
			}
		case Type::kActorValue:
			{
				std::string actorValueArgument = Utils::GetActorValueName(_actorValue).data();
				switch (_actorValueType) {
				case ActorValueType::kActorValue:
					break;
				case ActorValueType::kBase:
					actorValueArgument.insert(0, "Base "sv);
					break;
				case ActorValueType::kMax:
					actorValueArgument.insert(0, "Max "sv);
					break;
				case ActorValueType::kPercentage:
					actorValueArgument.append("%"sv);
					break;
				}
				return actorValueArgument;
			}
		case Type::kGraphVariable:
			{
				return _graphVariableName.GetValue().data();
			}
		}

		return "(Invalid)";
	}

	void NumericValue::Parse(rapidjson::Value& a_value)
	{
		const auto valueObject = a_value.GetObj();

		if (const auto valueIt = valueObject.FindMember("value"); valueIt != valueObject.MemberEnd() && valueIt->value.IsNumber()) {
			SetStaticValue(valueIt->value.GetFloat());
			return;
		}

		if (const auto formIt = valueObject.FindMember("form"); formIt != valueObject.MemberEnd() && formIt->value.IsObject()) {
			SetType(Type::kGlobalVariable);
			_globalVariable.Parse(formIt->value);
			return;
		}

		if (const auto actorValueIt = valueObject.FindMember("actorValue"); actorValueIt != valueObject.MemberEnd() && actorValueIt->value.IsNumber()) {
			const auto actorValueTypeIt = valueObject.FindMember("actorValueType");
			if (actorValueTypeIt != valueObject.MemberEnd() && actorValueTypeIt->value.IsString()) {
				const std::string actorValueTypeString = actorValueTypeIt->value.GetString();

				auto valueType = ActorValueType::kActorValue;
				if (actorValueTypeString == GetActorValueTypeString(ActorValueType::kActorValue)) {
					valueType = ActorValueType::kActorValue;
				} else if (actorValueTypeString == GetActorValueTypeString(ActorValueType::kBase)) {
					valueType = ActorValueType::kBase;
				} else if (actorValueTypeString == GetActorValueTypeString(ActorValueType::kMax)) {
					valueType = ActorValueType::kMax;
				} else if (actorValueTypeString == GetActorValueTypeString(ActorValueType::kPercentage)) {
					valueType = ActorValueType::kPercentage;
				}

				SetActorValue(static_cast<RE::ActorValue>(actorValueIt->value.GetInt()), valueType);
				return;
			}
		}

		if (const auto graphVariableIt = valueObject.FindMember("graphVariable"); graphVariableIt != valueObject.MemberEnd() && graphVariableIt->value.IsString()) {
			const auto graphVariableTypeIt = valueObject.FindMember("graphVariableType");
			if (graphVariableTypeIt != valueObject.MemberEnd() && graphVariableTypeIt->value.IsString()) {
				const std::string graphVariableTypeString = graphVariableTypeIt->value.GetString();

				auto valueType = GraphVariableType::kFloat;
				if (graphVariableTypeString == GetGraphVariableTypeString(GraphVariableType::kFloat)) {
					valueType = GraphVariableType::kFloat;
				} else if (graphVariableTypeString == GetGraphVariableTypeString(GraphVariableType::kInt)) {
					valueType = GraphVariableType::kInt;
				} else if (graphVariableTypeString == GetGraphVariableTypeString(GraphVariableType::kBool)) {
					valueType = GraphVariableType::kBool;
				}

				SetGraphVariable(graphVariableIt->value.GetString(), valueType);
			}
		}
	}

	void NumericValue::ParseLegacy(std::string_view a_argument, bool a_bIsActorValue /*= false*/)
	{
		if (a_bIsActorValue) {
			int32_t valueInt = -1;
			std::from_chars(a_argument.data(), a_argument.data() + a_argument.size(), valueInt);
			SetActorValue(static_cast<RE::ActorValue>(valueInt));
		} else {
			float floatValue = 0.f;
			auto [ptr, ec]{ std::from_chars(a_argument.data(), a_argument.data() + a_argument.size(), floatValue) };

			if (ec == std::errc()) {
				SetStaticValue(floatValue);
				return;
			}

			if (ec == std::errc::invalid_argument) {
				SetType(Type::kGlobalVariable);
				_globalVariable.ParseLegacy(a_argument);
			}
		}
	}

	rapidjson::Value NumericValue::Serialize(rapidjson::Document::AllocatorType& a_allocator)
	{
		rapidjson::Value object(rapidjson::kObjectType);

		switch (_type) {
		case Type::kStaticValue:
			object.AddMember("value", _floatValue, a_allocator);
			break;
		case Type::kGlobalVariable:
			object.AddMember("form", _globalVariable.Serialize(a_allocator), a_allocator);
			break;
		case Type::kActorValue:
			object.AddMember("actorValue", static_cast<int32_t>(_actorValue), a_allocator);
			object.AddMember("actorValueType", rapidjson::StringRef(GetActorValueTypeString(_actorValueType).data(), GetActorValueTypeString(_actorValueType).length()), a_allocator);
			break;
		case Type::kGraphVariable:
			object.AddMember("graphVariable", _graphVariableName.Serialize(a_allocator), a_allocator);
			object.AddMember("graphVariableType", rapidjson::StringRef(GetGraphVariableTypeString(_graphVariableType).data(), GetGraphVariableTypeString(_graphVariableType).length()), a_allocator);
		}

		return object;
	}

	bool NumericValue::GetActorValue(RE::TESObjectREFR* a_refr, float& a_outValue) const
	{
		if (_actorValue > RE::ActorValue::kNone && _actorValue < RE::ActorValue::kTotal && a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				const auto actorValueOwner = actor->AsActorValueOwner();
				a_outValue = actorValueOwner->GetActorValue(_actorValue);
				return true;
			}
		}

		return false;
	}

	bool NumericValue::GetActorValueBase(RE::TESObjectREFR* a_refr, float& a_outValue) const
	{
		if (_actorValue > RE::ActorValue::kNone && _actorValue < RE::ActorValue::kTotal && a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				const auto actorValueOwner = actor->AsActorValueOwner();
				a_outValue = actorValueOwner->GetBaseActorValue(_actorValue);
				return true;
			}
		}

		return false;
	}

	bool NumericValue::GetActorValueMax(RE::TESObjectREFR* a_refr, float& a_outValue) const
	{
		if (_actorValue > RE::ActorValue::kNone && _actorValue < RE::ActorValue::kTotal && a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				const auto actorValueOwner = actor->AsActorValueOwner();
				const float permanentValue = actorValueOwner->GetPermanentActorValue(_actorValue);
				const float temporaryValue = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, _actorValue);
				a_outValue = permanentValue + temporaryValue;
				return true;
			}
		}

		return false;
	}

	bool NumericValue::GetActorValuePercentage(RE::TESObjectREFR* a_refr, float& a_outValue) const
	{
		if (_actorValue > RE::ActorValue::kNone && _actorValue < RE::ActorValue::kTotal && a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				const auto actorValueOwner = actor->AsActorValueOwner();
				const float currentValue = actorValueOwner->GetActorValue(_actorValue);
				const float permanentValue = actorValueOwner->GetPermanentActorValue(_actorValue);
				float temporaryValue = temporaryValue = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, _actorValue);
				const float maxValue = permanentValue + temporaryValue;

				if (maxValue <= 0.f) {
					a_outValue = 0.f;
					return true;
				}

				if (maxValue < std::numeric_limits<float>::epsilon() && currentValue < std::numeric_limits<float>::epsilon()) {
					a_outValue = 0.f;
					return true;
				}

				const float percent = currentValue / maxValue;
				a_outValue = fmin(fmax(percent, 0.f), 1.f);
				return true;
			}
		}

		return false;
	}

	template <typename T>
	bool NumericValue::DisplayComboBox(std::map<int32_t, std::string_view> a_enumMap, T& a_value, float a_firstColumnWidthPercent)
	{
		bool bEdited = false;

		ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
		ImGui::PushID(&a_value);
		std::string currentEnumName;

		if (const auto it = a_enumMap.find(static_cast<int32_t>(a_value)); it != a_enumMap.end()) {
			currentEnumName = it->second;
		} else {
			currentEnumName = std::format("Unknown ({})", static_cast<int32_t>(a_value));
		}

		if (ImGui::BeginCombo("##Enum value", currentEnumName.data())) {
			for (auto& [enumValue, enumName] : a_enumMap) {
				const bool bIsCurrent = enumValue == static_cast<int32_t>(a_value);
				if (ImGui::Selectable(enumName.data(), bIsCurrent)) {
					if (!bIsCurrent) {
						a_value = static_cast<T>(enumValue);
						bEdited = true;
					}
				}
				if (bIsCurrent) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();

		return bEdited;
	}

	bool TextValue::DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
	{
		bool bEdited = false;

		if (a_bEditable) {
			ImGui::PushID(&_text);
			ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
			ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
			if (!_bAllowSpaces) {
				flags |= ImGuiInputTextFlags_CharsNoBlank;
			}
			if (ImGui::InputTextWithHint("##Text", "Text...", &_text, flags)) {
				bEdited = true;
			}
			ImGui::PopID();
		} else {
			ImGui::TextUnformatted(_text.data());
		}

		return bEdited;
	}

	void TextValue::Parse(const rapidjson::Value& a_value)
	{
		_text = (a_value.GetString());
	}

	rapidjson::Value TextValue::Serialize([[maybe_unused]] rapidjson::Document::AllocatorType& a_allocator) const
	{
		rapidjson::Value object(rapidjson::StringRef(_text.data(), _text.length()));

		return object;
	}

	bool NiPoint3Value::DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
	{
		bool bEdited = false;

		if (a_bEditable) {
			ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
			ImGui::PushID(&_value);
			if (ImGui::InputFloat3("", reinterpret_cast<float*>(&_value))) {
				bEdited = true;
			}
			ImGui::PopID();

			UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
			ImGui::TextUnformatted(GetArgument().data());
		} else {
			ImGui::TextUnformatted(GetArgument().data());
		}

		return bEdited;
	}

	std::string NiPoint3Value::GetArgument() const
	{
		return std::format("({}, {}, {})", GetValue().x, GetValue().y, GetValue().z);
	}

	void NiPoint3Value::Parse(rapidjson::Value& a_value)
	{
		if (const auto valueArray = a_value.GetArray(); valueArray.Size() == 3) {
			_value.x = valueArray[0].GetFloat();
			_value.y = valueArray[1].GetFloat();
			_value.z = valueArray[2].GetFloat();
			_bIsValid = true;
		}
	}

	rapidjson::Value NiPoint3Value::Serialize([[maybe_unused]] rapidjson::Document::AllocatorType& a_allocator) const
	{
		rapidjson::Value arrayValue(rapidjson::kArrayType);

		arrayValue.PushBack(_value.x, a_allocator);
		arrayValue.PushBack(_value.y, a_allocator);
		arrayValue.PushBack(_value.z, a_allocator);

		return arrayValue;
	}

	bool ConditionSet::EvaluateAll(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const
	{
		ReadLocker locker(_lock);

		return std::ranges::all_of(_conditions, [&](auto& a_condition) { return a_condition->Evaluate(a_refr, a_clipGenerator); });
	}

	bool ConditionSet::EvaluateAny(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator) const
	{
		ReadLocker locker(_lock);

		return std::ranges::any_of(_conditions, [&](auto& a_condition) { return a_condition->Evaluate(a_refr, a_clipGenerator); });
	}

	bool ConditionSet::HasInvalidConditions() const
	{
		ReadLocker locker(_lock);

		return std::ranges::any_of(_conditions, [&](auto& a_condition) { return !a_condition->IsValid(); });
	}

	RE::BSVisit::BSVisitControl ConditionSet::ForEachCondition(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<ICondition>&)>& a_func)
	{
		using Result = RE::BSVisit::BSVisitControl;

		ReadLocker locker(_lock);

		auto result = Result::kContinue;

		for (auto& condition : _conditions) {
			result = a_func(condition);
			if (result == Result::kStop) {
				return result;
			}
		}

		return result;
	}

	void ConditionSet::AddCondition(std::unique_ptr<ICondition>& a_condition, bool a_bSetDirty /* = false*/)
	{
		if (!a_condition) {
			return;
		}

		WriteLocker locker(_lock);

		a_condition->SetParentConditionSet(this);
		_conditions.emplace_back(std::move(a_condition));

		if (a_bSetDirty) {
			SetDirty(true);
		}
	}

	void ConditionSet::RemoveCondition(const std::unique_ptr<ICondition>& a_condition)
	{
		if (!a_condition) {
			return;
		}

		{
			WriteLocker locker(_lock);

			std::erase(_conditions, a_condition);
		}

		SetDirty(true);

		auto& detectedProblems = DetectedProblems::GetSingleton();
		if (detectedProblems.HasSubModsWithInvalidConditions()) {
			detectedProblems.CheckForSubModsWithInvalidConditions();
		}
	}

	std::unique_ptr<ICondition> ConditionSet::ExtractCondition(std::unique_ptr<ICondition>& a_condition)
	{
		WriteLocker locker(_lock);

		auto extracted = std::move(a_condition);
		std::erase(_conditions, a_condition);

		return extracted;
	}

	void ConditionSet::InsertCondition(std::unique_ptr<ICondition>& a_conditionToInsert, const std::unique_ptr<ICondition>& a_insertAfter, bool a_bSetDirty /*= false*/)
	{
		if (!a_conditionToInsert) {
			return;
		}

		WriteLocker locker(_lock);

		if (a_bSetDirty) {
			SetDirty(true);
		}

		if (a_insertAfter) {
			if (const auto it = std::ranges::find(_conditions, a_insertAfter); it != _conditions.end()) {
				a_conditionToInsert->SetParentConditionSet(this);
				_conditions.insert(it + 1, std::move(a_conditionToInsert));
				return;
			}
		}

		a_conditionToInsert->SetParentConditionSet(this);
		_conditions.emplace_back(std::move(a_conditionToInsert));
	}

	void ConditionSet::ReplaceCondition(std::unique_ptr<ICondition>& a_conditionToSubstitute, std::unique_ptr<ICondition>& a_newCondition)
	{
		if (a_conditionToSubstitute == nullptr || a_newCondition == nullptr) {
			return;
		}

		if (a_conditionToSubstitute == a_newCondition) {
			return;  // same condition - nothing to do
		}

		// try to retain condition sets from multi condition components
		std::vector<ConditionSet*> conditionSetsToSave{};
		if (const auto numComponents = a_conditionToSubstitute->GetNumComponents(); numComponents > 0) {
			for (uint32_t i = 0; i < numComponents; i++) {
				const auto component = a_conditionToSubstitute->GetComponent(i);

				if (const auto multiConditionComponent = dynamic_cast<IMultiConditionComponent*>(component)) {
					conditionSetsToSave.emplace_back(multiConditionComponent->GetConditions());
				}
			}
		}

		if (!conditionSetsToSave.empty()) {
			if (const auto numComponents = a_newCondition->GetNumComponents(); numComponents > 0) {
				uint32_t movedSets = 0;
				for (uint32_t i = 0; i < numComponents; i++) {
					const auto component = a_newCondition->GetComponent(i);

					if (const auto multiConditionComponent = dynamic_cast<IMultiConditionComponent*>(component)) {
						const auto conditionSet = multiConditionComponent->GetConditions();
						conditionSet->MoveConditions(conditionSetsToSave[movedSets++]);
					}

					if (movedSets >= conditionSetsToSave.size()) {
						break;
					}
				}
			}
		}

		{
			WriteLocker locker(_lock);

			a_newCondition->SetParentConditionSet(this);
			a_conditionToSubstitute = std::move(a_newCondition);
		}

		SetDirty(true);

		auto& detectedProblems = DetectedProblems::GetSingleton();
		if (detectedProblems.HasSubModsWithInvalidConditions()) {
			detectedProblems.CheckForSubModsWithInvalidConditions();
		}
	}

	void ConditionSet::MoveCondition(std::unique_ptr<ICondition>& a_sourceCondition, ConditionSet* a_sourceSet, const std::unique_ptr<ICondition>& a_targetCondition, bool a_bInsertAfter /*= true*/)
	{
		if (a_sourceCondition == a_targetCondition) {
			return;  // same condition - nothing to do
		}

		if (a_sourceSet == this) {
			// move within the same set
			WriteLocker locker(_lock);

			const auto sourceIndex = std::distance(_conditions.begin(), std::ranges::find(_conditions, a_sourceCondition));
			auto targetIndex = std::distance(_conditions.begin(), std::ranges::find(_conditions, a_targetCondition));

			if (a_bInsertAfter) {
				++targetIndex;
			}

			if (sourceIndex < targetIndex) {
				--targetIndex;
			}

			if (sourceIndex == targetIndex) {
				return;
			}

			auto extractedCondition = std::move(a_sourceCondition);
			_conditions.erase(_conditions.begin() + sourceIndex);
			_conditions.insert(_conditions.begin() + targetIndex, std::move(extractedCondition));

			SetDirty(true);
		} else {
			// move from other set

			// make sure the source condition is not an ancestor of this set
			if (IsChildOf(a_sourceCondition.get())) {
				return;
			}

			WriteLocker locker(_lock);
			const auto targetIt = std::ranges::find(_conditions, a_targetCondition);
			auto extractedCondition = a_sourceSet->ExtractCondition(a_sourceCondition);
			extractedCondition->SetParentConditionSet(this);
			if (targetIt != _conditions.end()) {
				if (a_bInsertAfter) {
					_conditions.insert(targetIt + 1, std::move(extractedCondition));
				} else {
					_conditions.insert(targetIt, std::move(extractedCondition));
				}
			} else {
				_conditions.emplace_back(std::move(extractedCondition));
			}

			SetDirty(true);
		}
	}

	void ConditionSet::MoveConditions(ConditionSet* a_otherSet)
	{
		WriteLocker locker(_lock);

		a_otherSet->ForEachCondition([this](auto& a_condition) {
			a_condition->SetParentConditionSet(this);
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		_conditions = std::move(a_otherSet->_conditions);
	}

	void ConditionSet::AppendConditions(ConditionSet* a_otherSet)
	{
		WriteLocker locker(_lock);

		a_otherSet->ForEachCondition([this](auto& a_condition) {
			a_condition->SetParentConditionSet(this);
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		WriteLocker otherLocker(a_otherSet->_lock);

		_conditions.reserve(_conditions.size() + a_otherSet->_conditions.size());

		_conditions.insert(_conditions.end(), std::make_move_iterator(a_otherSet->_conditions.begin()), std::make_move_iterator(a_otherSet->_conditions.end()));

		SetDirty(true);
	}

	void ConditionSet::ClearConditions()
	{
		{
			WriteLocker locker(_lock);

			_conditions.clear();
		}

		SetDirty(true);

		auto& detectedProblems = DetectedProblems::GetSingleton();
		if (detectedProblems.HasSubModsWithInvalidConditions()) {
			detectedProblems.CheckForSubModsWithInvalidConditions();
		}
	}

	rapidjson::Value ConditionSet::Serialize(rapidjson::Document::AllocatorType& a_allocator)
	{
		rapidjson::Value conditionArrayValue(rapidjson::kArrayType);

		ForEachCondition([&](auto& a_condition) {
			rapidjson::Value conditionValue(rapidjson::kObjectType);
			a_condition->Serialize(static_cast<void*>(&conditionValue), static_cast<void*>(&a_allocator));
			conditionArrayValue.PushBack(conditionValue, a_allocator);

			return RE::BSVisit::BSVisitControl::kContinue;
		});

		return conditionArrayValue;
	}

	bool ConditionSet::IsDirtyRecursive() const
	{
		ReadLocker locker(_lock);

		if (IsDirty()) {
			return true;
		}

		for (auto& condition : _conditions) {
			if (const auto multiCondition = dynamic_cast<IMultiConditionComponent*>(condition.get())) {
				if (multiCondition->GetConditions()->IsDirtyRecursive()) {
					return true;
				}
			}
		}

		return false;
	}

	void ConditionSet::SetDirtyRecursive(bool a_bDirty)
	{
		ReadLocker locker(_lock);

		for (auto& condition : _conditions) {
			if (const auto multiCondition = dynamic_cast<IMultiConditionComponent*>(condition.get())) {
				multiCondition->GetConditions()->SetDirtyRecursive(a_bDirty);
			}
		}

		SetDirty(a_bDirty);
	}

	bool ConditionSet::IsChildOf(ICondition* a_condition)
	{
		if (const auto multiCondition = dynamic_cast<IMultiConditionComponent*>(a_condition)) {
			const auto& conditionSet = multiCondition->GetConditions();
			if (conditionSet == this) {
				return true;
			}

			for (const auto& condition : multiCondition->GetConditions()->_conditions) {
				if (IsChildOf(condition.get())) {
					return true;
				}
			}
		}

		return false;
	}

	SubMod* ConditionSet::GetParentSubMod() const
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

	const ICondition* ConditionSet::GetParentCondition() const
	{
		if (_parentMultiConditionComponent) {
			return _parentMultiConditionComponent->GetParentCondition();
		}

		return nullptr;
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

	void MultiConditionComponent::InitializeComponent(void* a_value)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		const auto object = value.GetObj();
		if (const auto conditionsIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); conditionsIt != object.MemberEnd() && conditionsIt->value.IsArray()) {
			for (const auto conditionsArray = conditionsIt->value.GetArray(); auto& conditionValue : conditionsArray) {
				if (auto condition = CreateConditionFromJson(conditionValue)) {
					conditionSet->AddCondition(condition);
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
		if (conditionSet->GetNumConditions() == 1) {
			return "1 child condition";
		}
		return std::format("{} child conditions", conditionSet->GetNumConditions()).data();
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

	void RandomConditionComponent::InitializeComponent(void* a_value)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);

		const auto object = val.GetObj();

		if (const auto randomIt = object.FindMember(rapidjson::StringRef(_name.data(), _name.length())); randomIt != object.MemberEnd() && randomIt->value.IsObject()) {
			const auto randomObj = randomIt->value.GetObject();

			if (const auto minIt = randomObj.FindMember("min"); minIt != randomObj.MemberEnd() && minIt->value.IsNumber()) {
				minValue = minIt->value.GetFloat();
			}

			if (const auto maxIt = randomObj.FindMember("max"); maxIt != randomObj.MemberEnd() && maxIt->value.IsNumber()) {
				maxValue = maxIt->value.GetFloat();
			}
		}
	}

	void RandomConditionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		auto& val = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		rapidjson::Value object(rapidjson::kObjectType);
		object.AddMember("min", minValue, allocator);
		object.AddMember("max", maxValue, allocator);

		val.AddMember(rapidjson::StringRef(_name.data(), _name.length()), object, allocator);
	}

	bool RandomConditionComponent::DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
	{
		bool bEdited = false;

		const std::string argument(GetArgument());

		if (a_bEditable) {
			ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
			ImGui::PushID(&minValue);
			float tempValues[2] = { minValue, maxValue };
			if (ImGui::InputFloat2("", reinterpret_cast<float*>(&tempValues))) {
				bEdited = true;
				minValue = tempValues[0];
				maxValue = tempValues[1];
			}
			ImGui::PopID();

			UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
			ImGui::TextUnformatted(argument.data());
		} else {
			ImGui::TextUnformatted(argument.data());
		}

		return bEdited;
	}

	RE::BSString RandomConditionComponent::GetArgument() const
	{
		return std::format("Random [{}, {}]", minValue, maxValue).data();
	}

	bool RandomConditionComponent::GetRandomFloat(RE::hkbClipGenerator* a_clipGenerator, float& a_outFloat) const
	{
		if (const auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_clipGenerator)) {
			a_outFloat = activeClip->GetRandomFloat(this);
			return true;
		}

		a_outFloat = effolkronium::random_static::get<float>(minValue, maxValue);
		return false;
	}
}
