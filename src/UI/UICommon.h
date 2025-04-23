#pragma once

#include "imgui.h"

#include <imgui_internal.h>

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
		constexpr ImVec4 LOG_VARIANT_COLOR(0.8f, 0.4f, 0.8f, 1.f);
		constexpr ImVec4 LOG_SYNCHRONIZED_COLOR(0.64f, 0.3f, 1.f, 1.f);
		constexpr ImVec4 EVENT_LOG_PAYLOAD_COLOR(0.5f, 0.7f, 1.f, 1.f);
		constexpr ImVec4 EVENT_LOG_TIME_COLOR_SHORT(0.4f, 0.8f, 0.4f, 1.f);
		constexpr ImVec4 EVENT_LOG_TIME_COLOR_MEDIUM(0.8f, 0.8f, 0.4f, 1.f);
		constexpr ImVec4 EVENT_LOG_TIME_COLOR_LONG(0.8f, 0.4f, 0.4f, 1.f);
		constexpr ImVec4 EVENT_LOG_TRIGGERED_TRANSITION_COLOR(0.5f, 0.7f, 1.f, 1.f);
		constexpr ImVec4 DEFAULT_CONDITION_COLOR(1.f, 1.f, 1.f, 1.f);
		constexpr ImVec4 CUSTOM_CONDITION_COLOR(0.5f, 0.7f, 1.f, 1.f);
		constexpr ImVec4 INVALID_CONDITION_COLOR(1.f, 0.3f, 0.3f, 1.f);
		constexpr ImVec4 CONDITION_PRESET_COLOR(0.f, 0.9f, 0.76f, 1.f);
		constexpr ImVec4 CONDITION_PRESET_BORDER_COLOR(0.f, 0.53f, 0.5f, 1.f);
		constexpr ImVec4 CONDITION_SHARED_STATE_BORDER_COLOR(0.45f, 0.45f, 0.22f, 1.00f);
		constexpr ImVec4 YELLOW_COLOR(1.f, 1.f, 0.f, 1.f);
		constexpr ImVec4 BLACK_COLOR(0.f, 0.f, 0.f, 1.f);

		void TextUnformattedColored(const ImVec4& a_col, const char* a_text, const char* a_textEnd = nullptr);
		void TextUnformattedDisabled(const char* a_text, const char* a_textEnd = nullptr);
		void TextUnformattedWrapped(const char* a_text, const char* a_textEnd = nullptr);
		bool TextUnformattedEllipsisNoTooltip(const char* a_text, const char* a_textEnd, float a_maxWidth);
		bool TextUnformattedEllipsis(const char* a_text, const char* a_textEnd = nullptr, float a_maxWidth = 0.f);
		bool TextUnformattedEllipsisShort(const char* a_fullText, const char* a_shortText, const char* a_shortTextEnd = nullptr, float a_maxWidth = 0.f);

		inline void AddTooltip(const char* a_desc, ImGuiHoveredFlags a_flags = ImGuiHoveredFlags_DelayNormal)
		{
			if (ImGui::IsItemHovered(a_flags)) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8, 8 });
				if (ImGui::BeginTooltip()) {
					ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
					ImGui::TextUnformatted(a_desc);
					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}
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
		void DrawWarningIcon();

		void ButtonWithConfirmationModal(std::string_view a_label, std::string_view a_confirmation, const std::function<void()>& a_func, const ImVec2& a_buttonSize = ImVec2(0, 0));

		// modified from imgui with added text
		bool ArrowButtonText(const char* a_label, ImGuiDir a_dir, bool a_bArrowOnRight, const ImVec2& a_sizeArg = ImVec2(0, 0), ImGuiButtonFlags a_flags = ImGuiButtonFlags_None);
		bool PopupToggleButton(const char* a_label, const char* a_popupId, const ImVec2& a_sizeArg = ImVec2(0, 0));

		// modified from imgui_stdlib with added support for a max length
		bool InputText(const char* a_label, std::string* a_str, int a_maxLength, ImGuiInputTextFlags a_flags = 0, ImGuiInputTextCallback a_callback = nullptr, void* a_userData = nullptr);
		bool InputTextMultiline(const char* a_label, std::string* a_str, int a_maxLength, const ImVec2& a_size = ImVec2(0, 0), ImGuiInputTextFlags a_flags = 0, ImGuiInputTextCallback a_callback = nullptr, void* a_userData = nullptr);
		bool InputTextWithHint(const char* a_label, const char* a_hint, std::string* a_str, int a_maxLength, ImGuiInputTextFlags a_flags = 0, ImGuiInputTextCallback a_callback = nullptr, void* a_userData = nullptr);

		// inspired by reshade's key input box
		std::string GetKeyName(uint32_t a_keycode);
		std::string GetKeyName(uint32_t a_key[4]);
		bool InputKey(const char* a_label, uint32_t a_key[4]);

		// modified from imgui with removed mouse cursor change
		void ScaleAllSizes(ImGuiStyle& a_style, float a_scaleFactor);

		// Adapted from https://github.com/khlorz/imgui-combo-filter/ which adapted it from https://github.com/forrestthewoods/lib_fts/blob/master/code/fts_fuzzy_match.h
		bool FuzzySearch(char const* a_pattern, char const* a_haystack, int& a_outScore, unsigned char a_matches[], int a_maxMatches, int& a_outMatches);
		bool FuzzySearchRecursive(const char* a_pattern, const char* a_src, int& a_outScore, const char* a_strBegin, const unsigned char a_srcMatches[], unsigned char a_newMatches[], int a_maxMatches, int& a_nextMatch, int& a_recursionCount, int a_recursionLimit);

		void SetScrollToComboItemJump(ImGuiWindow* a_listbox_window, int a_index);
		void SetScrollToComboItemUp(ImGuiWindow* a_listbox_window, int a_index);
		void SetScrollToComboItemDown(ImGuiWindow* a_listbox_window, int a_index);
	}
}
