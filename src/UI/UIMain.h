#pragma once
#include "UIWindow.h"

#include "OpenAnimationReplacer.h"
#include <imgui_internal.h>

namespace UI
{
	class UIMain : public UIWindow
	{
	public:
		// Drag & Drop payload struct
		struct DragConditionPayload
		{
			DragConditionPayload(std::unique_ptr<Conditions::ICondition>& a_condition, Conditions::ConditionSet* a_conditionSet) :
				condition(a_condition), conditionSet(a_conditionSet) {}

			std::unique_ptr<Conditions::ICondition>& condition;
			Conditions::ConditionSet* conditionSet;
		};

		struct SubModNameFilterResult
		{
			bool bDisplay = true;
			bool bDuplicateName = false;
		};

		struct ConditionInfo
		{
			ConditionInfo(std::string_view a_name, std::string_view a_description, ImVec4 a_textColor = ImVec4(1.f, 1.f, 1.f, 1.f), std::string_view a_requiredPlugin = ""sv, std::string_view a_requiredPluginAuthor = ""sv, REL::Version a_requiredPluginVersion = { 0, 0, 0 }) :
				name(a_name), description(a_description), textColor(a_textColor), requiredPluginName(a_requiredPlugin), requiredPluginAuthor(a_requiredPluginAuthor), requiredVersion(a_requiredPluginVersion) {}

			ConditionInfo(const Conditions::ICondition* a_condition) :
				name(a_condition->GetName()),
				description(a_condition->GetDescription()),
				requiredPluginName(a_condition->GetRequiredPluginName()),
				requiredPluginAuthor(a_condition->GetRequiredPluginAuthor()),
				requiredVersion(a_condition->GetRequiredVersion())
			{
				switch (a_condition->GetConditionType()) {
				case Conditions::ConditionType::kPreset:
					textColor = UICommon::CONDITION_PRESET_COLOR;
					break;
				case Conditions::ConditionType::kCustom:
					textColor = UICommon::CUSTOM_CONDITION_COLOR;
					break;
				case Conditions::ConditionType::kNormal:
				default:
					textColor = UICommon::DEFAULT_CONDITION_COLOR;
					break;
				}
			}

			std::string name;
			std::string description;
			ImVec4 textColor;
			std::string requiredPluginName;
			std::string requiredPluginAuthor;
			REL::Version requiredVersion;
		};

		struct FilteredConditionInfo
		{
			FilteredConditionInfo(int a_index, int a_score) :
				index(a_index),
				score(a_score)
			{}

			int index;
			int score;

			bool operator<(const FilteredConditionInfo& a_other) const noexcept
			{
				return score < a_other.score;
			}
		};

		struct ComboFilterData
		{
			std::string filterString{};
			struct
			{
				const ConditionInfo* info;
				int index;
			} initialValues{ nullptr, -1 };
			int currentSelection = -1;

			std::vector<FilteredConditionInfo> filteredInfos{};
			bool filterStatus = false;

			bool SetNewValue(const ConditionInfo* a_conditionInfo, int a_newIndex) noexcept;
			bool SetNewValue(const ConditionInfo* a_conditionInfo) noexcept;
			void ResetToInitialValue() noexcept;
		};

	protected:
		bool ShouldDrawImpl() const override;
		void DrawImpl() override;
		void OnOpen() override;
		void OnClose() override;

