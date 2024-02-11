#include "UIAnimationLog.h"
#include "Settings.h"
#include "UICommon.h"
#include "UIManager.h"

namespace UI
{
	bool UIAnimationLog::ShouldDrawImpl() const
	{
		return Settings::bEnableAnimationLog;
	}

	void UIAnimationLog::DrawImpl()
	{
		SetWindowDimensions(Settings::fAnimationLogsOffsetX, Settings::fAnimationLogsOffsetY, Settings::fAnimationLogWidth, 0.f, WindowAlignment::kTopRight, ImVec2(Settings::fAnimationLogWidth, -1), ImVec2(Settings::fAnimationLogWidth, -1), ImGuiCond_Always);
		ForceSetWidth(Settings::fAnimationLogWidth);

		constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

		if (ImGui::Begin("Animation Log", nullptr, windowFlags)) {
			if (UIManager::GetSingleton().GetRefrToEvaluate() != nullptr) {
				auto& animationLog = AnimationLog::GetSingleton();
				if (!animationLog.IsAnimationLogEmpty()) {
					if (ImGui::BeginTable("AnimationLogTable", 1, ImGuiTableFlags_Borders)) {
						animationLog.ForEachAnimationLogEntry([&](AnimationLogEntry& a_logEntry) {
							DrawLogEntry(a_logEntry);
						});
						ImGui::EndTable();
					}
				} else {
					UICommon::TextUnformattedDisabled("No animation log entries");
				}
			} else {
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
				ImGui::TextWrapped("No reference selected. Type in a FormID in the main window, or select a reference in the console.");
				ImGui::PopStyleColor();
			}
		}
		ImGui::End();
	}

	void UIAnimationLog::OnOpen()
	{
		AnimationLog::GetSingleton().SetLogAnimations(true);
	}

	void UIAnimationLog::OnClose()
	{
		AnimationLog::GetSingleton().SetLogAnimations(false);
	}

	void UIAnimationLog::DrawLogEntry(AnimationLogEntry& a_logEntry) const
	{
		using Event = AnimationLogEntry::Event;
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();

		if (a_logEntry.timeDrawn < Settings::fAnimationLogEntryFadeTime) {
			const float alpha = std::lerp(0.f, 0.25f, std::fmax(Settings::fAnimationLogEntryFadeTime - a_logEntry.timeDrawn, 0.f) / Settings::fAnimationLogEntryFadeTime);
			const ImVec4 color(1.f, 1.f, 1.f, alpha);
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(color));
		}

		const ImGuiIO& io = ImGui::GetIO();
		a_logEntry.timeDrawn += io.DeltaTime;

		switch (a_logEntry.event) {
		case Event::kActivate:
			UICommon::TextUnformattedColored(UICommon::LOG_ACTIVATED_COLOR, "Activate");
			break;
		case Event::kActivateSynchronized:
			UICommon::TextUnformattedColored(UICommon::LOG_ACTIVATED_COLOR, "Activate");
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::LOG_SYNCHRONIZED_COLOR, "Paired");
			break;
		case Event::kEcho:
			UICommon::TextUnformattedColored(UICommon::LOG_ECHO_COLOR, "Echo");
			break;
		case Event::kLoop:
			UICommon::TextUnformattedColored(UICommon::LOG_LOOP_COLOR, "Loop");
			break;
		case Event::kActivateReplace:
			UICommon::TextUnformattedColored(UICommon::LOG_ACTIVATED_COLOR, "Activate");
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::LOG_REPLACED_COLOR, "[Replaced]");
			break;
		case Event::kActivateReplaceSynchronized:
			UICommon::TextUnformattedColored(UICommon::LOG_ACTIVATED_COLOR, "Activate");
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::LOG_SYNCHRONIZED_COLOR, "Paired");
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::LOG_REPLACED_COLOR, "[Replaced]");
			break;
		case Event::kEchoReplace:
			UICommon::TextUnformattedColored(UICommon::LOG_ECHO_COLOR, "Echo");
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::LOG_REPLACED_COLOR, "[Replaced]");
			break;
		case Event::kLoopReplace:
			UICommon::TextUnformattedColored(UICommon::LOG_LOOP_COLOR, "Loop");
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::LOG_REPLACED_COLOR, "[Replaced]");
			break;
		case Event::kInterrupt:
			UICommon::TextUnformattedColored(UICommon::LOG_INTERRUPTED_COLOR, "Interrupted");
			break;
		case Event::kPairedMismatch:
			UICommon::TextUnformattedColored(UICommon::LOG_INTERRUPTED_COLOR, "Interrupted");
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::LOG_SYNCHRONIZED_COLOR, "(Paired Mismatch)");
			break;
		}

		if (a_logEntry.bVariant) {
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::LOG_VARIANT_COLOR, "[Variant]");
		}

		if (a_logEntry.event < Event::kInterrupt && a_logEntry.bInterruptible) {
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::LOG_INTERRUPTED_COLOR, "(Interruptible)");
		}

		const std::string projectText = std::format("Project: {} ", a_logEntry.projectName);
		ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(projectText.data()).x);
		UICommon::TextUnformattedDisabled(projectText.data());

		const std::string clipText = std::format("Clip: {} ", a_logEntry.clipName);
		const float clipTextWidth = ImGui::CalcTextSize(clipText.data()).x;

		UICommon::TextUnformattedDisabled("Name:");
		ImGui::SameLine();
		UICommon::TextUnformattedEllipsis(a_logEntry.animationName.data(), nullptr, ImGui::GetContentRegionAvail().x - clipTextWidth);

		ImGui::SameLine(ImGui::GetContentRegionMax().x - clipTextWidth);
		UICommon::TextUnformattedDisabled(clipText.data());

		if (a_logEntry.bOriginal) {
			UICommon::TextUnformattedDisabled("Original animation");
		} else {
			UICommon::TextUnformattedDisabled("Mod:");
			ImGui::SameLine();
			ImGui::TextUnformatted(a_logEntry.modName.data());
			ImGui::SameLine();
			UICommon::TextUnformattedDisabled("Submod:");
			ImGui::SameLine();
			ImGui::TextUnformatted(a_logEntry.subModName.data());
			//ImGui::Text(std::format("{} / {}", a_logEntry.modName, a_logEntry.subModName).data());
		}

		float variantTextWidth = 0.f;
		if (a_logEntry.bVariant) {
			variantTextWidth = ImGui::CalcTextSize(a_logEntry.variantFilename.data()).x + ImGui::GetStyle().ItemSpacing.x;
		}

		UICommon::TextUnformattedDisabled("Path:");
		ImGui::SameLine();
		UICommon::TextUnformattedEllipsis(a_logEntry.animPath.data(), nullptr, ImGui::GetContentRegionAvail().x - variantTextWidth);

		if (a_logEntry.bVariant) {
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::LOG_VARIANT_COLOR, a_logEntry.variantFilename.data());
		}
	}
}
