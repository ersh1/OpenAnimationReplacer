#pragma once

#include "Utils.h"
#include "UI/UICommon.h"

#include "API/OpenAnimationReplacer-SharedTypes.h"

#include <imgui_stdlib.h>
#include <rapidjson/document.h>


template <class T, class U>
concept Derived = std::is_base_of_v<U, T>;

template <typename T>
concept NonAbstract = !std::is_abstract_v<T>;

struct ReplacementTrace
{
	struct Step  // describes each step the replacement logic went over
	{
		enum class StepResult : uint8_t
		{
			kNone,

			kSuccess,
			kFail,

			kDisabled,
			kNoConditions,
		};

		struct ConditionEntry  // each condition it went over
		{
			std::string conditionName{};
			StepResult evaluationResult;
			std::vector<ConditionEntry> childConditions{};
		};

		Step(std::string_view a_modName, std::string_view a_subModName) :
			modName(a_modName), subModName(a_subModName)
		{}

		StepResult result = StepResult::kNone;
		std::string modName{};
		std::string subModName{};
		std::vector<ConditionEntry> conditions{};
		std::vector<ConditionEntry> synchronizedConditions{};
	};

	void StartNewTrace();

	void TraceAnimation(const class ReplacementAnimation* a_replacement);

	void SetTraceAnimationResult(Step::StepResult a_result)
	{
		steps.back().result = a_result;
	}

	void TraceCondition(Conditions::ICondition* a_condition, Step::StepResult a_result);
	void StartTracingMultiCondition();
	void EndTracingMultiCondition(Conditions::ICondition* a_condition, Step::StepResult a_result);

	void SetEvaluatingSynchronizedConditions(bool a_bSynchronized) { bSynchronized = a_bSynchronized; }

	std::vector<Step> steps{};
	bool bSynchronized = false;

	std::stack<std::vector<Step::ConditionEntry>> childStack{};
};

namespace Components
{
	template <Derived<RE::TESForm> T>
	class TESFormValue
	{
	public:
		bool IsValid() const { return _form != nullptr; }
		T* GetValue() const { return _form; }

		void SetValue(T* a_form)
		{
			_form = a_form;

			if (_form) {
				RE::FormID fileIndex = 0;

				if ((_form->formID & 0xFF000000) == 0) {
					_pluginNameString = "Skyrim.esm"sv;
				} else if (auto file = _form->GetFile(0)) {
					fileIndex = file->compileIndex << (3 * 8);
					fileIndex += file->smallFileCompileIndex << ((1 * 8) + 4);
					_pluginNameString = file->GetFilename();
				} else {
					_pluginNameString = "Skyrim.esm"sv;
				}

				RE::FormID localFormID = _form->formID & ~fileIndex;
				_formIdString = std::format("{:X}", localFormID);
			}
		}

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
		{
			bool bEdited = false;

			if (a_bEditable) {
				ImGui::PushID(this);
				const float formIDWidth = ImGui::GetFontSize() * 4;
				const float pluginNameWidth = UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent) - formIDWidth - ImGui::GetStyle().ItemSpacing.x;
				ImGui::SetNextItemWidth(pluginNameWidth);
				if (ImGui::InputTextWithHint("##PluginName", "Plugin name...", &_pluginNameString, ImGuiInputTextFlags_CallbackAlways, &TESFormValue::FormInputTextCallback, this)) {
					bEdited = true;
				}
				ImGui::SameLine();
				ImGui::SetNextItemWidth(formIDWidth);
				if (UI::UICommon::InputTextWithHint("##FormID", "FormID", &_formIdString, 6, ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase, &TESFormValue::FormInputTextCallback, this)) {
					bEdited = true;
				}
				//ImGui::TableSetColumnIndex(1);
				UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
				ImGui::TextUnformatted(GetArgument().data());
				ImGui::PopID();
			} else {
				if (_form) {
					ImGui::TextUnformatted(std::format("[{:08X}]", _form->GetFormID()).data());
					//ImGui::TableSetColumnIndex(1);
					UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
					ImGui::TextUnformatted(GetArgument().data());
				} else {
					ImGui::TextUnformatted(std::format("{}, {}", _pluginNameString, _formIdString).data());
					//ImGui::TableSetColumnIndex(1);
					UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
					ImGui::TextUnformatted("(Not found)"sv.data());
				}
			}

			return bEdited;
		}

