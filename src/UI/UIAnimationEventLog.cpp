#include "UIAnimationEventLog.h"

#include "Settings.h"
#include "UICommon.h"
#include "UIManager.h"

#include "Utils.h"
#include "imgui_stdlib.h"

namespace UI
{
	bool UIAnimationEventLog::ShouldDrawImpl() const
	{
		return Settings::bEnableAnimationEventLog;
	}

	void UIAnimationEventLog::DrawImpl()
	{
		const float offsetX = Settings::bEnableAnimationLog ? Settings::fAnimationLogWidth + ImGui::GetStyle().FramePadding.x : 0.f;
		SetWindowDimensions(Settings::fAnimationLogsOffsetX + offsetX, Settings::fAnimationLogsOffsetY, Settings::fAnimationEventLogWidth, 0.f, WindowAlignment::kTopRight, ImVec2(Settings::fAnimationEventLogWidth, -1), ImVec2(Settings::fAnimationEventLogWidth, -1), ImGuiCond_Always);
		ForceSetWidth(Settings::fAnimationEventLogWidth);

		constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

		if (ImGui::Begin("Animation Event Log", nullptr, windowFlags)) {
			auto& animationEventLog = AnimationEventLog::GetSingleton();

			if (!animationEventLog.IsLogEmpty()) {
				// draw log entries
				if (ImGui::BeginTable("AnimationLogTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0.f, Settings::fAnimationEventLogHeight))) {
					ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 0.2f);
					ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch, 0.8f);
					const auto timeWidth = ImGui::CalcTextSize("+0.000").x;
					ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, timeWidth);
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableHeadersRow();

					// draw log entries
					uint32_t lastTimestamp = 0;

					ImGuiListClipper clipper;
					clipper.Begin(animationEventLog.GetLogEntryCount());
					while (clipper.Step()) {
						animationEventLog.ForEachLogEntry([&](AnimationEventLogEntry* a_logEntry) {
							DrawLogEntry(a_logEntry, lastTimestamp);
							lastTimestamp = a_logEntry->timestamp;
						},
							Settings::bAnimationLogOrderDescending, clipper.DisplayStart, clipper.DisplayEnd);
					}
					clipper.End();

					// scroll to the new event if there is one
					if (animationEventLog.HasNewEvent()) {
						if (Settings::bAnimationLogOrderDescending) {
							ImGui::SetScrollHereY(1.f);
						} else {
							ImGui::SetScrollY(0.f);
						}
					}

					ImGui::EndTable();
				}

				if (IsInteractable()) {
					DrawFilterPanel();
				}

			} else {
				UICommon::TextUnformattedDisabled("No animation event log entries");
			}

			if (IsInteractable()) {
				ImGui::Separator();
				DrawEventSourcesPanel();
			}

