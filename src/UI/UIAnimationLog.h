#pragma once
#include "UIWindow.h"
#include "OpenAnimationReplacer.h"

namespace UI
{
    class UIAnimationLog : public UIWindow
    {
    protected:
        bool ShouldDrawImpl() const override;
        void DrawImpl() override;
        void OnOpen() override;
        void OnClose() override;

    private:
        void DrawLogEntry(AnimationLogEntry& a_logEntry) const;
    };
}
