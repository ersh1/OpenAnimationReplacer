#pragma once
#include "UIWindow.h"

namespace UI
{
	class UIErrorBanner : public UIWindow
	{
	public:
	protected:
		bool ShouldDrawImpl() const override;
		void DrawImpl() override;
	};
}
