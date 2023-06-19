#pragma once
#include "UIWindow.h"

#include <imgui_internal.h>
#include "OpenAnimationReplacer.h"

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

    protected:
        bool ShouldDrawImpl() const override;
        void DrawImpl() override;
        void OnOpen() override;
        void OnClose() override;

    private:
        void DrawSettings(const ImVec2& a_pos);
        void DrawMissingPlugins();
        void DrawConflictingSubMods() const;
        void DrawSubModsWithInvalidConditions() const;
        void DrawReplacerMods();
        void DrawReplacerMod(ReplacerMod* a_replacerMod, std::unordered_map<std::string, SubModNameFilterResult>& a_filterResults);
        void DrawSubMod(ReplacerMod* a_replacerMod, SubMod* a_subMod, bool a_bAddPathToName = false);
        void DrawReplacementAnimations();
        void DrawReplacementAnimation(const ReplacementAnimation* a_replacementAnimation);
        bool DrawConditionSet(Conditions::ConditionSet* a_conditionSet, ConditionEditMode a_editMode, bool a_bDrawLines, const ImVec2& a_drawStartPos);
        ImRect DrawCondition(std::unique_ptr<Conditions::ICondition>& a_condition, Conditions::ConditionSet* a_conditionSet, ConditionEditMode a_editMode, bool& a_bOutSetDirty);
        ImRect DrawBlankCondition(Conditions::ConditionSet* a_conditionSet, ConditionEditMode a_editMode);

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
        std::unique_ptr<Conditions::ICondition> _conditionCopy = nullptr;
        std::unique_ptr<Conditions::ConditionSet> _conditionSetCopy = nullptr;
    };
}
