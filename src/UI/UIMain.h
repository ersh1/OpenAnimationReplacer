#pragma once
#include "UIWindow.h"
#include "UIComboFilter.h"

#include "OpenAnimationReplacer.h"
#include <imgui_internal.h>

namespace UI
{
	class UIMain : public UIWindow
	{
	public:
		// Drag & Drop payload structs
		struct DragConditionPayload
		{
			DragConditionPayload(std::unique_ptr<Conditions::ICondition>& a_condition, Conditions::ConditionSet* a_conditionSet) :
				condition(a_condition), conditionSet(a_conditionSet) {}

			std::unique_ptr<Conditions::ICondition>& condition;
			Conditions::ConditionSet* conditionSet;
		};

		struct DragFunctionPayload
		{
			DragFunctionPayload(std::unique_ptr<Functions::IFunction>& a_function, Functions::FunctionSet* a_functionSet) :
				function(a_function), functionSet(a_functionSet) {}

			std::unique_ptr<Functions::IFunction>& function;
			Functions::FunctionSet* functionSet;
		};

		struct SubModNameFilterResult
		{
			bool bDisplay = true;
			bool bDuplicateName = false;
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
		bool DrawConditionSet(Conditions::ConditionSet* a_conditionSet, SubMod* a_parentSubMod, EditMode a_editMode, Conditions::ConditionType a_conditionType, RE::TESObjectREFR* a_refrToEvaluate, bool a_bDrawLines, const ImVec2& a_drawStartPos);
		bool DrawFunctionSet(Functions::FunctionSet* a_functionSet, SubMod* a_parentSubMod, EditMode a_editMode, Functions::FunctionSetType a_functionSetType, RE::TESObjectREFR* a_refrToEvaluate, bool a_bDrawLines, const ImVec2& a_drawStartPos);
		ImRect DrawCondition(std::unique_ptr<Conditions::ICondition>& a_condition, Conditions::ConditionSet* a_conditionSet, SubMod* a_parentSubMod, EditMode a_editMode, Conditions::ConditionType a_conditionType, RE::TESObjectREFR* a_refrToEvaluate, bool& a_bOutSetDirty);
		ImRect DrawFunction(std::unique_ptr<Functions::IFunction>& a_function, Functions::FunctionSet* a_functionSet, SubMod* a_parentSubMod, EditMode a_editMode, Functions::FunctionSetType a_functionSetType, RE::TESObjectREFR* a_refrToEvaluate, bool& a_bOutSetDirty);
		ImRect DrawBlankCondition(Conditions::ConditionSet* a_conditionSet, EditMode a_editMode, Conditions::ConditionType a_conditionType);
		ImRect DrawBlankFunction(Functions::FunctionSet* a_functionSet, SubMod* a_parentSubMod, EditMode a_editMode, Functions::FunctionSetType a_functionSetType);
		bool DrawConditionPreset(ReplacerMod* a_replacerMod, Conditions::ConditionPreset* a_conditionPreset, bool& a_bOutWasPresetRenamed);

		static void DrawInfoTooltip(const Info& a_info, ImGuiHoveredFlags a_flags = ImGuiHoveredFlags_DelayNormal);

		void UnloadedAnimationsWarning();

		static bool CanPreviewAnimation(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation);
		static bool IsPreviewingAnimation(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex = std::nullopt);
		static float GetPreviewButtonsWidth(const ReplacementAnimation* a_replacementAnimation, bool a_bIsPreviewing);
		static void DrawPreviewButtons(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, float a_previewButtonWidth, bool a_bCanPreview, bool a_bIsPreviewing, Variant* a_variant = nullptr);

		static int ReferenceInputTextCallback(struct ImGuiInputTextCallbackData* a_data);

		EditMode _editMode = EditMode::kNone;
		float _firstColumnWidthPercent = 0.55f;

		bool _bShowSettings = false;

		// modified from imgui so it allows setting tooltip size
		static bool BeginDragDropSourceEx(ImGuiDragDropFlags a_flags = 0, ImVec2 a_tooltipSize = ImVec2(0, 0));

		std::string _lastAddNewConditionName;
		std::string _lastAddNewFunctionName;

		UI::UIConditionComboFilter _conditionComboFilter;
		UI::UIFunctionComboFilter _functionComboFilter;

		std::unique_ptr<Conditions::ICondition> _conditionCopy = nullptr;
		std::unique_ptr<Functions::IFunction> _functionCopy = nullptr;
		std::unique_ptr<Conditions::ConditionSet> _conditionSetCopy = nullptr;
		std::unique_ptr<Functions::FunctionSet> _functionSetCopy = nullptr;
		[[nodiscard]] bool ConditionContainsPreset(Conditions::ICondition* a_condition, Conditions::ConditionPreset* a_conditionPreset = nullptr) const;
		[[nodiscard]] bool ConditionSetContainsPreset(Conditions::ConditionSet* a_conditionSet, Conditions::ConditionPreset* a_conditionPreset = nullptr) const;
	};
}