		std::string GetArgument() const
		{
			if (_form) {
				switch (T::FORMTYPE) {
				case RE::FormType::Global:
					{
						const auto global = reinterpret_cast<RE::TESGlobal*>(_form);
						return std::format("{} ({})", global->value, global->GetFormEditorID());
					}
				case RE::FormType::Keyword:
				case RE::FormType::LocationRefType:
					return _form->GetFormEditorID();
				default:
					return Utils::GetFormNameString(_form);
				}
			}
			return "(Not found)";
		}

		void Parse(rapidjson::Value& a_value)
		{
			const auto formObject = a_value.GetObj();
			if (const auto pluginNameIt = formObject.FindMember("pluginName"); pluginNameIt != formObject.MemberEnd() && pluginNameIt->value.IsString()) {
				if (const auto formIDIt = formObject.FindMember("formID"); formIDIt != formObject.MemberEnd() && formIDIt->value.IsString()) {
					_pluginNameString = Utils::TrimWhitespace(pluginNameIt->value.GetString());
					_formIdString = Utils::TrimHexPrefix(Utils::TrimWhitespace(formIDIt->value.GetString()));
					RE::FormID formID;
					auto [ptr, ec] = std::from_chars(_formIdString.data(), _formIdString.data() + _formIdString.size(), formID, 16);
					if (ec == std::errc()) {
						_formIdString = std::format("{:X}", formID);

						auto form = Utils::LookupForm<T>(formID, _pluginNameString);
						SetValue(form);
					}
				}
			}
		}

		void ParseLegacy(std::string_view a_argument)
		{
			if (const size_t splitPos = a_argument.find('|'); splitPos != std::string_view::npos) {
				_pluginNameString = Utils::TrimQuotes(Utils::TrimWhitespace(a_argument.substr(0, splitPos)));
				_formIdString = std::string(Utils::TrimHexPrefix(Utils::TrimWhitespace(a_argument.substr(splitPos + 1))));
				RE::FormID formID;
				auto [ptr, ec] = std::from_chars(_formIdString.data(), _formIdString.data() + _formIdString.size(), formID, 16);
				if (ec == std::errc()) {
					_formIdString = std::format("{:X}", formID);

					auto form = Utils::LookupForm<T>(formID, _pluginNameString);
					SetValue(form);
				}
			}
		}

		rapidjson::Value Serialize(rapidjson::Document::AllocatorType& a_allocator) const
		{
			rapidjson::Value result(rapidjson::kObjectType);
			result.AddMember("pluginName", rapidjson::StringRef(_pluginNameString.data(), _pluginNameString.length()), a_allocator);
			result.AddMember("formID", rapidjson::StringRef(_formIdString.data(), _formIdString.length()), a_allocator);
			return result;
		}

		void LookupForm()
		{
			RE::FormID formID;
			auto [ptr, ec]{ std::from_chars(_formIdString.data(), _formIdString.data() + _formIdString.size(), formID, 16) };
			if (ec == std::errc()) {
				_form = Utils::LookupForm<T>(formID, _pluginNameString);
			} else {
				_form = nullptr;
			}
		}

	protected:
		static int FormInputTextCallback(struct ImGuiInputTextCallbackData* a_data)
		{
			auto* value = static_cast<TESFormValue*>(a_data->UserData);
			value->LookupForm();

			return 0;
		}

