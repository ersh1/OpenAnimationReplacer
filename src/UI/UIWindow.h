#pragma once

#include "imgui.h"

namespace UI
{
	enum class WindowID : uint8_t
	{
		kMain = 0,
		kAnimationLog,
		kAnimationEventLog,
		kAnimationQueue,
		kErrorBanner,
		kWelcomeBanner,

		kMax
	};

	enum class WindowAlignment : uint8_t
	{
		kTopLeft = 0,
		kTopCenter,
		kTopRight,
		kCenterLeft,
		kCenter,
		kCenterRight,
		kBottomLeft,
		kBottomCenter,
		kBottomRight,
	};

	class UIWindow
	{
	public:
		UIWindow() = default;
		virtual ~UIWindow() noexcept = default;

		UIWindow(const UIWindow&) = delete;
		UIWindow(UIWindow&&) = delete;
		UIWindow& operator=(const UIWindow&) = delete;
		UIWindow& operator=(UIWindow&&) = delete;

		void TryDraw();

		bool ShouldDraw();

	protected:
		virtual bool ShouldDrawImpl() const = 0;
		virtual void DrawImpl() = 0;
		virtual void OnOpen(){};
		virtual void OnClose(){};

		void OnShouldDraw(bool a_bShouldDraw);
		void SetWindowDimensions(float a_offsetX = 0.f, float a_offsetY = 0.f, float a_sizeX = -1.f, float a_sizeY = -1.f, WindowAlignment a_alignment = WindowAlignment::kTopLeft, ImVec2 a_sizeMin = ImVec2(0, 0), ImVec2 a_sizeMax = ImVec2(0, 0), ImGuiCond_ a_cond = ImGuiCond_FirstUseEver);
		void ForceSetWidth(float a_width);

		bool _bLastShouldDraw = false;

	private:
		struct
		{
			ImVec2 sizeMin;
			ImVec2 sizeMax;
			ImVec2 pos;
			ImVec2 pivot;
			ImVec2 size;
		} _sizeData;
	};
}