			animationEventLog.OnPostDisplay();
		}
		ImGui::End();
	}

	bool UIAnimationEventLog::IsInteractable() const
	{
		// draw control panel only when the main UI is open
		return UIManager::GetSingleton().bShowMain;
	}

	void UIAnimationEventLog::DrawFilterPanel() const
	{
		auto& animationEventLog = AnimationEventLog::GetSingleton();

		// filtering
		const auto& style = ImGui::GetStyle();
		const float orderButtonWidth = (ImGui::CalcTextSize("Order:").x + ImGui::GetFontSize() + style.FramePadding.x * 2);
		const std::string clearButtonName = "Clear Log";
		const float clearButtonWidth = (ImGui::CalcTextSize(clearButtonName.data()).x + style.FramePadding.x * 2);
		const float helpMarkerWidth = ImGui::CalcTextSize("(?)").x + style.ItemSpacing.x * 2;
		const float filterWidth = (ImGui::GetContentRegionAvail().x - style.FramePadding.x * 2 - helpMarkerWidth * 2 - orderButtonWidth - clearButtonWidth);

		ImGui::SetNextItemWidth(filterWidth);
		if (ImGui::InputTextWithHint("##filter", "Filter...", &animationEventLog.filter)) {
			animationEventLog.RefreshFilter();
		}
		ImGui::SameLine();
		UICommon::HelpMarker("Type a part of the event name / source name / payload to filter the log results. You can use regex.");

		ImGui::SameLine(ImGui::GetContentRegionMax().x - orderButtonWidth - clearButtonWidth - style.ItemSpacing.x);
		if (UICommon::ArrowButtonText("Order:", Settings::bAnimationLogOrderDescending ? ImGuiDir_Down : ImGuiDir_Up, true)) {
			Settings::bAnimationLogOrderDescending = !Settings::bAnimationLogOrderDescending;
			Settings::WriteSettings();
			animationEventLog.ForceHasNewEvent();
		}
		UICommon::AddTooltip("Click to change the log list order.");

		ImGui::SameLine(ImGui::GetContentRegionMax().x - clearButtonWidth);
		if (ImGui::Button(clearButtonName.data())) {
			animationEventLog.ClearLog();
		}
	}

	void UIAnimationEventLog::DrawEventSourcesPanel() const
	{
		auto& animationEventLog = AnimationEventLog::GetSingleton();

		// draw list of event sources
		ImGui::TextUnformatted("Event Sources");

		if (animationEventLog.HasEventSources()) {
			std::vector<RE::ObjectRefHandle> eventSourcesToRemove;
			animationEventLog.ForEachEventSource([&](const RE::ObjectRefHandle& a_handle) {
				if (!DrawEventSource(a_handle)) {
					eventSourcesToRemove.push_back(a_handle);
				}
			});

			for (const auto& handle : eventSourcesToRemove) {
				animationEventLog.RemoveEventSource(handle);
			}
		} else {
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
			ImGui::TextWrapped("No animation event sources added. Type in a FormID or select a reference in the console, then click Add Event Source.");
			ImGui::PopStyleColor();
		}

		ImGui::Spacing();

		// draw event source picker
		static char formIDBuf[9] = "";
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);

		RE::ObjectRefHandle selectedRefr{};
		if (const auto consoleRefr = Utils::GetConsoleRefr()) {
			selectedRefr = consoleRefr.get();
			ImGui::BeginDisabled();
			std::string formID = std::format("{:08X}", consoleRefr->GetFormID());
			ImGui::InputText("Event Source", formID.data(), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			ImGui::EndDisabled();
		} else {
			ImGui::InputTextWithHint("Event Source", "FormID...", formIDBuf, IM_ARRAYSIZE(formIDBuf), ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase, &EventSourceInputTextCallback);
			selectedRefr = _selectedRefr;
		}
		ImGui::SameLine();
		if (!selectedRefr) {
			ImGui::BeginDisabled();
			ImGui::Button("Add Event Source");
			ImGui::EndDisabled();
		} else {
			if (animationEventLog.HasEventSource(selectedRefr)) {
				if (ImGui::Button("Remove Event Source")) {
					animationEventLog.RemoveEventSource(selectedRefr);
				}
			} else {
				if (ImGui::Button("Add Event Source")) {
					animationEventLog.AddEventSource(selectedRefr);
				}
			}
		}
	}

	bool UIAnimationEventLog::DrawEventSource(const RE::ObjectRefHandle& a_handle) const
	{
		if (const auto ref = a_handle.get()) {
			std::string name = ref->GetDisplayFullName();
			if (name.empty()) {
				name = ref->GetName();
				if (name.empty()) {
					name = "Unknown";
				}
			}

			ImGui::AlignTextToFramePadding();
			UICommon::TextUnformattedDisabled(name.data());

			ImGui::SameLine();
			const std::string checkboxLabel = std::format("Log Notify Graph##{}", a_handle.native_handle());
			auto& animationEventLog = AnimationEventLog::GetSingleton();
			bool tempBool = animationEventLog.GetLogNotifies(a_handle);
			if (ImGui::Checkbox(checkboxLabel.data(), &tempBool)) {
				animationEventLog.SetLogNotifies(a_handle, tempBool);
			}
			UICommon::AddTooltip("Also log events sent from code or Papyrus by NotifyAnimationGraph / SendAnimationEvent etc.");

			ImGui::SameLine();
			const std::string buttonLabel = std::format("Remove##{}", a_handle.native_handle());
			if (ImGui::Button(buttonLabel.data())) {
				return false;
			}
		} else {
			return false;
		}

		return true;
	}

	void UIAnimationEventLog::DrawLogEntry(AnimationEventLogEntry* a_logEntry, uint32_t a_lastTimestamp) const
	{
		ImGui::TableNextRow();
		ImGui::AlignTextToFramePadding();

		if (a_logEntry->timeDrawn < Settings::fAnimationLogEntryFadeTime) {
			const float alpha = std::lerp(0.f, 0.25f, std::fmax(Settings::fAnimationLogEntryFadeTime - a_logEntry->timeDrawn, 0.f) / Settings::fAnimationLogEntryFadeTime);
			const ImVec4 color(1.f, 1.f, 1.f, alpha);
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(color));
		}

		const ImGuiIO& io = ImGui::GetIO();
		a_logEntry->timeDrawn += io.DeltaTime;

		// name
		ImGui::TableSetColumnIndex(0);
		UICommon::TextUnformattedEllipsis(a_logEntry->holderName.data());

		// event
		ImGui::TableSetColumnIndex(1);

		if (a_logEntry->bFromNotify) {
			if (a_logEntry->bTriggeredTransition) {
				UICommon::TextUnformattedColored(UICommon::EVENT_LOG_TRIGGERED_TRANSITION_COLOR, "[Notify]");
			} else {
				UICommon::TextUnformattedDisabled("[Notify]");
			}

			ImGui::SameLine();
		}

		ImGui::TextUnformatted(a_logEntry->tag.data());
		if (a_logEntry->payload.length() > 0) {
			ImGui::SameLine(0.f, 0.f);
			ImGui::PushStyleColor(ImGuiCol_Text, UICommon::EVENT_LOG_PAYLOAD_COLOR);
			ImGui::TextUnformatted(".");
			ImGui::SameLine(0.f, 0.f);
			UICommon::TextUnformattedEllipsis(a_logEntry->payload.data());
			ImGui::PopStyleColor();
		}

		// time
		ImGui::TableSetColumnIndex(2);

		float secondsSinceLastEvent = 0.f;
		if (a_lastTimestamp != 0) {
			if (Settings::bAnimationLogOrderDescending) {
				secondsSinceLastEvent = static_cast<float>(a_logEntry->timestamp - a_lastTimestamp) / 1000.f;
			} else {
				secondsSinceLastEvent = static_cast<float>(a_lastTimestamp - a_logEntry->timestamp) / 1000.f;
			}
		}

		ImVec4 interpColor;
		const float halfTime = Settings::fAnimationEventLogEntryColorTimeLong * 0.5f;
		if (secondsSinceLastEvent < halfTime) {
			const float alpha = secondsSinceLastEvent * 2.f / Settings::fAnimationEventLogEntryColorTimeLong;
			interpColor = ImLerp(UICommon::EVENT_LOG_TIME_COLOR_SHORT, UICommon::EVENT_LOG_TIME_COLOR_MEDIUM, std::min(alpha, 1.f));
		} else {
			const float alpha = (secondsSinceLastEvent - halfTime) * 2.f / Settings::fAnimationEventLogEntryColorTimeLong;
			interpColor = ImLerp(UICommon::EVENT_LOG_TIME_COLOR_MEDIUM, UICommon::EVENT_LOG_TIME_COLOR_LONG, std::min(alpha, 1.f));
		}

		std::string sign = Settings::bAnimationLogOrderDescending ? "+" : "-";
		const std::string timeString = std::format("{}{:0.3f}", sign, secondsSinceLastEvent);
		UICommon::TextUnformattedColored(interpColor, timeString.data());
	}

	int UIAnimationEventLog::EventSourceInputTextCallback(ImGuiInputTextCallbackData* a_data)
	{
		RE::FormID formID;
		auto [ptr, ec]{ std::from_chars(a_data->Buf, a_data->Buf + a_data->BufTextLen, formID, 16) };
		if (ec == std::errc()) {
			_selectedRefr = RE::TESForm::LookupByID<RE::TESObjectREFR>(formID);
		} else {
			_selectedRefr = nullptr;
		}

		return 0;
	}
}
