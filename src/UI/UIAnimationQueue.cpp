#include "UIAnimationQueue.h"
#include "Settings.h"

namespace UI
{
	bool UIAnimationQueue::ShouldDrawImpl() const
	{
		if (Settings::bEnableAnimationQueueProgressBar && !Settings::bDisablePreloading) {
			if (const auto animationFileManagerSingleton = RE::AnimationFileManagerSingleton::GetSingleton()) {
				if (_fLingerTime > 0.f || animationFileManagerSingleton->queuedAnimations.size() > Settings::uQueueMinSize) {
					return true;
				}
			}
		}

		return false;
	}

	void UIAnimationQueue::DrawImpl()
	{
		const auto animationFileManager = RE::AnimationFileManagerSingleton::GetSingleton();
		const uint32_t queuedCount = animationFileManager->queuedAnimations.size();

		if (queuedCount == 0) {
			const ImGuiIO& io = ImGui::GetIO();
			_fLingerTime -= io.DeltaTime;
		} else {
			_fLingerTime = Settings::fAnimationQueueLingerTime + Settings::fQueueFadeTime;
		}

		uint32_t loadedCount = animationFileManager->loadedAnimations.size();
		const float queuePercent = static_cast<float>(loadedCount) / static_cast<float>(queuedCount + loadedCount);
		const std::string queuePercentStr = std::format("{}/{}", loadedCount, queuedCount + loadedCount);

		constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

		constexpr float PAD = 10.0f;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const ImVec2 workPos = viewport->WorkPos;
		const ImVec2 workSize = viewport->WorkSize;
		ImVec2 windowPos, windowPosPivot;
		windowPos.x = workPos.x + workSize.x - PAD;
		windowPos.y = workPos.y + PAD;
		windowPosPivot.x = 1.0f;
		windowPosPivot.y = 0.0f;
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);

		const float alpha = std::fmin(_fLingerTime / Settings::fQueueFadeTime, 1.f);
		ImGui::SetNextWindowBgAlpha(0.25f);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
		if (ImGui::Begin("AnimationQueue", nullptr, windowFlags)) {
			static constexpr auto TEXT = "Loading animations..."sv;
			const auto windowWidth = ImGui::GetWindowSize().x;
			const auto titleTextWidth = ImGui::CalcTextSize(TEXT.data()).x;
			ImGui::SetCursorPosX((windowWidth - titleTextWidth) * 0.5f);
			ImGui::TextUnformatted(TEXT.data());
			ImGui::ProgressBar(queuePercent, ImVec2(200, 0), queuePercentStr.data());
		}
		ImGui::PopStyleVar();
		ImGui::End();
	}
}