		T* _form = nullptr;
		std::string _pluginNameString{};
		std::string _formIdString{};
	};

	class TextValue
	{
	public:
		[[nodiscard]] std::string_view GetValue() const { return _text; }
		void SetValue(std::string_view a_text) { _text = a_text; }
		void SetTextHint(std::string_view a_text) { _uiTextHint = a_text; }

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent);

		void Parse(const rapidjson::Value& a_value);

		rapidjson::Value Serialize([[maybe_unused]] rapidjson::Document::AllocatorType& a_allocator) const;

		void SetAllowSpaces(bool a_bAllowSpaces) { _bAllowSpaces = a_bAllowSpaces; }

	protected:
		std::string _text{};
		std::string _uiTextHint = "Text...";
		bool _bAllowSpaces = true;
	};

	class NumericValue
	{
	public:
		enum class Type : int
		{
			kStaticValue,
			kGlobalVariable,
			kActorValue,
			kGraphVariable,

			kTotal,
			kNone = kTotal
		};

		NumericValue() {
			_graphVariableName.SetTextHint("Graph variable name...");
		}

		[[nodiscard]] bool IsValid() const { return _type == Type::kGlobalVariable ? _globalVariable.IsValid() : true; }

		[[nodiscard]] float GetValue(RE::TESObjectREFR* a_refr) const;

		ActorValueType GetActorValueType() const { return _actorValueType; }
		RE::ActorValue GetActorValue() const { return _actorValue; }

		GraphVariableType GetGraphVariableType() const { return _graphVariableType; }
		std::string_view GetGraphVariableName() const { return _graphVariableName.GetValue(); }

		void SetType(Type a_type) { _type = a_type; }

		void SetStaticValue(float a_value)
		{
			_floatValue = a_value;
			_type = Type::kStaticValue;
		}

		void SetGlobalVariable(RE::TESGlobal* a_global)
		{
			_globalVariable.SetValue(a_global);
			_type = Type::kGlobalVariable;
		}

		void SetActorValue(RE::ActorValue a_actorValue)
		{
			_actorValue = a_actorValue;
			_type = Type::kActorValue;
		}

		void SetActorValue(RE::ActorValue a_actorValue, ActorValueType a_valueType)
		{
			_actorValue = a_actorValue;
			_actorValueType = a_valueType;
			_type = Type::kActorValue;
		}

		void SetActorValueType(ActorValueType a_valueType)
		{
			_actorValueType = a_valueType;
			_type = Type::kActorValue;
		}

		void SetGraphVariable(std::string_view a_graphVariableName, GraphVariableType a_valueType)
		{
			_graphVariableName.SetValue(a_graphVariableName);
			_graphVariableType = a_valueType;
			_type = Type::kGraphVariable;
		}

		void SetStaticRange(float a_min, float a_max)
		{
			_staticRange = { a_min, a_max };
			_type = Type::kStaticValue;
			_forcedType = Type::kStaticValue;
		}

		void SetStaticInteger(bool a_bIsInteger)
		{
			_bStaticInteger = a_bIsInteger;
		}

		void SetForcedType(Type a_forcedType)
		{
			_type = a_forcedType;
			_forcedType = a_forcedType;
		}

		[[nodiscard]] Type GetType() const { return _type; }

		[[nodiscard]] std::string_view GetTypeName() const
		{
			switch (_type) {
			case Type::kStaticValue:
				if (getEnumMap) {
					return "Enum Value"sv;
				}
				return "Static Value"sv;
			case Type::kGlobalVariable:
				return "Global Variable"sv;
			case Type::kActorValue:
				return "Actor Value"sv;
			case Type::kGraphVariable:
				return "Graph Variable"sv;
			}

			return ""sv;
		}

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent);

		[[nodiscard]] std::string GetArgument() const;

		void Parse(rapidjson::Value& a_value);

		void ParseLegacy(std::string_view a_argument, bool a_bIsActorValue = false);

		rapidjson::Value Serialize(rapidjson::Document::AllocatorType& a_allocator);

		std::function<std::map<int32_t, std::string_view>()> getEnumMap = nullptr;
		std::function<std::map<uint32_t, std::string_view>()> getUnsignedEnumMap = nullptr;

		bool HasEnumMap() const { return getEnumMap || getUnsignedEnumMap; }

	protected:
		Type _type = Type::kStaticValue;
		Type _forcedType = Type::kNone;
		bool _bStaticInteger = false;

		// static value
		float _floatValue = 0.f;
		std::optional<std::pair<float, float>> _staticRange = std::nullopt;

		// global variable
		TESFormValue<RE::TESGlobal> _globalVariable;

		// actor value
		ActorValueType _actorValueType = ActorValueType::kActorValue;
		RE::ActorValue _actorValue = RE::ActorValue::kNone;

		// graph variable
		GraphVariableType _graphVariableType = GraphVariableType::kFloat;
		TextValue _graphVariableName;

		bool GetActorValue(RE::TESObjectREFR* a_refr, float& a_outValue) const;
		bool GetActorValueBase(RE::TESObjectREFR* a_refr, float& a_outValue) const;
		bool GetActorValueMax(RE::TESObjectREFR* a_refr, float& a_outValue) const;
		bool GetActorValuePercentage(RE::TESObjectREFR* a_refr, float& a_outValue) const;

		static constexpr std::string_view GetActorValueTypeString(ActorValueType a_type)
		{
			switch (a_type) {
			case ActorValueType::kActorValue:
				return "Value"sv;
			case ActorValueType::kBase:
				return "Base"sv;
			case ActorValueType::kMax:
				return "Max"sv;
			case ActorValueType::kPercentage:
				return "Percentage"sv;
			default:
				return ""sv;
			}
		}

		static constexpr std::string_view GetGraphVariableTypeString(GraphVariableType a_type)
		{
			switch (a_type) {
			case GraphVariableType::kFloat:
				return "Float"sv;
			case GraphVariableType::kInt:
				return "Int"sv;
			case GraphVariableType::kBool:
				return "Bool"sv;
			default:
				return ""sv;
			}
		}

	private:
		template <typename Key, typename T>
		bool DisplayComboBox(std::map<Key, std::string_view> a_enumMap, T& a_value, float a_firstColumnWidthPercent);
	};

	class NiPoint3Value
	{
	public:
		[[nodiscard]] const RE::NiPoint3& GetValue() const { return _value; }

		void SetValue(const RE::NiPoint3& a_value)
		{
			_value = a_value;
			_bIsValid = true;
		}

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent);

		[[nodiscard]] std::string GetArgument() const;

		void Parse(rapidjson::Value& a_value);

		rapidjson::Value Serialize([[maybe_unused]] rapidjson::Document::AllocatorType& a_allocator) const;

		[[nodiscard]] bool IsValid() const { return _bIsValid; }

	protected:
		RE::NiPoint3 _value;
		bool _bIsValid = false;
	};

	template <Derived<RE::BGSKeyword> T>
	class KeywordValue
	{
	public:
		KeywordValue() = default;

		KeywordValue(const KeywordValue& a_rhs) :
			_type(a_rhs._type),
			_keywordForm(a_rhs._keywordForm),
			_keywordLiteral(a_rhs._keywordLiteral),
			_keywordFormsMatchingLiteral(a_rhs._keywordFormsMatchingLiteral) {}

		KeywordValue& operator=(KeywordValue&& a_rhs) noexcept
		{
			_type = a_rhs._type;
			_keywordLiteral = a_rhs._keywordLiteral;
			_keywordForm = a_rhs._keywordForm;
			_keywordFormsMatchingLiteral = a_rhs._keywordFormsMatchingLiteral;

			return *this;
		}

		enum class Type : uint8_t
		{
			kLiteral,
			kForm
		};

		bool IsValid() const
		{
			if (_keywordForm.IsValid()) {
				return true;
			}
			ReadLocker locker(_dataLock);
			return !_keywordFormsMatchingLiteral.empty();
		}

		T* GetFormValue() const { return _keywordForm.GetValue(); }

		bool HasKeyword(const RE::BGSKeywordForm* a_keywordForm) const
		{
			bool bFound = false;
			ForEachKeyword([&](T* a_keyword) {
				if (a_keywordForm->HasKeyword(a_keyword)) {
					bFound = true;
					return RE::BSContainer::ForEachResult::kStop;
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});

			return bFound;
		}

		void SetKeyword(T* a_keyword)
		{
			WriteLocker locker(_dataLock);
			_keywordFormsMatchingLiteral.clear();

			_type = Type::kForm;
			_keywordForm.SetValue(a_keyword);
		}

		void SetLiteral(std::string_view a_literal)
		{
			_keywordLiteral = a_literal;
			_type = Type::kLiteral;
			LookupFromLiteral();
		}

		Type GetType() const { return _type; }
		std::string_view GetTypeName() const { return _type == Type::kLiteral ? "Literal"sv : "Form"sv; }

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
		{
			bool bEdited = false;

			if (a_bEditable) {
				ImGui::PushID(&_type);
				ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
				if (ImGui::SliderInt("", reinterpret_cast<int*>(&_type), 0, 1, GetTypeName().data())) {
					// Lookup cached form again if type changed
					switch (_type) {
					case Type::kLiteral:
						LookupFromLiteral();
						break;
					case Type::kForm:
						_keywordForm.LookupForm();
						break;
					}
					bEdited = true;
				}
				ImGui::PopID();

				switch (_type) {
				case Type::kLiteral:
					ImGui::SetNextItemWidth(UI::UICommon::FirstColumnWidth(a_firstColumnWidthPercent));
					if (ImGui::InputTextWithHint("##Keyword", "Keyword...", &_keywordLiteral, ImGuiInputTextFlags_CallbackAlways, &KeywordValue::KeywordInputTextCallback, this)) {
						bEdited = true;
					}
					UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
					if (_keywordFormsMatchingLiteral.size() > 0) {
						std::string formIDs{};
						bool bIsFirst = true;
						ForEachKeyword([&](auto a_kywd) {
							if (!bIsFirst) {
								formIDs.append(", "sv);
							}
							formIDs.append(std::format("[{:08X}]", a_kywd->GetFormID()));
							bIsFirst = false;
							return RE::BSContainer::ForEachResult::kContinue;
						});
						ImGui::TextUnformatted(formIDs.data());
					} else {
						ImGui::TextUnformatted("(Not found)");
					}
					break;
				case Type::kForm:
					if (_keywordForm.DisplayInUI(a_bEditable, a_firstColumnWidthPercent)) {
						bEdited = true;
					}
					break;
				}
			} else {
				switch (_type) {
				case Type::kLiteral:
					ImGui::TextUnformatted(_keywordLiteral.data());
					UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
					if (_keywordFormsMatchingLiteral.size() > 0) {
						std::string formIDs{};
						bool bIsFirst = true;
						ForEachKeyword([&](auto a_kywd) {
							if (!bIsFirst) {
								formIDs.append(", "sv);
							}
							formIDs.append(std::format("[{:08X}]", a_kywd->GetFormID()));
							bIsFirst = false;
							return RE::BSContainer::ForEachResult::kContinue;
						});
						ImGui::TextUnformatted(formIDs.data());
					} else {
						ImGui::TextUnformatted("(Not found)");
					}
					break;
				case Type::kForm:
					_keywordForm.DisplayInUI(a_bEditable, a_firstColumnWidthPercent);
					break;
				}
			}

			return bEdited;
		}

		std::string GetArgument() const
		{
			switch (_type) {
			case Type::kLiteral:
				return _keywordLiteral;

			case Type::kForm:
				if (_keywordForm.IsValid()) {
					return _keywordForm.GetValue()->GetFormEditorID();
				}

			default:
				return "(Not found)";
			}
		}

		void Parse(rapidjson::Value& a_value)
		{
			const auto object = a_value.GetObj();

			if (const auto keywordIt = object.FindMember("editorID"); keywordIt != object.MemberEnd() && keywordIt->value.IsString()) {
				const std::string_view editorID = keywordIt->value.GetString();
				_keywordLiteral = editorID;

				LookupFromLiteral();

				return;
			}

			if (const auto formIt = object.FindMember("form"); formIt != object.MemberEnd() && formIt->value.IsObject()) {
				_type = Type::kForm;
				_keywordForm.Parse(formIt->value);
			}
		}

		void ParseLegacy(std::string_view a_argument)
		{
			_type = Type::kForm;
			_keywordForm.ParseLegacy(a_argument);
		}

		rapidjson::Value Serialize(rapidjson::Document::AllocatorType& a_allocator)
		{
			rapidjson::Value object(rapidjson::kObjectType);

			switch (_type) {
			case Type::kLiteral:
				object.AddMember("editorID", rapidjson::StringRef(_keywordLiteral.data(), _keywordLiteral.length()), a_allocator);
				break;
			case Type::kForm:
				object.AddMember("form", _keywordForm.Serialize(a_allocator), a_allocator);
				break;
			}

			return object;
		}

		void LookupFromLiteral()
		{
			WriteLocker locker(_dataLock);
			_keywordForm.SetValue(nullptr);
			_keywordFormsMatchingLiteral.clear();
			_type = Type::kLiteral;

			auto& keywords = RE::TESDataHandler::GetSingleton()->GetFormArray<T>();
			for (auto& kywd : keywords) {
				if (kywd && kywd->formEditorID == std::string_view(_keywordLiteral)) {
					_keywordFormsMatchingLiteral.emplace_back(kywd);
				}
			}
		}

		void ForEachKeyword(std::function<RE::BSContainer::ForEachResult(T*)> a_callback) const
		{
			if (_type == Type::kForm) {
				if (_keywordForm.IsValid()) {
					a_callback(_keywordForm.GetValue());
				}
				return;
			}

			ReadLocker locker(_dataLock);
			for (auto& kywd : _keywordFormsMatchingLiteral) {
				if (a_callback(kywd) == RE::BSContainer::ForEachResult::kStop) {
					return;
				}
			}
		}

	protected:
		static int KeywordInputTextCallback(struct ImGuiInputTextCallbackData* a_data)
		{
			auto* value = static_cast<KeywordValue*>(a_data->UserData);

			value->LookupFromLiteral();

			return 0;
		}

		Type _type = Type::kLiteral;

		TESFormValue<T> _keywordForm;

		std::string _keywordLiteral{};
		mutable SharedLock _dataLock{};
		std::vector<T*> _keywordFormsMatchingLiteral{};
	};

}
