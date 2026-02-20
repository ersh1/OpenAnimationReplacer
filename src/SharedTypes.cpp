#include "SharedTypes.h"

#include "ReplacementAnimation.h"
#include "ReplacerMods.h"

void ReplacementTrace::StartNewTrace()
{
	steps.clear();
}

void ReplacementTrace::TraceAnimation(const ReplacementAnimation* a_replacement)
{
	auto parentSubMod = a_replacement->GetParentSubMod();
	auto parentMod = parentSubMod->GetParentMod();

	steps.emplace_back(parentMod->GetName().data(), parentSubMod->GetName().data());
}

void ReplacementTrace::TraceCondition(Conditions::ICondition* a_condition, Step::StepResult a_result)
{
	std::string conditionName = std::format("{}{}({})", a_condition->IsNegated() ? "NOT "sv : ""sv, a_condition->GetName().c_str(), a_condition->GetArgument().c_str());

	if (childStack.empty()) {
		std::vector<Step::ConditionEntry>& conditions = bSynchronized ? steps.back().synchronizedConditions : steps.back().conditions;
		conditions.emplace_back(conditionName, a_result);
	} else {
		childStack.top().emplace_back(conditionName, a_result);
	}
}

void ReplacementTrace::StartTracingMultiCondition()
{
	childStack.emplace();
}

void ReplacementTrace::EndTracingMultiCondition(Conditions::ICondition* a_condition, Step::StepResult a_result)
{
	std::string conditionName = std::format("{}{}({})", a_condition->IsNegated() ? "NOT "sv : ""sv, a_condition->GetName().c_str(), a_condition->GetArgument().c_str());

	auto childSteps = childStack.top();
	childStack.pop();

	if (childStack.empty()) {
		std::vector<Step::ConditionEntry>& conditions = bSynchronized ? steps.back().synchronizedConditions : steps.back().conditions;
		conditions.emplace_back(conditionName, a_result, childSteps);
	} else {
		childStack.top().emplace_back(conditionName, a_result, childSteps);
	}
}

namespace Components
{
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
			if (_forcedType == Type::kNone && !_staticRange && !(HasEnumMap() && _type == Type::kStaticValue)) {
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
					if (HasEnumMap()) {
						if (getEnumMap) {
							if (DisplayComboBox<int32_t, float>(getEnumMap(), _floatValue, a_firstColumnWidthPercent)) {
								bEdited = true;
							}
						} else {
							if (DisplayComboBox<uint32_t, float>(getUnsignedEnumMap(), _floatValue, a_firstColumnWidthPercent)) {
								bEdited = true;
							}
						}

						UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
						ImGui::TextUnformatted(GetArgument().data());
					} else if (_staticRange) {
						ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
						ImGui::PushID(&_floatValue);
						if (_bStaticInteger) {
							int tempInt = static_cast<int>(_floatValue);
							if (ImGui::SliderInt("", &tempInt, static_cast<int>(_staticRange->first), static_cast<int>(_staticRange->second), "%d", ImGuiSliderFlags_AlwaysClamp)) {
								_floatValue = static_cast<float>(tempInt);
								bEdited = true;
							}
						} else {
							if (ImGui::SliderFloat("", &_floatValue, _staticRange->first, _staticRange->second, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
								bEdited = true;
							}
						}

						ImGui::PopID();

						UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
						ImGui::TextUnformatted(GetArgument().data());
					} else {
						ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
						ImGui::PushID(&_floatValue);
						if (_bStaticInteger) {
							int tempInt = static_cast<int>(_floatValue);
							if (ImGui::InputInt("", &tempInt, 1, 10, ImGuiSliderFlags_AlwaysClamp)) {
								_floatValue = static_cast<float>(tempInt);
								bEdited = true;
							}
						} else {
							if (ImGui::InputFloat("", &_floatValue, 0.01f, 1.0f, "%.3f")) {
								bEdited = true;
							}
						}

						ImGui::PopID();

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
				if (!HasEnumMap()) {
					ImGui::TextUnformatted("Static value");

					UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
					ImGui::TextUnformatted(GetArgument().data());
				} else {
					std::string currentEnumName = std::format("Unknown ({})", _floatValue);
					if (getEnumMap) {
						auto enumMap = getEnumMap();
						const auto it = enumMap.find(static_cast<int32_t>(_floatValue));
						if (it != enumMap.end()) {
							currentEnumName = it->second;
						}
					} else {
						auto enumMap = getUnsignedEnumMap();
						const auto it = enumMap.find(static_cast<uint32_t>(_floatValue));
						if (it != enumMap.end()) {
							currentEnumName = it->second;
						}
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
				if (HasEnumMap()) {
					if (getEnumMap) {
						auto enumMap = getEnumMap();
						if (const auto nameIt = enumMap.find(static_cast<int32_t>(_floatValue)); nameIt != enumMap.end()) {
							return nameIt->second.data();
						}
					} else {
						auto enumMap = getUnsignedEnumMap();
						if (const auto nameIt = enumMap.find(static_cast<uint32_t>(_floatValue)); nameIt != enumMap.end()) {
							return nameIt->second.data();
						}
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
			break;
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

	template <typename Key, typename T>
	bool NumericValue::DisplayComboBox(std::map<Key, std::string_view> a_enumMap, T& a_value, float a_firstColumnWidthPercent)
	{
		bool bEdited = false;

		ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
		ImGui::PushID(&a_value);
		std::string currentEnumName;

		if (const auto it = a_enumMap.find(static_cast<Key>(a_value)); it != a_enumMap.end()) {
			currentEnumName = it->second;
		} else {
			currentEnumName = std::format("Unknown ({})", static_cast<Key>(a_value));
		}

		if (ImGui::BeginCombo("##Enum value", currentEnumName.data())) {
			for (auto& [enumValue, enumName] : a_enumMap) {
				const bool bIsCurrent = enumValue == static_cast<Key>(a_value);
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
			if (ImGui::InputTextWithHint("##Text", _uiTextHint.data(), &_text, flags)) {
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

}
