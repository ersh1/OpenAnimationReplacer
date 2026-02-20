#pragma once
#include "OpenAnimationReplacer.h"
#include "UIWindow.h"

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
		[[nodiscard]] bool IsInteractable() const;
		void DrawFilterPanel() const;
		void DrawLogEntry(AnimationLogEntry& a_logEntry);

		void DrawTrace(const ReplacementTrace& a_trace) const;
		std::string_view GetTraceResultText(ReplacementTrace::Step::StepResult a_stepResult) const;
		void DrawTraceCondition(const ReplacementTrace::Step::ConditionEntry& a_conditionEntry) const;
	};
}
