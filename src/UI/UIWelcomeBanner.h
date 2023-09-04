#pragma once
#include "UIWindow.h"

namespace UI
{
	class UIWelcomeBanner : public UIWindow
	{
	public:
		inline static constexpr float DISPLAY_TIME = 8.f;

		void Display();

	protected:
		bool ShouldDrawImpl() const override;
		void DrawImpl() override;

	private:
		float _fLingerTime = 0.f;
		bool _bFirstFrame = false;
	};
}
