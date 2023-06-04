#include "UIWelcomeBanner.h"

#include "UICommon.h"
#include "Settings.h"

namespace UI
{
    void UIWelcomeBanner::Display()
    {
        _fLingerTime = DISPLAY_TIME;
        _bFirstFrame = true;
    }

    bool UIWelcomeBanner::ShouldDrawImpl() const
    {
        if (Settings::bShowWelcomeBanner) {
            return _fLingerTime > 0.f;
        }

        return false;
    }

    void UIWelcomeBanner::DrawImpl()
    {
        SetWindowDimensions(0.f, 200.f, 0.f, 0.f, WindowAlignment::kTopCenter, ImVec2(1.f, 1.f), ImVec2(0.f, 0.f), ImGuiCond_Always);

        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        float alpha = 1.f;
        if (_fLingerTime < Settings::fWelcomeBannerFadeTime) {
            alpha = std::lerp(0.f, 1.f, _fLingerTime / Settings::fWelcomeBannerFadeTime);
        } else if (_fLingerTime > DISPLAY_TIME - Settings::fWelcomeBannerFadeTime) {
            alpha = std::lerp(0.f, 1.f, (DISPLAY_TIME - _fLingerTime) / Settings::fWelcomeBannerFadeTime);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        if (ImGui::Begin("Open Animation Replacer##Welcome", nullptr, windowFlags)) {
            const auto titleText = std::format("Open Animation Replacer {}.{}.{}", Plugin::VERSION.major(), Plugin::VERSION.minor(), Plugin::VERSION.patch());
            constexpr auto textA = "Press"sv;
            const auto keyNameText = UICommon::GetKeyName(Settings::uToggleUIKeyData);
            constexpr auto textB = "to open the in-game UI."sv;
            const auto windowWidth = ImGui::GetWindowSize().x;
            const auto titleTextWidth = ImGui::CalcTextSize(titleText.data()).x;
            ImGui::SetCursorPosX((windowWidth - titleTextWidth) * 0.5f);
            ImGui::TextUnformatted(titleText.data());
            ImGui::Separator();

            ImGui::TextUnformatted(textA.data());
            ImGui::SameLine();
            UICommon::TextUnformattedColored(UICommon::KEY_TEXT_COLOR, keyNameText.data());
            ImGui::SameLine();
            ImGui::TextUnformatted(textB.data());
        }
        ImGui::PopStyleVar();
        ImGui::End();

        if (_bFirstFrame) {
            _bFirstFrame = false;
        } else {
            const ImGuiIO& io = ImGui::GetIO();
            _fLingerTime -= io.DeltaTime;
        }
    }
}
