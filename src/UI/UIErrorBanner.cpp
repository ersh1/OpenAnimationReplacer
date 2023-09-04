#include "UIErrorBanner.h"

#include "UICommon.h"
#include "UIManager.h"

namespace UI
{
	bool UIErrorBanner::ShouldDrawImpl() const
	{
		return UIManager::GetSingleton().bShowErrorBanner;
	}

	void UIErrorBanner::DrawImpl()
	{
		SetWindowDimensions(0.f, 250.f, 0.f, 0.f, WindowAlignment::kTopCenter, ImVec2(1.f, 1.f), ImVec2(0.f, 0.f), ImGuiCond_Always);

		constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

		if (ImGui::Begin("Open Animation Replacer##Error", nullptr, windowFlags)) {
			constexpr auto titleText = "Open Animation Replacer"sv;
			constexpr auto errorTextA = "Major issue detected! Open the Open Animation Replacer menu by pressing"sv;
			const auto keyNameText = UICommon::GetKeyName(Settings::uToggleUIKeyData);
			constexpr auto errorTextB = "and click the bar at the bottom for more information."sv;
			const auto windowWidth = ImGui::GetWindowSize().x;
			const auto titleTextWidth = ImGui::CalcTextSize(titleText.data()).x;
			ImGui::SetCursorPosX((windowWidth - titleTextWidth) * 0.5f);
			ImGui::TextUnformatted(titleText.data());
			ImGui::Separator();

			UICommon::TextUnformattedColored(UICommon::ERROR_TEXT_COLOR, errorTextA.data());
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::KEY_TEXT_COLOR, keyNameText.data());
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::ERROR_TEXT_COLOR, errorTextB.data());
		}
		ImGui::End();
	}
}