	private:
		void DrawSettings(const ImVec2& a_pos);
		void DrawMissingPlugins();
		void DrawInvalidPlugins();
		void DrawConflictingSubMods() const;
		void DrawReplacerModsWithInvalidConditions() const;
		void DrawSubModsWithInvalidConditions() const;
		void DrawReplacerMods();
		void DrawReplacerMod(ReplacerMod* a_replacerMod, std::unordered_map<std::string, SubModNameFilterResult>& a_filterResults);
		void DrawSubMod(ReplacerMod* a_replacerMod, SubMod* a_subMod, bool a_bAddPathToName = false);
		void DrawReplacementAnimations();
		void DrawReplacementAnimation(ReplacementAnimation* a_replacementAnimation);
		bool DrawConditionSet(Conditions::ConditionSet* a_conditionSet, SubMod* a_parentSubMod, ConditionEditMode a_editMode, Conditions::ConditionType a_conditionType, RE::TESObjectREFR* a_refrToEvaluate, bool a_bDrawLines, const ImVec2& a_drawStartPos);
		ImRect DrawCondition(std::unique_ptr<Conditions::ICondition>& a_condition, Conditions::ConditionSet* a_conditionSet, SubMod* a_parentSubMod, ConditionEditMode a_editMode, Conditions::ConditionType a_conditionType, RE::TESObjectREFR* a_refrToEvaluate, bool& a_bOutSetDirty);
		ImRect DrawBlankCondition(Conditions::ConditionSet* a_conditionSet, ConditionEditMode a_editMode, Conditions::ConditionType a_conditionType);
		bool DrawConditionPreset(ReplacerMod* a_replacerMod, Conditions::ConditionPreset* a_conditionPreset, bool& a_bOutWasPresetRenamed);

		void DrawConditionTooltip(const ConditionInfo& a_conditionInfo, ImGuiHoveredFlags a_flags = ImGuiHoveredFlags_DelayNormal) const;

		void UnloadedAnimationsWarning();

		// ImGui combo filter modified from https://github.com/khlorz/imgui-combo-filter/
		void CacheConditionInfos();
		static ComboFilterData* GetComboFilterData(ImGuiID a_comboId);
		static ComboFilterData* AddComboFilterData(ImGuiID a_comboId);
		static void ClearComboFilterData(ImGuiID a_comboId);
		bool ConditionComboFilter(const char* a_comboLabel, int& a_selectedItem, const std::vector<ConditionInfo>& a_conditionInfos, const ConditionInfo* a_currentConditionInfo, ImGuiComboFlags a_flags);

		static bool CanPreviewAnimation(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation);
		static bool IsPreviewingAnimation(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex = std::nullopt);
		static float GetPreviewButtonsWidth(const ReplacementAnimation* a_replacementAnimation, bool a_bIsPreviewing);
		static void DrawPreviewButtons(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, float a_previewButtonWidth, bool a_bCanPreview, bool a_bIsPreviewing, const ReplacementAnimation::Variant* a_variant = nullptr);

		static int ReferenceInputTextCallback(struct ImGuiInputTextCallbackData* a_data);

		ConditionEditMode _editMode = ConditionEditMode::kNone;
		float _firstColumnWidthPercent = 0.55f;

		bool _bShowSettings = false;

		// modified from imgui so it allows setting tooltip size
		static bool BeginDragDropSourceEx(ImGuiDragDropFlags a_flags = 0, ImVec2 a_tooltipSize = ImVec2(0, 0));

		// modified from imgui to draw a collapsed leaf node with no label
		bool TreeNodeCollapsedLeaf(const char* a_label, ImGuiTreeNodeFlags a_flags) const;
		bool TreeNodeCollapsedLeaf(const void* a_ptrId, ImGuiTreeNodeFlags a_flags, const char* a_label) const;
		bool TreeNodeCollapsedLeafBehavior(ImGuiID a_id, ImGuiTreeNodeFlags a_flags, const char* a_label) const;

		std::string _lastAddNewConditionName;

		std::vector<ConditionInfo>& GetConditionInfos(Conditions::ConditionType a_conditionType) { return a_conditionType == Conditions::ConditionType::kPreset ? _conditionInfosNoPRESET : _conditionInfos; }
		std::vector<ConditionInfo> _conditionInfos{};
		std::vector<ConditionInfo> _conditionInfosNoPRESET{};

		std::unique_ptr<Conditions::ICondition> _conditionCopy = nullptr;
		std::unique_ptr<Conditions::ConditionSet> _conditionSetCopy = nullptr;
		[[nodiscard]] bool ConditionContainsPreset(Conditions::ICondition* a_condition, Conditions::ConditionPreset* a_conditionPreset = nullptr) const;
		[[nodiscard]] bool ConditionSetContainsPreset(Conditions::ConditionSet* a_conditionSet, Conditions::ConditionPreset* a_conditionPreset = nullptr) const;
	};
}
