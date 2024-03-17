#pragma once
#include <imgui.h>
#include <imgui_stdlib.h>
#include <rapidjson/document.h>
#include <shared_mutex>

#include "UI/UICommon.h"
#include "Utils.h"

#include "API/OpenAnimationReplacer-ConditionTypes.h"

class ReplacerMod;
template <class T, class U>
concept Derived = std::is_base_of_v<U, T>;

template <typename T>
concept NonAbstract = !std::is_abstract_v<T>;

class SubMod;

namespace Conditions
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

				if (auto file = _form->GetFile(0)) {
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

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent);

		void Parse(const rapidjson::Value& a_value);

		rapidjson::Value Serialize([[maybe_unused]] rapidjson::Document::AllocatorType& a_allocator) const;

		void SetAllowSpaces(bool a_bAllowSpaces) { _bAllowSpaces = a_bAllowSpaces; }

	protected:
		std::string _text{};
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

			kTotal
		};

		[[nodiscard]] bool IsValid() const { return _type == Type::kGlobalVariable ? _globalVariable.IsValid() : true; }

		[[nodiscard]] float GetValue(RE::TESObjectREFR* a_refr) const;

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

	protected:
		Type _type = Type::kStaticValue;

		// static value
		float _floatValue = 0.f;

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
		template <typename T>
		bool DisplayComboBox(std::map<int32_t, std::string_view> a_enumMap, T& a_value, float a_firstColumnWidthPercent);
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

		[[nodiscard]] ConditionType GetConditionType() const override { return ConditionType::kNormal; }
		[[nodiscard]] ICondition* GetWrappedCondition() const override { return nullptr; }

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

	private:
		std::vector<std::unique_ptr<IConditionComponent>> _components;
	};

	class ConditionSet
	{
	public:
		ConditionSet() = default;

		ConditionSet(SubMod* a_parentSubMod) :
			_parentSubMod(a_parentSubMod) {}

		ConditionSet(IMultiConditionComponent* a_parentMultiConditionComponent) :
			_parentMultiConditionComponent(a_parentMultiConditionComponent) {}

		bool EvaluateAll(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, SubMod* a_parentSubMod) const;
		bool EvaluateAny(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, SubMod* a_parentSubMod) const;

		bool IsEmpty() const { return _conditions.empty(); }
		bool IsDirty() const { return _bDirty; }
		void SetDirty(bool a_bDirty) { _bDirty = a_bDirty; }
		bool HasInvalidConditions() const;
		RE::BSVisit::BSVisitControl ForEachCondition(const std::function<RE::BSVisit::BSVisitControl(std::unique_ptr<ICondition>&)>& a_func);
		void AddCondition(std::unique_ptr<ICondition>& a_condition, bool a_bSetDirty = false);
		void RemoveCondition(const std::unique_ptr<ICondition>& a_condition);
		std::unique_ptr<ICondition> ExtractCondition(std::unique_ptr<ICondition>& a_condition);
		void InsertCondition(std::unique_ptr<ICondition>& a_conditionToInsert, const std::unique_ptr<ICondition>& a_insertAfter, bool a_bSetDirty = false);
		void ReplaceCondition(std::unique_ptr<ICondition>& a_conditionToSubstitute, std::unique_ptr<ICondition>& a_newCondition);
		void MoveCondition(std::unique_ptr<ICondition>& a_sourceCondition, ConditionSet* a_sourceSet, const std::unique_ptr<ICondition>& a_targetCondition, bool a_bInsertAfter = false);
		void MoveConditions(ConditionSet* a_otherSet);
		void AppendConditions(ConditionSet* a_otherSet);
		void ClearConditions();

		void SetAsParent(std::unique_ptr<ICondition>& a_condition);

		rapidjson::Value Serialize(rapidjson::Document::AllocatorType& a_allocator);

		bool IsDirtyRecursive() const;
		void SetDirtyRecursive(bool a_bDirty);
		bool IsChildOf(ICondition* a_condition);
		size_t GetNumConditions() const { return _conditions.size(); }
		std::string GetNumConditionsText() const;

		[[nodiscard]] SubMod* GetParentSubMod() const;
		[[nodiscard]] const ICondition* GetParentCondition() const;

	private:
		mutable std::shared_mutex _lock;
		std::vector<std::unique_ptr<ICondition>> _conditions;
		bool _bDirty = false;

		SubMod* _parentSubMod = nullptr;
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

		[[nodiscard]] bool IsValid() const override { return !conditionSet->IsEmpty() && !conditionSet->HasInvalidConditions(); }

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

		TESFormValue<RE::TESForm> form;
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
		void SetActorValue(RE::ActorValue a_actorValue, ActorValueType a_valueType) override { value.SetActorValue(a_actorValue, a_valueType); }
		void SetGraphVariable(const char* a_graphVariableName, GraphVariableType a_valueType) override { value.SetGraphVariable(a_graphVariableName, a_valueType); }

		NumericValue value;
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

		NiPoint3Value value;
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

		KeywordValue<RE::BGSKeyword> keyword;
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

		KeywordValue<RE::BGSLocationRefType> locRefType;
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

		TextValue text;
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

	class RandomConditionComponent : public IRandomConditionComponent
	{
	public:
		RandomConditionComponent(const ICondition* a_parentCondition, const char* a_name, const char* a_description = "") :
			IRandomConditionComponent(a_parentCondition, a_name, a_description) {}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override;

		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] bool IsValid() const override { return true; }

		bool GetRandomFloat(RE::hkbClipGenerator* a_clipGenerator, float& a_outFloat, void* a_parentSubMod) const override;
		[[nodiscard]] float GetMinValue() const override { return minValue; }
		[[nodiscard]] float GetMaxValue() const override { return maxValue; }
		void SetMinValue(float a_min) override { minValue = a_min; }
		void SetMaxValue(float a_max) override { maxValue = a_max; }

		float minValue = 0.f;
		float maxValue = 1.f;
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

		[[nodiscard]] bool IsValid() const override { return conditionPreset && !conditionPreset->IsEmpty() && !conditionPreset->HasInvalidConditions(); }

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
