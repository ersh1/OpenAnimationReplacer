#pragma once
#include "AnimationEventLog.h"
#include "UIWindow.h"

namespace UI
{
    class UIAnimationEventLog : public UIWindow
    {
    protected:
        bool ShouldDrawImpl() const override;
        void DrawImpl() override;

    private:
		[[nodiscard]] bool IsInteractable() const;
		void DrawFilterPanel() const;
		void DrawEventSourcesPanel() const;
		bool DrawEventSource(const RE::ObjectRefHandle& a_handle) const;
        void DrawLogEntry(AnimationEventLogEntry& a_logEntry, uint32_t a_lastTimestamp) const;

		static int EventSourceInputTextCallback(ImGuiInputTextCallbackData* a_data);

		static inline RE::TESObjectREFR* _selectedRefr = nullptr;
        static inline std::string _filter;
    };
}
