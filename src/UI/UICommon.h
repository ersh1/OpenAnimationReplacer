#pragma once

#include "imgui.h"

namespace UI
{
    enum class ConditionEvaluateResult : uint8_t
    {
        kSuccess,
        kFailure,
        kRandom
    };

    namespace UICommon
    {
        constexpr ImVec4 SUCCESS_COLOR(0.2f, 0.8f, 0.2f, 1.f);
        constexpr ImVec4 SUCCESS_BG_COLOR(0.2f, 0.8f, 0.2f, 0.35f);
        constexpr ImVec4 FAIL_COLOR(0.8f, 0.2f, 0.2f, 1.f);
        constexpr ImVec4 UNKNOWN_COLOR(0.8f, 0.8f, 0.f, 1.f);
        constexpr ImVec4 TREE_LINE_COLOR(0.5f, 0.5f, 0.5f, 1.f);
        constexpr ImVec4 ERROR_BUTTON_COLOR(0.8f, 0.1f, 0.1f, 0.75f);
        constexpr ImVec4 ERROR_BUTTON_HOVERED_COLOR(0.8f, 0.1f, 0.1f, 1.f);
        constexpr ImVec4 ERROR_BUTTON_ACTIVE_COLOR(0.6f, 0.07f, 0.07f, 1.f);
        constexpr ImVec4 ERROR_TEXT_COLOR(1.f, 0.3f, 0.3f, 1.f);
        constexpr ImVec4 KEY_TEXT_COLOR(1.f, 0.6f, 0.2f, 1.f);
        constexpr ImVec4 WARNING_BUTTON_COLOR(0.8f, 0.3f, 0.1f, 0.5f);
        constexpr ImVec4 WARNING_BUTTON_HOVERED_COLOR(0.8f, 0.3f, 0.1f, 1.f);
        constexpr ImVec4 WARNING_BUTTON_ACTIVE_COLOR(0.7f, 0.25f, 0.08f, 1.f);
        constexpr ImVec4 WARNING_TEXT_COLOR(1.f, 0.5f, 0.1f, 1.f);
        constexpr ImVec4 USER_MOD_COLOR(1.f, 0.9f, 0.4f, 1.f);
        constexpr ImVec4 DIRTY_COLOR(1.f, 0.5f, 0.2f, 1.f);
        constexpr ImVec4 LOG_ACTIVATED_COLOR(0.2f, 0.8f, 0.2f, 1.f);
        constexpr ImVec4 LOG_ECHO_COLOR(0.2f, 0.5f, 0.8f, 1.f);
        constexpr ImVec4 LOG_LOOP_COLOR(0.4f, 0.7f, 1.f, 1.f);
        constexpr ImVec4 LOG_REPLACED_COLOR(0.8f, 0.8f, 0.f, 1.f);
        constexpr ImVec4 LOG_INTERRUPTED_COLOR(0.8f, 0.5f, 0.25f, 1.f);
        constexpr ImVec4 CUSTOM_CONDITION_COLOR(0.5f, 0.7f, 1.f, 1.f);
        constexpr ImVec4 INVALID_CONDITION_COLOR(1.f, 0.3f, 0.3f, 1.f);

        void TextUnformattedColored(const ImVec4& a_col, const char* a_text, const char* a_textEnd = nullptr);
        void TextUnformattedDisabled(const char* a_text, const char* a_textEnd = nullptr);

        inline void AddTooltip(const char* a_desc, ImGuiHoveredFlags a_flags = ImGuiHoveredFlags_DelayNormal)
        {
            if (ImGui::IsItemHovered(a_flags)) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8, 8 });
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
                ImGui::TextUnformatted(a_desc);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                ImGui::PopStyleVar();
            }
        }

        inline void HelpMarker(const char* a_desc)
        {
            ImGui::AlignTextToFramePadding();
            TextUnformattedDisabled("(?)");
            AddTooltip(a_desc, ImGuiHoveredFlags_DelayShort);
        }

        inline void TextDescriptionRightAligned(const char* a_description)
        {
            const auto savedCursorPos = ImGui::GetCursorPos();
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(a_description).x - ImGui::GetStyle().FramePadding.x);
            TextUnformattedDisabled(a_description);
            ImGui::SetCursorPos(savedCursorPos);
        }

        inline void SecondColumn(float a_percent = 0.5f)
        {
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x * a_percent);
        }

        inline float FirstColumnWidth(float a_firstColumnWidthPercent)
        {
            return ImGui::GetContentRegionAvail().x - (ImGui::GetContentRegionMax().x * (1.f - a_firstColumnWidthPercent)) - ImGui::GetStyle().ItemSpacing.x;
        }

        void DrawConditionEvaluateResult(ConditionEvaluateResult a_result);

        void ButtonWithConfirmationModal(std::string_view a_label, std::string_view a_confirmation, const std::function<void()>& a_func, const ImVec2& a_buttonSize = ImVec2(0, 0));

        // modified from imgui with added text
        bool ArrowButtonText(const char* a_label, ImGuiDir a_dir, const ImVec2& a_sizeArg = ImVec2(0, 0), ImGuiButtonFlags a_flags = ImGuiButtonFlags_None);
        bool PopupToggleButton(const char* a_label, const char* a_popupId, const ImVec2& a_sizeArg = ImVec2(0, 0));

        // modified from imgui_stdlib with added support for a max length
        bool InputText(const char* a_label, std::string* a_str, int a_maxLength, ImGuiInputTextFlags a_flags = 0, ImGuiInputTextCallback a_callback = nullptr, void* a_userData = nullptr);
        bool InputTextMultiline(const char* a_label, std::string* a_str, int a_maxLength, const ImVec2& a_size = ImVec2(0, 0), ImGuiInputTextFlags a_flags = 0, ImGuiInputTextCallback a_callback = nullptr, void* a_userData = nullptr);
        bool InputTextWithHint(const char* a_label, const char* a_hint, std::string* a_str, int a_maxLength, ImGuiInputTextFlags a_flags = 0, ImGuiInputTextCallback a_callback = nullptr, void* a_userData = nullptr);

        // inspired by reshade's key input box
        std::string GetKeyName(uint32_t a_keycode);
        std::string GetKeyName(uint32_t a_key[4]);
        bool InputKey(const char* a_label, uint32_t a_key[4]);
    }
}
