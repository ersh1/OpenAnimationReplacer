#pragma once
#include "UIWindow.h"

namespace UI
{
	class UIAnimationQueue : public UIWindow
	{
	public:
	protected:
		bool ShouldDrawImpl() const override;
		void DrawImpl() override;

	private:
		float _fLingerTime = 0.f;
	};
}
