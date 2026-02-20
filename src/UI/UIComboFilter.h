#pragma once

#include "API/OpenAnimationReplacer-ConditionTypes.h"
#include "API/OpenAnimationReplacer-FunctionTypes.h"
#include "OpenAnimationReplacer.h"
#include "UICommon.h"

// ImGui combo filter modified from https://github.com/khlorz/imgui-combo-filter/

namespace UI
{
	struct Info
	{
		Info(std::string_view a_name, std::string_view a_description, ImVec4 a_textColor = ImVec4(1.f, 1.f, 1.f, 1.f), std::string_view a_requiredPlugin = ""sv, std::string_view a_requiredPluginAuthor = ""sv, REL::Version a_requiredPluginVersion = { 0, 0, 0 }) :
			name(a_name), description(a_description), textColor(a_textColor), requiredPluginName(a_requiredPlugin), requiredPluginAuthor(a_requiredPluginAuthor), requiredVersion(a_requiredPluginVersion) {}

		Info(const Conditions::ICondition* a_condition) :
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

		Info(const Functions::IFunction* a_event) :
			name(a_event->GetName()),
			description(a_event->GetDescription()),
			requiredPluginName(a_event->GetRequiredPluginName()),
			requiredPluginAuthor(a_event->GetRequiredPluginAuthor()),
			requiredVersion(a_event->GetRequiredVersion())
		{
			switch (a_event->GetFunctionType()) {
			case Functions::FunctionType::kCustom:
				textColor = UICommon::CUSTOM_CONDITION_COLOR;
				break;
			case Functions::FunctionType::kNormal:
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

	struct FilteredInfo
	{
		FilteredInfo(int a_index, int a_score) :
			index(a_index),
			score(a_score)
		{}

		int index;
		int score;

		bool operator<(const FilteredInfo& a_other) const noexcept
		{
			return score < a_other.score;
		}
	};

	struct ComboFilterData
	{
		std::string filterString{};
		struct
		{
			const Info* info;
			int index;
		} initialValues{ nullptr, -1 };
		int currentSelection = -1;

		std::vector<FilteredInfo> filteredInfos{};
		bool filterStatus = false;

		bool SetNewValue(const Info* a_info, int a_newIndex) noexcept
		{
			currentSelection = a_newIndex;
			return SetNewValue(a_info);
		}

		bool SetNewValue(const Info* a_info) noexcept
		{
			if (filterStatus) {
				if (currentSelection >= 0) {
					currentSelection = filteredInfos[currentSelection].index;
				}
				filteredInfos.clear();
				filterStatus = false;
				filterString = ""sv;
			}

			const bool bRet = currentSelection != initialValues.index;
			if (bRet) {
				initialValues.info = a_info;
				initialValues.index = currentSelection;
			}

			return bRet;
		}

		void ResetToInitialValue() noexcept
		{
			filterStatus = false;
			filterString = ""sv;
			filteredInfos.clear();
			currentSelection = initialValues.index;
		}
	};

	struct ComboMapHasher
	{
		ImGuiID operator()(ImGuiID a_id) const noexcept
		{
			return a_id;
		}
	};

	template <typename T, typename Derived>
	class UIComboFilter
	{
	public:
		void CacheInfos() {}

		ComboFilterData* GetComboFilterData(ImGuiID a_comboId)
		{
			const auto it = _comboHashMap.find(a_comboId);
			return it == _comboHashMap.end() ? nullptr : it->second.get();
		}

		ComboFilterData* AddComboFilterData(ImGuiID a_comboId)
		{
			IM_ASSERT(!_comboHashMap.contains(a_comboId) && "A combo data currently exists on the same id!");
			auto& new_data = _comboHashMap[a_comboId];
			new_data.reset(new ComboFilterData());
			return new_data.get();
		}

		void ClearComboFilterData(ImGuiID a_comboId) { _comboHashMap.erase(a_comboId); }

		float CalcComboItemHeight(int a_itemCount, float a_offsetMultiplier)
		{
			const ImGuiContext* g = GImGui;
			return a_itemCount < 0 ? FLT_MAX : (g->FontSize + g->Style.ItemSpacing.y) * a_itemCount - g->Style.ItemSpacing.y + (g->Style.WindowPadding.y * a_offsetMultiplier);
		}

		bool ComboFilter(const char* a_comboLabel, int& a_selectedItem, const std::vector<Info>& a_infos, const Info* a_currentInfo, ImGuiComboFlags a_flags, std::function<void(const Info&, ImGuiHoveredFlags)> a_tooltipCallback)
		{
			using namespace ImGui;

			ImGuiContext* g = GImGui;
			ImGuiWindow* window = GetCurrentWindow();

			g->NextWindowData.ClearFlags();  // We behave like Begin() and need to consume those values
			if (window->SkipItems) {
				return false;
			}

			IM_ASSERT((a_flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) != (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview));  // Can't use both flags together

			const ImGuiStyle& style = g->Style;
			const ImGuiID combo_id = window->GetID(a_comboLabel);

			const float arrow_size = (a_flags & ImGuiComboFlags_NoArrowButton) ? 0.0f : GetFrameHeight();
			const ImVec2 label_size = CalcTextSize(a_comboLabel, NULL, true);
			const float expected_w = CalcItemWidth();
			const float w = (a_flags & ImGuiComboFlags_NoPreview) ? arrow_size : CalcItemWidth();
			const ImVec2 bb_max(window->DC.CursorPos.x + w, window->DC.CursorPos.y + (label_size.y + style.FramePadding.y * 2.0f));
			const ImRect bb(window->DC.CursorPos, bb_max);
			const ImVec2 total_bb_max(bb.Max.x + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), bb.Max.y);
			const ImRect total_bb(bb.Min, total_bb_max);
			ItemSize(total_bb, style.FramePadding.y);
			if (!ItemAdd(total_bb, combo_id, &bb)) {
				return false;
			}

			// Open on click
			bool hovered, held;
			bool pressed = ButtonBehavior(bb, combo_id, &hovered, &held);
			bool popup_open = IsPopupOpen(combo_id, ImGuiPopupFlags_None);
			bool popup_just_opened = false;
			if (pressed && !popup_open) {
				OpenPopupEx(combo_id, ImGuiPopupFlags_None);
				popup_open = true;
				popup_just_opened = true;
			}

			// Render shape
			const ImU32 frame_col = GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
			const float value_x2 = ImMax(bb.Min.x, bb.Max.x - arrow_size);
			RenderNavHighlight(bb, combo_id);
			if (!(a_flags & ImGuiComboFlags_NoPreview))
				window->DrawList->AddRectFilled(bb.Min, ImVec2(value_x2, bb.Max.y), frame_col, style.FrameRounding, (a_flags & ImGuiComboFlags_NoArrowButton) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
			if (!(a_flags & ImGuiComboFlags_NoArrowButton)) {
				ImU32 bg_col = GetColorU32((popup_open || hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
				ImU32 text_col = GetColorU32(ImGuiCol_Text);
				window->DrawList->AddRectFilled(ImVec2(value_x2, bb.Min.y), bb.Max, bg_col, style.FrameRounding, (w <= arrow_size) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersRight);
				if (value_x2 + arrow_size - style.FramePadding.x <= bb.Max.x)
					RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y), text_col, popup_open ? ImGuiDir_Up : ImGuiDir_Down, 1.0f);
			}
			RenderFrameBorder(bb.Min, bb.Max, style.FrameRounding);

			// Render preview and label
			if (a_currentInfo && !(a_flags & ImGuiComboFlags_NoPreview)) {
				const ImVec2 min_pos(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y);
				PushStyleColor(ImGuiCol_Text, a_currentInfo->textColor);
				RenderTextClipped(min_pos, ImVec2(value_x2, bb.Max.y), a_currentInfo->name.data(), NULL, NULL);
				PopStyleColor();
			}
			if (label_size.x > 0) {
				RenderText(ImVec2(bb.Max.x + style.ItemInnerSpacing.x, bb.Min.y + style.FramePadding.y), a_comboLabel);
			}

			if (!popup_open) {
				ClearComboFilterData(combo_id);
				return false;
			}

			ComboFilterData* comboData = GetComboFilterData(combo_id);
			if (!comboData) {
				comboData = AddComboFilterData(combo_id);
				comboData->currentSelection = a_selectedItem;
				comboData->initialValues.info = a_currentInfo;
				comboData->initialValues.index = a_selectedItem;
			}

			PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3.50f, 5.00f));
			int popup_item_count = -1;
			if (!(g->NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
				if ((a_flags & ImGuiComboFlags_HeightMask_) == 0)
					a_flags |= ImGuiComboFlags_HeightRegular;
				IM_ASSERT(ImIsPowerOfTwo(a_flags & ImGuiComboFlags_HeightMask_));  // Only one
				if (a_flags & ImGuiComboFlags_HeightRegular)
					popup_item_count = 8 + 1;
				else if (a_flags & ImGuiComboFlags_HeightSmall)
					popup_item_count = 4 + 1;
				else if (a_flags & ImGuiComboFlags_HeightLarge)
					popup_item_count = 20 + 1;
				const float popup_height = CalcComboItemHeight(popup_item_count, 5.0f);  // Increment popup_item_count to account for the InputText widget
				SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(expected_w, popup_height));
			}

			char name[16];
			ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g->BeginPopupStack.Size);  // Recycle windows based on depth

			if (ImGuiWindow* popup_window = FindWindowByName(name)) {
				if (popup_window->WasActive) {
					// Always override 'AutoPosLastDirection' to not leave a chance for a past value to affect us.
					ImVec2 size_expected = CalcWindowNextAutoFitSize(popup_window);
					popup_window->AutoPosLastDirection = (a_flags & ImGuiComboFlags_PopupAlignLeft) ? ImGuiDir_Left : ImGuiDir_Down;  // Left = "Below, Toward Left", Down = "Below, Toward Right (default)"
					ImRect r_outer = GetPopupAllowedExtentRect(popup_window);
					ImVec2 pos = FindBestWindowPosForPopupEx(bb.GetBL(), size_expected, &popup_window->AutoPosLastDirection, r_outer, bb, ImGuiPopupPositionPolicy_ComboBox);
					SetNextWindowPos(pos);
				}
			}

			constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
			if (!Begin(name, nullptr, window_flags)) {
				PopStyleVar();
				EndPopup();
				IM_ASSERT(0);  // This should never happen as we tested for IsPopupOpen() above
				return false;
			}

			if (popup_just_opened) {
				SetKeyboardFocusHere();
			}

			const float items_max_width = expected_w - (style.WindowPadding.x * 2.00f);
			PushItemWidth(items_max_width);
			PushStyleVar(ImGuiStyleVar_FrameRounding, 5.00f);
			PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor(240, 240, 240, 255));
			PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 0, 0, 255));
			const bool buffer_changed = InputText("##inputText", &comboData->filterString, ImGuiInputTextFlags_AutoSelectAll, nullptr, nullptr);
			PopStyleColor(2);
			PopStyleVar(1);
			PopItemWidth();

			bool selection_changed = false;
			const bool clicked_outside = !IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AnyWindow) && IsMouseClicked(0);

			auto getInfo = [&](int a_index) -> const Info& {
				const int index = comboData->filterStatus ? comboData->filteredInfos[a_index].index : a_index;
				return a_infos[index];
			};

			const int item_count = static_cast<int>(comboData->filterStatus ? comboData->filteredInfos.size() : a_infos.size());
			char listbox_name[16];
			ImFormatString(listbox_name, 16, "##lbn%u", combo_id);
			if (--popup_item_count > item_count || popup_item_count < 0) {
				popup_item_count = item_count;
			}
			if (BeginListBox(listbox_name, ImVec2(items_max_width, CalcComboItemHeight(popup_item_count, 1.50f)))) {
				ImGuiWindow* listbox_window = ImGui::GetCurrentWindow();
				listbox_window->Flags |= ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;

				if (listbox_window->Appearing) {
					UICommon::SetScrollToComboItemJump(listbox_window, comboData->initialValues.index);
				}

				ImGuiListClipper listclipper;
				listclipper.Begin(item_count);
				char select_item_id[128];
				while (listclipper.Step()) {
					for (int i = listclipper.DisplayStart; i < listclipper.DisplayEnd; ++i) {
						bool is_selected = i == comboData->currentSelection;
						const Info& info = getInfo(i);

						ImFormatString(select_item_id, 128, "%s##id%d", info.name.data(), i);
						PushStyleColor(ImGuiCol_Text, info.textColor);
						if (Selectable(select_item_id, is_selected)) {
							if (comboData->SetNewValue(&info, i)) {
								selection_changed = true;
								a_selectedItem = comboData->currentSelection;
							}
							CloseCurrentPopup();
						}
						PopStyleColor();
						a_tooltipCallback(info, ImGuiHoveredFlags_DelayNormal);
					}
				}

				if (clicked_outside || IsKeyPressed(ImGuiKey_Escape)) {
					comboData->ResetToInitialValue();
					CloseCurrentPopup();
				} else if (buffer_changed) {
					comboData->filteredInfos.clear();
					comboData->filterStatus = !comboData->filterString.empty();
					if (comboData->filterStatus) {
						constexpr int max_matches = 128;
						unsigned char matches[max_matches];
						int match_count;
						int score = 0;

						for (int i = 0; i < a_infos.size(); ++i) {
							auto& info = a_infos[i];
							if (UICommon::FuzzySearch(comboData->filterString.data(), info.name.data(), score, matches, max_matches, match_count)) {
								comboData->filteredInfos.emplace_back(i, score);
							}
						}
						std::sort(comboData->filteredInfos.rbegin(), comboData->filteredInfos.rend());
					}
					comboData->currentSelection = comboData->filteredInfos.empty() ? -1 : 0;
					SetScrollY(0.0f);
				} else if (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter)) {  // Automatically exit the combo popup on selection
					if (comboData->SetNewValue(comboData->currentSelection < 0 ? nullptr : &getInfo(comboData->currentSelection))) {
						selection_changed = true;
						a_selectedItem = comboData->currentSelection;
					}
					CloseCurrentPopup();
				}

				if (IsKeyPressed(ImGuiKey_UpArrow)) {
					if (comboData->currentSelection > 0) {
						UICommon::SetScrollToComboItemUp(listbox_window, --comboData->currentSelection);
					}
				} else if (IsKeyPressed(ImGuiKey_DownArrow)) {
					if (comboData->currentSelection >= -1 && comboData->currentSelection < item_count - 1) {
						UICommon::SetScrollToComboItemDown(listbox_window, ++comboData->currentSelection);
					}
				}

				EndListBox();
			}
			EndPopup();
			PopStyleVar();

			return selection_changed;
		}

	protected:
		std::unordered_map<ImGuiID, std::unique_ptr<ComboFilterData>, ComboMapHasher> _comboHashMap{};
	};

	class UIConditionComboFilter : public UIComboFilter<Conditions::ICondition, UIConditionComboFilter>
	{
	public:
		void CacheInfos()
		{
			_conditionInfos.clear();
			_conditionInfosNoPRESET.clear();

			OpenAnimationReplacer::GetSingleton().ForEachConditionFactory([&](std::string_view a_name, [[maybe_unused]] auto a_factory) {
				if (const auto tempCondition = OpenAnimationReplacer::GetSingleton().CreateCondition(a_name)) {
					const auto conditionType = tempCondition->GetConditionType();
					ImVec4 color;
					switch (conditionType) {
					case Conditions::ConditionType::kCustom:
						color = UICommon::CUSTOM_CONDITION_COLOR;
						break;
					case Conditions::ConditionType::kPreset:
						color = UICommon::CONDITION_PRESET_COLOR;
						break;
					case Conditions::ConditionType::kNormal:
					default:
						color = UICommon::DEFAULT_CONDITION_COLOR;
						break;
					}
					_conditionInfos.emplace_back(a_name, tempCondition->GetDescription(), color, tempCondition->GetRequiredPluginName(), tempCondition->GetRequiredPluginAuthor(), tempCondition->GetRequiredVersion());
					if (conditionType != Conditions::ConditionType::kPreset) {
						_conditionInfosNoPRESET.emplace_back(a_name, tempCondition->GetDescription(), color, tempCondition->GetRequiredPluginName(), tempCondition->GetRequiredPluginAuthor(), tempCondition->GetRequiredVersion());
					}
				}
			});
		}

		std::vector<Info>& GetConditionInfos(Conditions::ConditionType a_conditionType) { return a_conditionType == Conditions::ConditionType::kPreset ? _conditionInfosNoPRESET : _conditionInfos; }

	protected:
		std::vector<Info> _conditionInfos{};
		std::vector<Info> _conditionInfosNoPRESET{};
	};

	class UIFunctionComboFilter : public UIComboFilter<Functions::IFunction, UIFunctionComboFilter>
	{
	public:
		void CacheInfos()
		{
			_functionInfos.clear();

			OpenAnimationReplacer::GetSingleton().ForEachFunctionFactory([&](std::string_view a_name, [[maybe_unused]] auto a_factory) {
				if (const auto tempFunction = OpenAnimationReplacer::GetSingleton().CreateFunction(a_name)) {
					const auto eventType = tempFunction->GetFunctionType();
					ImVec4 color;
					switch (eventType) {
					case Functions::FunctionType::kCustom:
						color = UICommon::CUSTOM_CONDITION_COLOR;
						break;
					case Functions::FunctionType::kNormal:
					default:
						color = UICommon::DEFAULT_CONDITION_COLOR;
						break;
					}
					_functionInfos.emplace_back(a_name, tempFunction->GetDescription(), color, tempFunction->GetRequiredPluginName(), tempFunction->GetRequiredPluginAuthor(), tempFunction->GetRequiredVersion());
				}
			});
		}

		std::vector<Info>& GetFunctionInfos() { return _functionInfos; }

	protected:
		std::vector<Info> _functionInfos{};
	};
}
