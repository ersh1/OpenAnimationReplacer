#include "UIMain.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include "ActiveClip.h"
#include "DetectedProblems.h"
#include "Jobs.h"
#include "OpenAnimationReplacer.h"
#include "Parsing.h"
#include "UICommon.h"
#include "UIManager.h"

namespace UI
{
	bool UIMain::ShouldDrawImpl() const
	{
		return UIManager::GetSingleton().bShowMain;
	}

	void UIMain::DrawImpl()
	{
		// disable idle camera rotation
		if (const auto playerCamera = RE::PlayerCamera::GetSingleton()) {
			playerCamera->GetRuntimeData2().idleTimer = 0.f;
		}

		//ImGui::ShowDemoWindow();

		SetWindowDimensions(0.f, 0.f, 850.f, -1, WindowAlignment::kCenterLeft);

		const auto title = std::format("Open Animation Replacer {}.{}.{}", Plugin::VERSION.major(), Plugin::VERSION.minor(), Plugin::VERSION.patch());
		if (ImGui::Begin(title.data(), &UIManager::GetSingleton().bShowMain, ImGuiWindowFlags_NoCollapse)) {
			if (ImGui::BeginTable("EvaluateForReference", 2, ImGuiTableFlags_None)) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				static char formIDBuf[9] = "";
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);

				if (const auto consoleRefr = Utils::GetConsoleRefr()) {
					ImGui::BeginDisabled();
					std::string formID = std::format("{:08X}", consoleRefr->GetFormID());
					ImGui::InputText("Evaluate for reference", formID.data(), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
					ImGui::EndDisabled();
				} else {
					ImGui::InputTextWithHint("Evaluate for reference", "FormID...", formIDBuf, IM_ARRAYSIZE(formIDBuf), ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase, &ReferenceInputTextCallback);
				}

				ImGui::SameLine();
				UICommon::HelpMarker("Select a reference in the game's console, or type the FormID of a reference to color animations and conditions based on their current result for that reference. No need to type the leading zeros. (The player's FormID is 14).");

				if (const auto refr = UIManager::GetSingleton().GetRefrToEvaluate()) {
					ImGui::TableSetColumnIndex(1);
					ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(UICommon::SUCCESS_BG_COLOR));
					ImGui::TextUnformatted(refr->GetDisplayFullName());
				}

				ImGui::EndTable();
			}

			//ImGui::Spacing();

			const auto& style = ImGui::GetStyle();
			const float bottomBarHeight = ImGui::GetTextLineHeight() + style.FramePadding.y * 4 + style.ItemSpacing.y * 4;
			if (ImGui::BeginChild("Tabs", ImVec2(0.f, -bottomBarHeight), true)) {
				if (ImGui::BeginTabBar("TabBar")) {
					if (ImGui::BeginTabItem("Replacer Mods")) {
						DrawReplacerMods();
						ImGui::EndTabItem();
					}
					if (ImGui::BeginTabItem("Replacement Animations")) {
						DrawReplacementAnimations();
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}
			}
			ImGui::EndChild();

			const std::string animationLogButtonName = "Animation Log";
			const float animationLogButtonWidth = (ImGui::CalcTextSize(animationLogButtonName.data()).x + style.FramePadding.x * 2 + style.ItemSpacing.x);

			const std::string animationEventLogButtonName = "Event Log";
			const float animationEventLogButtonWidth = (ImGui::CalcTextSize(animationEventLogButtonName.data()).x + style.FramePadding.x * 2 + style.ItemSpacing.x);

			const std::string settingsButtonName = _bShowSettings ? "Settings <" : "Settings >";
			const float settingsButtonWidth = (ImGui::CalcTextSize(settingsButtonName.data()).x + style.FramePadding.x * 2 + style.ItemSpacing.x);

			// Bottom bar
			if (ImGui::BeginChild("BottomBar", ImVec2(ImGui::GetContentRegionAvail().x - animationLogButtonWidth - animationEventLogButtonWidth - settingsButtonWidth, 0.f), true)) {
				ImGui::AlignTextToFramePadding();
				// Status text

				auto& problems = DetectedProblems::GetSingleton();

				const std::string_view problemText = problems.GetProblemMessage();
				using Severity = DetectedProblems::Severity;

				if (problems.GetProblemSeverity() > Severity::kNone) {
					// Problems found
					switch (problems.GetProblemSeverity()) {
					case Severity::kWarning:
						ImGui::PushStyleColor(ImGuiCol_Button, UICommon::WARNING_BUTTON_COLOR);
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UICommon::WARNING_BUTTON_HOVERED_COLOR);
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, UICommon::WARNING_BUTTON_ACTIVE_COLOR);
						break;
					case Severity::kError:
						ImGui::PushStyleColor(ImGuiCol_Button, UICommon::ERROR_BUTTON_COLOR);
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UICommon::ERROR_BUTTON_HOVERED_COLOR);
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, UICommon::ERROR_BUTTON_ACTIVE_COLOR);
						break;
					}

					if (ImGui::Button(problemText.data(), ImGui::GetContentRegionAvail())) {
						if (problems.CheckForProblems()) {
							ImGui::OpenPopup("Problems");
						}
					}

					ImGui::PopStyleColor(3);

					const auto viewport = ImGui::GetMainViewport();
					ImGui::SetNextWindowPos(ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.5f), ImGuiCond_None, ImVec2(0.5f, 0.5f));

					float height = 30.f;
					if (problems.IsOutdated()) {
						height += 100.f;
					}
					if (problems.HasMissingPlugins()) {
						height += 100.f;
						height += problems.NumMissingPlugins() * ImGui::GetTextLineHeightWithSpacing();
					}
					if (problems.HasSubModsWithInvalidEntries()) {
						height += 200.f;
						height += problems.NumSubModsSharingPriority() * ImGui::GetTextLineHeightWithSpacing();
					}
					if (problems.HasSubModsSharingPriority()) {
						height += 200.f;
						height += problems.NumSubModsSharingPriority() * ImGui::GetTextLineHeightWithSpacing();
					}

					const float buttonHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f;
					height += buttonHeight;

					height = std::clamp(height, 600.f, ImGui::GetIO().DisplaySize.y);

					ImGui::SetNextWindowSize(ImVec2(800.f, height));
					if (ImGui::BeginPopupModal("Problems", nullptr, ImGuiWindowFlags_NoResize)) {
						auto childSize = ImGui::GetContentRegionAvail();
						childSize.y -= buttonHeight;
						if (ImGui::BeginChild("ProblemsContent", childSize)) {
							bool bShouldDrawSeparator = false;
							if (problems.IsOutdated()) {
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::ERROR_TEXT_COLOR);
								ImGui::TextWrapped("ERROR: At least one replacer mod has conditions that require a newer version of Open Animation Replacer.\nThe mod will not function correctly. Please update Open Animation Replacer!");
								ImGui::PopStyleColor();

								bShouldDrawSeparator = true;
							}

							if (problems.HasMissingPlugins()) {
								if (bShouldDrawSeparator) {
									ImGui::Spacing();
									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();
									ImGui::Spacing();
								}
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::ERROR_TEXT_COLOR);
								ImGui::TextWrapped("ERROR: At least one replacer mod has conditions that aren't included in Open Animation Replacer itself, but are added by an Open Animation Replacer API plugin that is either not installed or outdated.\nThe mod will not function correctly. Please download or update the required plugin!");
								ImGui::PopStyleColor();
								ImGui::Spacing();
								DrawMissingPlugins();

								bShouldDrawSeparator = true;
							}

							if (problems.HasInvalidPlugins()) {
								if (bShouldDrawSeparator) {
									ImGui::Spacing();
									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();
									ImGui::Spacing();
								}
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::ERROR_TEXT_COLOR);
								ImGui::TextWrapped("ERROR: At least one replacer mod has conditions that aren't included in Open Animation Replacer itself, but are added by an Open Animation Replacer API plugin that seems to have failed to initialize properly.\nThe mod will not function correctly. Please make sure you have installed the required plugin and all its dependencies correctly!");
								ImGui::PopStyleColor();
								ImGui::Spacing();
								DrawInvalidPlugins();

								bShouldDrawSeparator = true;
							}

							if (problems.HasReplacerModsWithInvalidEntries()) {
								if (bShouldDrawSeparator) {
									ImGui::Spacing();
									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();
									ImGui::Spacing();
								}
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::ERROR_TEXT_COLOR);
								ImGui::TextWrapped("ERROR: At least one replacer mod has conditions that are invalid.\nThe mod will not function correctly. Please check the conditions of the replacer mod, and check whether there's an update available!");
								ImGui::PopStyleColor();
								ImGui::Spacing();
								DrawReplacerModsWithInvalidConditions();

								bShouldDrawSeparator = true;
							}

							if (problems.HasSubModsWithInvalidEntries()) {
								if (bShouldDrawSeparator) {
									ImGui::Spacing();
									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();
									ImGui::Spacing();
								}
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::ERROR_TEXT_COLOR);
								ImGui::TextWrapped("ERROR: At least one submod has conditions that are invalid.\nThe mod will not function correctly. Please check the conditions of the replacer mod, and check whether there's an update available!");
								ImGui::PopStyleColor();
								ImGui::Spacing();
								DrawSubModsWithInvalidConditions();

								bShouldDrawSeparator = true;
							}

							if (problems.HasSubModsSharingPriority()) {
								if (bShouldDrawSeparator) {
									ImGui::Spacing();
									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();
									ImGui::Spacing();
								}
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::WARNING_TEXT_COLOR);
								ImGui::TextWrapped("WARNING: The following mods have conflicting priorities.\nThis might cause unexpected behavior if they replace the same animations.");
								ImGui::PopStyleColor();
								ImGui::Spacing();
								DrawConflictingSubMods();

								bShouldDrawSeparator = true;
							}
						}
						ImGui::EndChild();

						constexpr float buttonWidth = 120.f;
						ImGui::SetCursorPosX((ImGui::GetWindowSize().x - buttonWidth) * 0.5f);
						ImGui::SetItemDefaultFocus();
						if (ImGui::Button("OK", ImVec2(buttonWidth, 0))) {
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndPopup();
					}
				} else {
					ImGui::TextUnformatted(problemText.data());
				}
			}
			ImGui::EndChild();

			// Animation log button
			ImGui::SameLine(ImGui::GetWindowWidth() - animationLogButtonWidth - animationEventLogButtonWidth - settingsButtonWidth);
			if (ImGui::Button(animationLogButtonName.data(), ImVec2(0.f, bottomBarHeight - style.ItemSpacing.y))) {
				Settings::bEnableAnimationLog = !Settings::bEnableAnimationLog;
				Settings::WriteSettings();
			}

			// Animation event log button
			ImGui::SameLine(ImGui::GetWindowWidth() - animationEventLogButtonWidth - settingsButtonWidth);
			if (ImGui::Button(animationEventLogButtonName.data(), ImVec2(0.f, bottomBarHeight - style.ItemSpacing.y))) {
				Settings::bEnableAnimationEventLog = !Settings::bEnableAnimationEventLog;
				Settings::WriteSettings();
			}

			// Settings button
			ImGui::SameLine(ImGui::GetWindowWidth() - settingsButtonWidth);
			if (ImGui::Button(settingsButtonName.data(), ImVec2(0.f, bottomBarHeight - style.ItemSpacing.y))) {
				_bShowSettings = !_bShowSettings;
			}
		}

		const auto windowContentMax = ImGui::GetWindowContentRegionMax();
		const auto windowPos = ImGui::GetWindowPos();
		const auto settingsPos = ImVec2(windowPos.x + windowContentMax.x + 7.f, windowPos.y + windowContentMax.y + 8.f);

		ImGui::End();

		if (_bShowSettings) {
			DrawSettings(settingsPos);
		}
	}

	void UIMain::OnOpen()
	{
		UIManager::GetSingleton().AddInputConsumer();

		auto& detectedProblems = DetectedProblems::GetSingleton();
		detectedProblems.CheckForSubModsSharingPriority();
		detectedProblems.CheckForSubModsWithInvalidEntries();
	}

	void UIMain::OnClose()
	{
		UIManager::GetSingleton().RemoveInputConsumer();
	}

	void UIMain::DrawSettings(const ImVec2& a_pos)
	{
		ImGui::SetNextWindowPos(a_pos, ImGuiCond_None, ImVec2(0.f, 1.f));

		if (ImGui::Begin("Settings", &_bShowSettings, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
			// UI settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("UI Settings");
			ImGui::Spacing();

			if (UICommon::InputKey("Menu key", Settings::uToggleUIKeyData)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set the key to toggle the UI.");

			ImGui::Spacing();

			if (ImGui::Checkbox("Show welcome banner", &Settings::bShowWelcomeBanner)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Enable to show the welcome banner on startup.");

			static float tempScale = Settings::fUIScale;
			ImGui::SliderFloat("UI scale", &tempScale, 0.5f, 2.f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SameLine();
			UICommon::HelpMarker("Set the UI scale.");
			ImGui::SameLine();
			ImGui::BeginDisabled(tempScale == Settings::fUIScale);
			if (ImGui::Button("Apply##UIScale")) {
				Settings::fUIScale = tempScale;
				Settings::WriteSettings();
			}
			ImGui::EndDisabled();

			ImGui::Spacing();
			ImGui::Separator();

			// General settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("General Settings");
			ImGui::Spacing();

			constexpr uint16_t animLimitMin = 0x4000;
			const uint16_t animLimitMax = Settings::GetMaxAnimLimit();
			if (ImGui::SliderScalar("Animation limit", ImGuiDataType_U16, &Settings::uAnimationLimit, &animLimitMin, &animLimitMax, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set the animation limit per behavior project. The game will crash if you set this too high without increasing the heap size. The game is incapable of playing animations past the upper limit set here, there's no point trying to circumvent it through the .ini file.");

			constexpr uint32_t heapMin = 0x20000000;
			constexpr uint32_t heapMax = 0x7FC00000;
			if (ImGui::SliderScalar("Havok heap size", ImGuiDataType_U32, &Settings::uHavokHeapSize, &heapMin, &heapMax, "0x%X", ImGuiSliderFlags_AlwaysClamp)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set the havok heap size. Takes effect after restarting the game. (Vanilla value is 0x20000000)");

			if (ImGui::Checkbox("Async parsing", &Settings::bAsyncParsing)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Enable to asynchronously parse all the replacer mods on load. This dramatically speeds up the process. No real reason to disable this setting.");

			if (Settings::bDisablePreloading) {
				ImGui::BeginDisabled();
				bool bDummy = false;
				ImGui::Checkbox("Load default behaviors in main menu", &bDummy);
				ImGui::EndDisabled();
			} else {
				if (ImGui::Checkbox("Load default behaviors in main menu", &Settings::bLoadDefaultBehaviorsInMainMenu)) {
					Settings::WriteSettings();
				}
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Enable to start loading default male/female behaviors in the main menu. Ignored with animation preloading disabled as there's no benefit in doing so in that case.");

			ImGui::Spacing();
			ImGui::Separator();

			// Duplicate filtering settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Duplicate Filtering Settings");
			ImGui::Spacing();

			if (ImGui::Checkbox("Filter out duplicate animations", &Settings::bFilterOutDuplicateAnimations)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Enable to check for duplicates before adding an animation. Only one copy of an animation binding will be used in multiple replacer animations. This might massively cut down on the number of loaded animations as replacer mods tend to use multiple copies of the same animation with different condition.");

			/*ImGui::BeginDisabled(!Settings::bFilterOutDuplicateAnimations);
            if (ImGui::Checkbox("Cache animation file hashes", &Settings::bCacheAnimationFileHashes)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Enable to save a cache of animation file hashes, so the hashes don't have to be recalculated on every game launch. It's saved to a .bin file next to the .dll. This should speed up the loading process a little bit.");
            ImGui::SameLine();
            if (ImGui::Button("Clear cache")) {
                AnimationFileHashCache::GetSingleton().DeleteCache();
            }
            UICommon::AddTooltip("Delete the animation file hash cache. This will cause the hashes to be recalculated on the next game launch.");
            ImGui::EndDisabled();*/

			ImGui::Spacing();
			ImGui::Separator();

			// Animation queue progress bar settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Animation Queue Progress Bar Settings");
			ImGui::Spacing();

			if (ImGui::Checkbox("Enable", &Settings::bEnableAnimationQueueProgressBar)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Enable to show a progress bar while animations are loading.");

			if (ImGui::SliderFloat("Linger time", &Settings::fAnimationQueueLingerTime, 0.f, 10.f, "%.1f s", ImGuiSliderFlags_AlwaysClamp)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Duration the bar will remain on screen after all animations have been loaded.");

			ImGui::Spacing();
			ImGui::Separator();

			// Animation log settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Animation Log Settings");
			ImGui::Spacing();

			if (ImGui::SliderFloat("Log width", &Settings::fAnimationLogWidth, 300.f, 1500.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Width of the animation log window.");

			constexpr uint32_t entriesMin = 1;
			constexpr uint32_t entriesMax = 20;
			if (ImGui::SliderScalar("Max entries", ImGuiDataType_U32, &Settings::uAnimationLogMaxEntries, &entriesMin, &entriesMax, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				AnimationLog::GetSingleton().ClampLog();
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set the maximum number of entries in the animation log.");

			const char* logTypes[] = { "When replaced", "With potential replacements", "All" };
			if (ImGui::SliderInt("Activate log mode", reinterpret_cast<int*>(&Settings::uAnimationActivateLogMode), 0, 2, logTypes[Settings::uAnimationActivateLogMode])) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set conditions in which animation clip activation should be logged.");

			if (ImGui::SliderInt("Echo log mode", reinterpret_cast<int*>(&Settings::uAnimationEchoLogMode), 0, 2, logTypes[Settings::uAnimationEchoLogMode])) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set conditions in which animation clip echo should be logged. (Echo is when an animation clip transitions into itself. Happens with some non-looping clips)");

			if (ImGui::SliderInt("Loop log mode", reinterpret_cast<int*>(&Settings::uAnimationLoopLogMode), 0, 2, logTypes[Settings::uAnimationLoopLogMode])) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set conditions in which animation clip looping should be logged.");

			if (ImGui::Checkbox("Only log current project", &Settings::bAnimationLogOnlyActiveGraph)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Enable to only log animation clips from the active behavior graph - filter out first person animations when in third person, etc.");

			if (ImGui::Checkbox("Write to log file##AnimationLog", &Settings::bAnimationLogWriteToTextLog)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Enable to also log the clips into the file at 'Documents\\My Games\\Skyrim Special Edition\\SKSE\\OpenAnimationReplacer.log'.");

			ImGui::Spacing();
			ImGui::Separator();

			// Animation event log settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Animation Event Log Settings");
			ImGui::Spacing();

			float tempSize[2] = { Settings::fAnimationEventLogWidth, Settings::fAnimationEventLogHeight };
			if (ImGui::SliderFloat2("Event Log Size", tempSize, 300.f, 1500.f, "%.0f")) {
				Settings::fAnimationEventLogWidth = tempSize[0];
				Settings::fAnimationEventLogHeight = tempSize[1];
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Width and height of the animation event log.");

			if (ImGui::Checkbox("Write to log file##EventLog", &Settings::bAnimationEventLogWriteToTextLog)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Enable to also log animation events into the file at 'Documents\\My Games\\Skyrim Special Edition\\SKSE\\OpenAnimationReplacer.log'.");

			ImGui::Spacing();

			float tempOffset[2] = { Settings::fAnimationLogsOffsetX, Settings::fAnimationLogsOffsetY };
			if (ImGui::SliderFloat2("Log Offset", tempOffset, 0.f, 500.f, "%.0f")) {
				Settings::fAnimationLogsOffsetX = tempOffset[0];
				Settings::fAnimationLogsOffsetY = tempOffset[1];
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Position offset from the corner of the screen used by the logs.");

			ImGui::Spacing();
			ImGui::Separator();

			// Workarounds
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Workarounds");
			ImGui::SameLine();
			UICommon::HelpMarker("These settings are workarounds for some issues with Legacy replacer mods.");
			ImGui::Spacing();

			if (ImGui::Checkbox("Don't reset on loop the state data of Random conditions by default in Legacy mods", &Settings::bLegacyKeepRandomResultsByDefault)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set to disable the setting \"Should reset on loop/echo\" by default in Random conditions for Legacy replacer mods. This will make them behave as previously expected. Changing this setting takes effect after restarting the game.");

			ImGui::Spacing();
			ImGui::Separator();

			// Experimental settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Experimental Settings");
			ImGui::SameLine();
			UICommon::HelpMarker("These settings are considered experimental and might not work correctly for you. They take effect after restarting the game.");
			ImGui::Spacing();

			if (ImGui::Checkbox("Disable preloading", &Settings::bDisablePreloading)) {
				if (Settings::bDisablePreloading) {
					Settings::bLoadDefaultBehaviorsInMainMenu = false;
				}
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set to disable preloading all animations when the behavior is first loaded. This is not recommended, and the setting has been proven to cause some differences in animation behavior.");

			if (ImGui::Checkbox("Increase animation limit", &Settings::bIncreaseAnimationLimit)) {
				Settings::ClampAnimLimit();
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Enable to increase the animation limit to double the default value. Should generally work fine, but I might have missed some places to patch in the game code so this is still considered to be experimental. There's no benefit in enabling this if you're not going over the limit.");
		}

		ImGui::End();
	}

	void UIMain::DrawMissingPlugins()
	{
		if (ImGui::BeginTable("MissingPlugins", 3, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Min required", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			DetectedProblems::GetSingleton().ForEachMissingPlugin([&](auto& a_missingPlugin) {
				auto& missingPluginName = a_missingPlugin.first;
				auto& missingPluginVersion = a_missingPlugin.second;

				const REL::Version currentPluginVersion = OpenAnimationReplacer::GetSingleton().GetPluginVersion(missingPluginName);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				ImGui::TextUnformatted(missingPluginName.data());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(missingPluginVersion.string("."sv).data());
				ImGui::TableSetColumnIndex(2);
				if (currentPluginVersion > 0) {
					ImGui::TextUnformatted(currentPluginVersion.string("."sv).data());
				} else {
					ImGui::TextUnformatted("Missing");
				}
			});

			ImGui::EndTable();
		}
	}

	void UIMain::DrawInvalidPlugins()
	{
		if (ImGui::BeginTable("InvalidPlugins", 3, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Min required", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			DetectedProblems::GetSingleton().ForEachMissingPlugin([&](auto& a_invalidPlugin) {
				auto& invalidPluginName = a_invalidPlugin.first;
				auto& invalidPluginVersion = a_invalidPlugin.second;

				const REL::Version currentPluginVersion = OpenAnimationReplacer::GetSingleton().GetPluginVersion(invalidPluginName);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				ImGui::TextUnformatted(invalidPluginName.data());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(invalidPluginVersion.string("."sv).data());
				ImGui::TableSetColumnIndex(2);
				if (currentPluginVersion > 0) {
					ImGui::TextUnformatted(currentPluginVersion.string("."sv).data());
				} else {
					ImGui::TextUnformatted("Missing");
				}
			});

			ImGui::EndTable();
		}
	}

	void UIMain::DrawConflictingSubMods() const
	{
		if (ImGui::BeginTable("ConflictingSubMods", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("Submod", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			bool bJustStarted = true;
			int32_t prevPriority = 0;

			DetectedProblems::GetSingleton().ForEachSubModSharingPriority([&](const SubMod* a_subMod) {
				const auto parentMod = a_subMod->GetParentMod();

				const auto nodeName = a_subMod->GetName();

				if (bJustStarted) {
					bJustStarted = false;
				} else if (prevPriority != a_subMod->GetPriority()) {
					ImGui::Spacing();
				}
				prevPriority = a_subMod->GetPriority();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				const bool bOpen = ImGui::TreeNode(nodeName.data());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(std::to_string(a_subMod->GetPriority()).data());
				if (bOpen) {
					if (parentMod) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(parentMod->GetName().data());
						UICommon::TextUnformattedEllipsis(a_subMod->GetPath().data());
					}
					ImGui::TreePop();
				}
			});

			ImGui::EndTable();
		}
	}

	void UIMain::DrawReplacerModsWithInvalidConditions() const
	{
		if (ImGui::BeginTable("ReplacerModsWithInvalidConditions", 1, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("Replacer mod", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			DetectedProblems::GetSingleton().ForEachReplacerModWithInvalidEntries([&](const ReplacerMod* a_replacerMod) {
				const auto replacerModName = a_replacerMod->GetName();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(replacerModName.data());
			});

			ImGui::EndTable();
		}
	}

	void UIMain::DrawSubModsWithInvalidConditions() const
	{
		if (ImGui::BeginTable("SubModsWithInvalidConditions", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("Parent mod", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Submod", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			DetectedProblems::GetSingleton().ForEachSubModWithInvalidEntries([&](const SubMod* a_subMod) {
				const auto subModName = a_subMod->GetName();
				const auto parentModName = a_subMod->GetParentMod()->GetName();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				ImGui::TextUnformatted(parentModName.data());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(subModName.data());
			});

			ImGui::EndTable();
		}
	}

	void UIMain::DrawReplacerMods()
	{
		static char nameFilterBuf[32] = "";
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 18);
		ImGui::InputTextWithHint("Filter", "Mod/submod/author name...", nameFilterBuf, IM_ARRAYSIZE(nameFilterBuf));
		ImGui::SameLine();
		UICommon::HelpMarker("Type a part of a mod/submod/author name to filter the list.");

		const float offset = ImGui::CalcTextSize("Inspect Mode").x + ImGui::CalcTextSize("User Mode").x + ImGui::CalcTextSize("Author Mode").x + ImGui::CalcTextSize("(?)").x + 100.f;
		ImGui::SameLine(ImGui::GetWindowWidth() - offset);

		ImGui::RadioButton("Inspect Mode", reinterpret_cast<int*>(&_editMode), 0);
		ImGui::SameLine();
		ImGui::RadioButton("User Mode", reinterpret_cast<int*>(&_editMode), 1);
		ImGui::SameLine();
		ImGui::RadioButton("Author Mode", reinterpret_cast<int*>(&_editMode), 2);
		ImGui::SameLine();
		UICommon::HelpMarker("Editing in author mode will edit the original config files contained in the mod folders. User mode creates and saves a new configuration file that will override the original one when the settings are reloaded. That won't affect the original file.");

		ImGui::Separator();

		if (ImGui::BeginChild("Mods")) {
			bool bFirstDisplayed = true;
			OpenAnimationReplacer::GetSingleton().ForEachSortedReplacerMod([&](ReplacerMod* a_replacerMod) {
				// Parse names for filtering and duplicate names
				std::unordered_map<std::string, SubModNameFilterResult> subModNameFilterResults{};
				const bool bEntireReplacerModMatchesFilter = !std::strlen(nameFilterBuf) || Utils::ContainsStringIgnoreCase(a_replacerMod->GetName(), nameFilterBuf) || Utils::ContainsStringIgnoreCase(a_replacerMod->GetAuthor(), nameFilterBuf);
				bool bDisplayMod = bEntireReplacerModMatchesFilter;

				a_replacerMod->ForEachSubMod([&](const SubMod* a_subMod) {
					SubModNameFilterResult res;
					const auto subModName = a_subMod->GetName();
					res.bDisplay = bEntireReplacerModMatchesFilter || Utils::ContainsStringIgnoreCase(subModName, nameFilterBuf);

					// if at least one submod matches filter, display the replacer mod
					if (res.bDisplay) {
						bDisplayMod = true;
					}

					auto [it, bInserted] = subModNameFilterResults.try_emplace(subModName.data(), res);
					if (!bInserted) {
						subModNameFilterResults[subModName.data()].bDuplicateName = true;
					}

					return RE::BSVisit::BSVisitControl::kContinue;
				});

				if (bDisplayMod) {
					if (!bFirstDisplayed && a_replacerMod->IsLegacy()) {
						ImGui::Separator();
					}
					DrawReplacerMod(a_replacerMod, subModNameFilterResults);
					bFirstDisplayed = false;
				}
			});
		}
		ImGui::EndChild();
	}

	void UIMain::DrawReplacerMod(ReplacerMod* a_replacerMod, std::unordered_map<std::string, SubModNameFilterResult>& a_filterResults)
	{
		if (!a_replacerMod) {
			return;
		}

		bool bNodeOpen = ImGui::TreeNodeEx(a_replacerMod, ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");

		if (a_replacerMod->IsDirty()) {
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::DIRTY_COLOR, "*");
		}

		if (a_replacerMod->GetConfigSource() == Parsing::ConfigSource::kUser) {
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::USER_MOD_COLOR, "(User)");
		}

		// node name
		ImGui::SameLine();
		if (a_replacerMod->HasInvalidConditions(false) || a_replacerMod->HasInvalidFunctions()) {
			UICommon::TextUnformattedColored(UICommon::INVALID_CONDITION_COLOR, a_replacerMod->GetName().data());
		} else {
			ImGui::TextUnformatted(a_replacerMod->GetName().data());
		}

		if (bNodeOpen) {
			// Mod name
			if (!a_replacerMod->IsLegacy() && _editMode == EditMode::kAuthor) {
				const std::string nameId = "Mod name##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "name";
				ImGui::SetNextItemWidth(-150.f);
				std::string tempName(a_replacerMod->GetName());
				if (ImGui::InputTextWithHint(nameId.data(), "Mod name", &tempName)) {
					a_replacerMod->SetName(tempName);
					a_replacerMod->SetDirty(true);
				}
			}

			// Mod author
			const std::string authorId = "Author##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "author";
			if (!a_replacerMod->IsLegacy() && _editMode == EditMode::kAuthor) {
				ImGui::SetNextItemWidth(250.f);
				std::string tempAuthor(a_replacerMod->GetAuthor());
				if (ImGui::InputTextWithHint(authorId.data(), "Author", &tempAuthor)) {
					a_replacerMod->SetAuthor(tempAuthor);
					a_replacerMod->SetDirty(true);
				}
			} else if (!a_replacerMod->GetAuthor().empty()) {
				if (ImGui::BeginTable(authorId.data(), 1, ImGuiTableFlags_BordersOuter)) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					UICommon::TextDescriptionRightAligned("Author");
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(a_replacerMod->GetAuthor().data());
					ImGui::EndTable();
				}
			}

			// Mod description
			const std::string descriptionId = "Mod description##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "description";
			if (!a_replacerMod->IsLegacy() && _editMode == EditMode::kAuthor) {
				ImGui::SetNextItemWidth(-150.f);
				std::string tempDescription(a_replacerMod->GetDescription());
				if (ImGui::InputTextMultiline(descriptionId.data(), &tempDescription, ImVec2(0, ImGui::GetTextLineHeight() * 5))) {
					a_replacerMod->SetDescription(tempDescription);
					a_replacerMod->SetDirty(true);
				}
				ImGui::Spacing();
			} else if (!a_replacerMod->GetDescription().empty()) {
				if (ImGui::BeginTable(descriptionId.data(), 1, ImGuiTableFlags_BordersOuter)) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					UICommon::TextDescriptionRightAligned("Description");
					UICommon::TextUnformattedWrapped(a_replacerMod->GetDescription().data());
					ImGui::EndTable();
				}
				ImGui::Spacing();
			}

			// Condition presets
			if (_editMode > EditMode::kNone || a_replacerMod->HasConditionPresets()) {
				const std::string conditionPresetsLabel = "Condition presets##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "conditionPresets";
				ImGuiTreeNodeFlags flags = a_replacerMod->HasConditionPresets() ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
				if (ImGui::CollapsingHeader(conditionPresetsLabel.data(), flags)) {
					ImGui::AlignTextToFramePadding();
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

					if (a_replacerMod->HasConditionPresets()) {
						bool bShouldSort = false;
						a_replacerMod->ForEachConditionPreset([&](Conditions::ConditionPreset* a_preset) {
							if (DrawConditionPreset(a_replacerMod, a_preset, bShouldSort)) {
								a_replacerMod->SetDirty(true);
							}

							return RE::BSVisit::BSVisitControl::kContinue;
						});

						if (bShouldSort) {
							a_replacerMod->SortConditionPresets();
						}
					}

					if (_editMode > EditMode::kNone) {
						// Add condition preset button
						constexpr auto popupName = "Adding new condition preset"sv;
						if (ImGui::Button("Add new condition preset")) {
							const auto popupPos = ImGui::GetCursorScreenPos();
							ImGui::SetNextWindowPos(popupPos);
							ImGui::OpenPopup(popupName.data());
						}

						if (ImGui::BeginPopupModal(popupName.data(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
							std::string conditionPresetName;
							if (ImGui::InputTextWithHint("##ConditionPresetName", "Type a unique name...", &conditionPresetName, ImGuiInputTextFlags_EnterReturnsTrue)) {
								if (conditionPresetName.size() > 2 && !a_replacerMod->HasConditionPreset(conditionPresetName)) {
									auto newConditionPreset = std::make_unique<Conditions::ConditionPreset>(conditionPresetName, ""sv);
									a_replacerMod->AddConditionPreset(newConditionPreset);
									a_replacerMod->SetDirty(true);
									ImGui::CloseCurrentPopup();
								}
							}
							ImGui::SetItemDefaultFocus();
							ImGui::SameLine();
							if (ImGui::Button("Cancel")) {
								ImGui::CloseCurrentPopup();
							}
							ImGui::EndPopup();
						}
					}

					ImGui::PopStyleVar();
				}
			}

			// Submods
			ImGui::TextUnformatted("Submods:");
			a_replacerMod->ForEachSubMod([&](SubMod* a_subMod) {
				// Filter
				const auto search = a_filterResults.find(a_subMod->GetName().data());
				if (search != a_filterResults.end()) {
					const SubModNameFilterResult& filterResult = search->second;
					if (filterResult.bDisplay) {
						DrawSubMod(a_replacerMod, a_subMod, filterResult.bDuplicateName);
					}
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});

			// Save mod config
			if (!a_replacerMod->IsLegacy() && _editMode > EditMode::kNone) {
				const bool bIsDirty = a_replacerMod->IsDirty();
				if (!bIsDirty) {
					ImGui::BeginDisabled();
				}
				ImGui::BeginDisabled(a_replacerMod->HasInvalidConditions(true) || a_replacerMod->HasInvalidFunctions());
				if (ImGui::Button(_editMode == EditMode::kAuthor ? "Save mod config (Author)" : "Save mod config (User)")) {
					a_replacerMod->SaveConfig(_editMode);
				}
				ImGui::EndDisabled();
				if (!bIsDirty) {
					ImGui::EndDisabled();
				}

				// Reload mod config
				ImGui::SameLine();
				UICommon::ButtonWithConfirmationModal("Reload mod config", "Are you sure you want to reload the config?\nThis operation cannot be undone!\n\n"sv, [&]() {
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReloadReplacerModConfigJob>(a_replacerMod);
				});

				// delete user config
				const bool bUserConfigExists = Utils::DoesUserConfigExist(a_replacerMod->GetPath());
				if (!bUserConfigExists) {
					ImGui::BeginDisabled();
				}
				ImGui::SameLine();
				UICommon::ButtonWithConfirmationModal("Delete mod user config", "Are you sure you want to delete the user config?\nThis operation cannot be undone!\n\n"sv, [&]() {
					Utils::DeleteUserConfig(a_replacerMod->GetPath());
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReloadReplacerModConfigJob>(a_replacerMod);
				});
				if (!bUserConfigExists) {
					ImGui::EndDisabled();
				}
			}

			ImGui::TreePop();
		}
	}

	void UIMain::DrawSubMod(ReplacerMod* a_replacerMod, SubMod* a_subMod, bool a_bAddPathToName /*= false*/)
	{
		bool bStyleVarPushed = false;
		if (a_subMod->IsDisabled()) {
			auto& style = ImGui::GetStyle();
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.DisabledAlpha);
			bStyleVarPushed = true;
		}

		bool bNodeOpen = ImGui::TreeNodeEx(a_subMod, ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");

		// Disable checkbox
		ImGui::SameLine();
		if (_editMode > EditMode::kNone) {
			std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_subMod));
			ImGui::PushID(idString.data());
			bool bEnabled = !a_subMod->IsDisabled();
			if (ImGui::Checkbox("##disableSubMod", &bEnabled)) {
				a_subMod->SetDisabled(!bEnabled);
				OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, true);
				a_subMod->SetDirty(true);
			}
			UICommon::AddTooltip("If unchecked, the submod will be disabled and none of its replacement animations will be considered.");
			ImGui::PopID();
		}

		if (a_subMod->IsDirty()) {
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::DIRTY_COLOR, "*");
		}

		switch (a_subMod->GetConfigSource()) {
		case Parsing::ConfigSource::kUser:
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::USER_MOD_COLOR, "(User)");
			break;
		case Parsing::ConfigSource::kLegacy:
			ImGui::SameLine();
			UICommon::TextUnformattedDisabled("(Legacy)");
			break;
		case Parsing::ConfigSource::kLegacyActorBase:
			ImGui::SameLine();
			UICommon::TextUnformattedDisabled("(Legacy ActorBase)");
			break;
		}

		// node name
		ImGui::SameLine();
		if (a_subMod->HasInvalidConditions() || a_subMod->HasInvalidFunctions()) {
			UICommon::TextUnformattedColored(UICommon::INVALID_CONDITION_COLOR, a_subMod->GetName().data());
		} else {
			ImGui::TextUnformatted(a_subMod->GetName().data());
		}
		ImGui::SameLine();

		if (a_bAddPathToName) {
			UICommon::TextUnformattedDisabled(a_subMod->GetPath().data());
			ImGui::SameLine();
		}

		float cursorPosX = ImGui::GetCursorPosX();

		std::string priorityText = "Priority: " + std::to_string(a_subMod->GetPriority());
		UICommon::SecondColumn(_firstColumnWidthPercent);
		if (ImGui::GetCursorPosX() < cursorPosX) {
			// make sure we don't draw priority text over the name
			ImGui::SetCursorPosX(cursorPosX);
		}
		ImGui::TextUnformatted(priorityText.data());

		if (bStyleVarPushed) {
			ImGui::PopStyleVar();
		}

		if (bNodeOpen) {
			// Submod name
			{
				if (_editMode == EditMode::kAuthor) {
					std::string subModNameId = "Submod name##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "name";
					ImGui::SetNextItemWidth(-150.f);
					std::string tempName(a_subMod->GetName());
					if (ImGui::InputTextWithHint(subModNameId.data(), "Submod name", &tempName)) {
						a_subMod->SetName(tempName);
						a_subMod->SetDirty(true);
					}
				}
			}

			// Submod description
			{
				std::string subModDescriptionId = "Submod description##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "description";
				if (_editMode == EditMode::kAuthor) {
					ImGui::SetNextItemWidth(-150.f);
					std::string tempDescription(a_subMod->GetDescription());
					if (ImGui::InputTextMultiline(subModDescriptionId.data(), &tempDescription, ImVec2(0, ImGui::GetTextLineHeight() * 5))) {
						a_subMod->SetDescription(tempDescription);
						a_subMod->SetDirty(true);
					}
				} else if (!a_subMod->GetDescription().empty()) {
					if (ImGui::BeginTable(subModDescriptionId.data(), 1, ImGuiTableFlags_BordersOuter)) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						UICommon::TextDescriptionRightAligned("Description");
						UICommon::TextUnformattedWrapped(a_subMod->GetDescription().data());
						ImGui::EndTable();
					}
				}
			}

			// Submod priority
			{
				std::string priorityLabel = "Priority##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "priority";
				if (_editMode != EditMode::kNone) {
					int32_t tempPriority = a_subMod->GetPriority();
					if (ImGui::InputInt(priorityLabel.data(), &tempPriority, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
						a_subMod->SetPriority(tempPriority);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, true);
						a_subMod->SetDirty(true);
					}
				} else {
					if (ImGui::BeginTable(priorityLabel.data(), 1, ImGuiTableFlags_BordersOuter)) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						UICommon::TextDescriptionRightAligned("Priority");
						ImGui::TextUnformatted(std::to_string(a_subMod->GetPriority()).data());
						ImGui::EndTable();
					}
				}
			}

			auto tableWidth = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("(?)").x - ImGui::GetStyle().FramePadding.x * 2;

			// Submod override animations folder
			{
				std::string overrideAnimationsFolderLabel = "Override animations folder##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "overrideAnimationsFolder";
				std::string overrideAnimationsFolderTooltip = "If set, this submod will load animations from the given folder (in the parent directory) instead of its own folder.";
				if (_editMode != EditMode::kNone) {
					ImGui::SetNextItemWidth(-220.f);
					std::string tempOverrideAnimationsFolder(a_subMod->GetOverrideAnimationsFolder());
					if (ImGui::InputTextWithHint(overrideAnimationsFolderLabel.data(), "Override animations folder", &tempOverrideAnimationsFolder)) {
						a_subMod->SetOverrideAnimationsFolder(tempOverrideAnimationsFolder);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(overrideAnimationsFolderTooltip.data());
				} else if (!a_subMod->GetOverrideAnimationsFolder().empty()) {
					if (ImGui::BeginTable(overrideAnimationsFolderLabel.data(), 1, ImGuiTableFlags_BordersOuter, ImVec2(tableWidth, 0))) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						UICommon::TextDescriptionRightAligned("Override animations folder");
						ImGui::TextUnformatted(a_subMod->GetOverrideAnimationsFolder().data());
						ImGui::EndTable();
					}
					ImGui::SameLine();
					UICommon::HelpMarker(overrideAnimationsFolderTooltip.data());
				}
			}

			// Submod required project name
			{
				std::string requiredProjectNameId = "Required project name##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "requiredProjectName";
				std::string requiredProjectNameTooltip = "If set, this submod will only load for the specified project name. Leave empty to load for all projects.";
				if (_editMode != EditMode::kNone) {
					ImGui::SetNextItemWidth(-220.f);
					std::string tempRequiredProjectName(a_subMod->GetRequiredProjectName());
					if (ImGui::InputTextWithHint(requiredProjectNameId.data(), "Required project name", &tempRequiredProjectName)) {
						a_subMod->SetRequiredProjectName(tempRequiredProjectName);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(requiredProjectNameTooltip.data());
				} else if (!a_subMod->GetRequiredProjectName().empty()) {
					if (ImGui::BeginTable(requiredProjectNameId.data(), 1, ImGuiTableFlags_BordersOuter, ImVec2(tableWidth, 0))) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						UICommon::TextDescriptionRightAligned("Required project name");
						ImGui::TextUnformatted(a_subMod->GetRequiredProjectName().data());
						ImGui::EndTable();
					}
					ImGui::SameLine();
					UICommon::HelpMarker(requiredProjectNameTooltip.data());
				}
			}

			// Submod ignore no triggers flag
			{
				std::string noTriggersFlagLabel = "Ignore \"Don't Convert Annotations To Triggers\" flag##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "ignoreDontConvertAnnotationsToTriggersFlag";
				std::string noTriggersFlagTooltip = "If checked, the animation clip flag \"Don't convert annotations to triggers\", set on some animation clips in vanilla, will be ignored. This means that animation triggers (events) that included in the replacer animation file itself as annotations will now run, instead of being ignored.";

				if (_editMode != EditMode::kNone) {
					bool tempIgnoreNoTriggersFlag = a_subMod->IsIgnoringDontConvertAnnotationsToTriggersFlag();
					if (ImGui::Checkbox(noTriggersFlagLabel.data(), &tempIgnoreNoTriggersFlag)) {
						a_subMod->SetIgnoringDontConvertAnnotationsToTriggersFlag(tempIgnoreNoTriggersFlag);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(noTriggersFlagTooltip.data());
				} else if (a_subMod->IsIgnoringDontConvertAnnotationsToTriggersFlag()) {
					ImGui::BeginDisabled();
					bool tempIgnoreNoTriggersFlag = a_subMod->IsIgnoringDontConvertAnnotationsToTriggersFlag();
					ImGui::Checkbox(noTriggersFlagLabel.data(), &tempIgnoreNoTriggersFlag);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(noTriggersFlagTooltip.data());
				}
			}

			// Submod triggers from annotations only
			{
				std::string triggersFromAnnotationsOnlyLabel = "Only use triggers from annotations##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "triggersFromAnnotationsOnly";
				std::string triggersFromAnnotationsOnlyTooltip = "If checked, the triggers \"baked in\" in the animation clips inside the behavior files will be ignored. The only events that will will be those from annotations inside the animation file.\nThe \"Don't convert annotations to triggers\" flag is still respected, so make sure to enable the above setting if necessary.";

				if (_editMode != EditMode::kNone) {
					bool tempTriggersFromAnnotationsOnly = a_subMod->IsOnlyUsingTriggersFromAnnotations();
					if (ImGui::Checkbox(triggersFromAnnotationsOnlyLabel.data(), &tempTriggersFromAnnotationsOnly)) {
						a_subMod->SetOnlyUsingTriggersFromAnnotations(tempTriggersFromAnnotationsOnly);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(triggersFromAnnotationsOnlyTooltip.data());
				} else if (a_subMod->IsIgnoringDontConvertAnnotationsToTriggersFlag()) {
					ImGui::BeginDisabled();
					bool tempTriggersFromAnnotationsOnly = a_subMod->IsOnlyUsingTriggersFromAnnotations();
					ImGui::Checkbox(triggersFromAnnotationsOnlyLabel.data(), &tempTriggersFromAnnotationsOnly);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(triggersFromAnnotationsOnlyTooltip.data());
				}
			}

			// Submod interruptible
			{
				std::string interruptibleLabel = "Interruptible##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "interruptible";
				std::string interruptibleTooltip = "If checked, the conditions will be checked every frame and the clip will be switched to another one if needed. Mostly useful for looping animations.";

				if (_editMode != EditMode::kNone) {
					bool tempInterruptible = a_subMod->IsInterruptible();
					if (ImGui::Checkbox(interruptibleLabel.data(), &tempInterruptible)) {
						a_subMod->SetInterruptible(tempInterruptible);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(interruptibleTooltip.data());
				} else if (a_subMod->IsInterruptible()) {
					ImGui::BeginDisabled();
					bool tempInterruptible = a_subMod->IsInterruptible();
					ImGui::Checkbox(interruptibleLabel.data(), &tempInterruptible);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(interruptibleTooltip.data());
				}
			}

			const auto drawBlendTimeOption = [this, a_subMod](CustomBlendType a_type, std::string_view a_boolLabel, std::string_view a_sliderLabel, std::string_view a_tooltip) {
				if (_editMode != EditMode::kNone) {
					bool tempHasCustomBlendTime = a_subMod->HasCustomBlendTime(a_type);
					if (ImGui::Checkbox(a_boolLabel.data(), &tempHasCustomBlendTime)) {
						a_subMod->ToggleCustomBlendTime(a_type, tempHasCustomBlendTime);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					ImGui::BeginDisabled(!a_subMod->HasCustomBlendTime(a_type));
					float tempBlendTime = a_subMod->GetCustomBlendTime(a_type);
					ImGui::SetNextItemWidth(200.f);
					if (ImGui::SliderFloat(a_sliderLabel.data(), &tempBlendTime, 0.f, 1.f, "%.2f s", ImGuiSliderFlags_AlwaysClamp)) {
						a_subMod->SetCustomBlendTime(a_type, tempBlendTime);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(a_tooltip.data());
				} else if (a_subMod->HasCustomBlendTime(a_type)) {
					ImGui::BeginDisabled();
					float tempBlendTime = a_subMod->GetCustomBlendTime(a_type);
					ImGui::SetNextItemWidth(200.f);
					ImGui::SliderFloat(a_sliderLabel.data(), &tempBlendTime, 0.f, 1.f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(a_tooltip.data());
				}
			};

			// Submod custom blend time on interrupt
			if (a_subMod->IsInterruptible()) {
				const std::string hasCustomBlendTimeLabel = "##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "hasCustomBlendTimeOnInterrupt";
				const std::string blendTimeLabel = "Custom blend time on interrupt##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "blendTimeOnInterrupt";
				const std::string blendTimeTooltip = "Sets custom blend time between an animation from this submod and a new one on interrupt.";
				drawBlendTimeOption(CustomBlendType::kInterrupt, hasCustomBlendTimeLabel, blendTimeLabel, blendTimeTooltip);
			}

			// Submod replace on loop
			{
				std::string replaceOnLoopLabel = "Replace on loop##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "replaceOnLoop";
				std::string replaceOnLoopTooltip = "If checked the conditions will be reevaluated on animation loop and the clip will be switched to another one if needed. Enabled by default.";

				if (_editMode != EditMode::kNone) {
					bool tempReplaceOnLoop = a_subMod->IsReevaluatingOnLoop();
					if (ImGui::Checkbox(replaceOnLoopLabel.data(), &tempReplaceOnLoop)) {
						a_subMod->SetReevaluatingOnLoop(tempReplaceOnLoop);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(replaceOnLoopTooltip.data());
				} else if (!a_subMod->IsReevaluatingOnLoop()) {
					ImGui::BeginDisabled();
					bool tempReplaceOnLoop = a_subMod->IsReevaluatingOnLoop();
					ImGui::Checkbox(replaceOnLoopLabel.data(), &tempReplaceOnLoop);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(replaceOnLoopTooltip.data());
				}
			}

			// Submod custom blend time on loop
			if (a_subMod->IsReevaluatingOnLoop()) {
				const std::string hasCustomBlendTimeLabel = "##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "hasCustomBlendTimeOnLoop";
				const std::string blendTimeLabel = "Custom blend time on loop##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "blendTimeOnLoop";
				const std::string blendTimeTooltip = "Sets custom blend time between an animation from this submod and a new one when replacing on loop.";
				drawBlendTimeOption(CustomBlendType::kLoop, hasCustomBlendTimeLabel, blendTimeLabel, blendTimeTooltip);
			}

			// Submod replace on echo
			{
				std::string replaceOnEchoLabel = "Replace on echo##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "replaceOnEcho";
				std::string replaceOnEchoTooltip = "If checked the conditions will be reevaluated on animation echo and the clip will be switched to another one if needed. Disabled by default because of cosmetic issues with several animations, should be only enabled on animations that actually need it.";

				if (_editMode != EditMode::kNone) {
					bool tempReplaceOnEcho = a_subMod->IsReevaluatingOnEcho();
					if (ImGui::Checkbox(replaceOnEchoLabel.data(), &tempReplaceOnEcho)) {
						a_subMod->SetReevaluatingOnEcho(tempReplaceOnEcho);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(replaceOnEchoTooltip.data());
				} else if (a_subMod->IsReevaluatingOnEcho()) {
					ImGui::BeginDisabled();
					bool tempReplaceOnEcho = a_subMod->IsReevaluatingOnEcho();
					ImGui::Checkbox(replaceOnEchoLabel.data(), &tempReplaceOnEcho);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(replaceOnEchoTooltip.data());
				}
			}

			// Submod custom blend time on echo
			if (a_subMod->IsReevaluatingOnEcho()) {
				const std::string hasCustomBlendTimeLabel = "##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "hasCustomBlendTimeOnEcho";
				const std::string blendTimeLabel = "Custom blend time on echo##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "blendTimeOnEcho";
				const std::string blendTimeTooltip = "Sets custom blend time between an animation from this submod and a new one when replacing on echo.";
				drawBlendTimeOption(CustomBlendType::kEcho, hasCustomBlendTimeLabel, blendTimeLabel, blendTimeTooltip);
			}

			// Submod run functions on loop
			{
				std::string runFunctionsOnLoopLabel = "Run functions on loop##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "runFunctionsOnLoop";
				std::string runFunctionsOnLoopTooltip = "If checked the OnDeactivate/OnActivate functions will run when the clip loops. Enabled by default.";

				if (_editMode != EditMode::kNone) {
					bool tempRunFunctionsOnLoop = a_subMod->IsRunningFunctionsOnLoop();
					if (ImGui::Checkbox(runFunctionsOnLoopLabel.data(), &tempRunFunctionsOnLoop)) {
						a_subMod->SetRunningFunctionsOnLoop(tempRunFunctionsOnLoop);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(runFunctionsOnLoopTooltip.data());
				} else if (!a_subMod->IsRunningFunctionsOnLoop()) {
					ImGui::BeginDisabled();
					bool tempRunFunctionsOnLoop = a_subMod->IsRunningFunctionsOnLoop();
					ImGui::Checkbox(runFunctionsOnLoopLabel.data(), &tempRunFunctionsOnLoop);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(runFunctionsOnLoopTooltip.data());
				}
			}

			// Submod run functions on echo
			{
				std::string runFunctionsOnEchoLabel = "Run functions on echo##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "runFunctionsOnEcho";
				std::string runFunctionsOnEchoTooltip = "If checked the OnDeactivate/OnActivate functions will run on clip echo. Enabled by default.";

				if (_editMode != EditMode::kNone) {
					bool tempRunFunctionsOnEcho = a_subMod->IsRunningFunctionsOnEcho();
					if (ImGui::Checkbox(runFunctionsOnEchoLabel.data(), &tempRunFunctionsOnEcho)) {
						a_subMod->SetRunningFunctionsOnEcho(tempRunFunctionsOnEcho);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(runFunctionsOnEchoTooltip.data());
				} else if (!a_subMod->IsRunningFunctionsOnEcho()) {
					ImGui::BeginDisabled();
					bool tempRunFunctionsOnEcho = a_subMod->IsRunningFunctionsOnEcho();
					ImGui::Checkbox(runFunctionsOnEchoLabel.data(), &tempRunFunctionsOnEcho);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(runFunctionsOnEchoTooltip.data());
				}
			}

			// Submod animations
			{
				// list animation files
				std::string filesTreeNodeLabel = "Animation Files##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "filesNode";
				if (ImGui::CollapsingHeader(filesTreeNodeLabel.data())) {
					ImGui::AlignTextToFramePadding();
					UICommon::TextUnformattedWrapped("This section only lists all the animation files found in the submod. The \"Replacement Animations\" section below lists actual loaded replacement animations per behavior project, and allows configuration.");

					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

					std::string filesTableId = std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "filesTable";
					if (ImGui::BeginTable(filesTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
						const std::filesystem::path submodFullPath = a_subMod->GetPath();
						a_subMod->ForEachReplacementAnimationFile([&](const ReplacementAnimationFile& a_file) {
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();

							const bool bHasVariants = a_file.variants.has_value();

							const std::filesystem::path fileFullPath = a_file.fullPath;
							const std::filesystem::path relativePath = fileFullPath.lexically_relative(submodFullPath);

							UICommon::TextUnformattedEllipsisShort(a_file.fullPath.data(), relativePath.string().data(), nullptr, ImGui::GetContentRegionAvail().x);
							if (bHasVariants) {
								for (const auto& variant : *a_file.variants) {
									ImGui::TableNextRow();
									ImGui::TableSetColumnIndex(0);
									ImGui::AlignTextToFramePadding();

									const std::filesystem::path variantFullPath = variant.fullPath;
									const std::filesystem::path variantRelativePath = variantFullPath.lexically_relative(submodFullPath);

									ImGui::Indent();
									UICommon::TextUnformattedEllipsisShort(variant.fullPath.data(), variantRelativePath.string().data(), nullptr, ImGui::GetContentRegionAvail().x);
									ImGui::Unindent();
								}
							}
						});
						ImGui::EndTable();
					}
					ImGui::PopStyleVar();
				}

				std::string animationsTreeNodeLabel = "Replacement Animations##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "animationsNode";
				if (ImGui::CollapsingHeader(animationsTreeNodeLabel.data())) {
					UnloadedAnimationsWarning();

					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

					std::string animationsTableId = std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "animationsTable";
					if (ImGui::BeginTable(animationsTableId.data(), 1, ImGuiTableFlags_Borders)) {
						const std::filesystem::path submodFullPath = a_subMod->GetPath();
						a_subMod->ForEachReplacementAnimation([&](ReplacementAnimation* a_replacementAnimation) {
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();

							const bool bAnimationDisabled = a_replacementAnimation->IsDisabled();
							if (bAnimationDisabled) {
								auto& style = ImGui::GetStyle();
								ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.DisabledAlpha);
							}

							if (_editMode != EditMode::kNone) {
								const std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_replacementAnimation));
								ImGui::PushID(idString.data());
								bool bEnabled = !a_replacementAnimation->GetDisabled();
								if (ImGui::Checkbox("##disableReplacementAnimation", &bEnabled)) {
									a_replacementAnimation->SetDisabled(!bEnabled);
									OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
									a_subMod->SetDirty(true);
								}
								UICommon::AddTooltip("If unchecked, the replacement animation will be disabled and will not be considered.");
								ImGui::PopID();
								ImGui::SameLine();
							}

							const bool bHasVariants = a_replacementAnimation->HasVariants();

							const auto refrToEvaluate = UIManager::GetSingleton().GetRefrToEvaluate();
							const bool bCanPreview = CanPreviewAnimation(refrToEvaluate, a_replacementAnimation);
							bool bIsPreviewing = IsPreviewingAnimation(refrToEvaluate, a_replacementAnimation);

							UICommon::TextUnformattedDisabled(std::format("[{}]", a_replacementAnimation->GetProjectName()).data());
							ImGui::SameLine();

							float previewButtonWidth = 0.f;
							if (bCanPreview || bIsPreviewing) {
								previewButtonWidth = GetPreviewButtonsWidth(a_replacementAnimation, bIsPreviewing);
							}

							const std::filesystem::path fullPath = a_replacementAnimation->GetAnimPath();
							const std::filesystem::path relativePath = fullPath.lexically_relative(submodFullPath);

							UICommon::TextUnformattedEllipsisShort(a_replacementAnimation->GetAnimPath().data(), relativePath.string().data(), nullptr, ImGui::GetContentRegionAvail().x - previewButtonWidth);

							// preview button(s)
							if (!bHasVariants) {  // don't draw for variants. each variant gets its own button
								DrawPreviewButtons(refrToEvaluate, a_replacementAnimation, previewButtonWidth, bCanPreview, bIsPreviewing);
							}

							// variants
							if (bHasVariants) {
								auto& variants = a_replacementAnimation->GetVariants();
								auto variantScope = variants.GetVariantStateScope();
								std::string variantsTableId = std::to_string(reinterpret_cast<std::uintptr_t>(&variants)) + "variantsTable";
								if (variantScope > Conditions::StateDataScope::kLocal) {
									ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, UICommon::CONDITION_SHARED_STATE_BORDER_COLOR);
								}
								if (ImGui::BeginTable(variantsTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
									ImGui::TableNextRow();
									ImGui::TableSetColumnIndex(0);
									ImGui::AlignTextToFramePadding();

									auto variantMode = variants.GetVariantMode();
									if (!a_replacementAnimation->IsSynchronizedAnimation()) {
										// variant mode
										if (_editMode != EditMode::kNone) {
											const std::string label = "Variant Mode##" + std::to_string(reinterpret_cast<std::uintptr_t>(&variants)) + "variantMode";
											const std::string current = variantMode == VariantMode::kRandom ? "Random" : "Sequential";
											int tempVariantMode = static_cast<int>(variantMode);
											ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15);
											if (ImGui::SliderInt(label.data(), &tempVariantMode, 0, 1, current.data(), ImGuiSliderFlags_NoInput)) {
												variantMode = static_cast<VariantMode>(tempVariantMode);
												variants.SetVariantMode(variantMode);
												a_replacementAnimation->UpdateVariantCache();
												a_subMod->SetDirty(true);
												OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
											}
										} else {
											UICommon::TextUnformattedDisabled("Variant Mode:");
											ImGui::SameLine();
											ImGui::TextUnformatted(variantMode == VariantMode::kRandom ? "Random" : "Sequential");
										}
									}

									// variant scope
									{
										auto getScopeName = [](const Conditions::StateDataScope a_scope) {
											switch (a_scope) {
											case Conditions::StateDataScope::kLocal:
												return "Local"sv;
											case Conditions::StateDataScope::kSubMod:
												return "Submod"sv;
											case Conditions::StateDataScope::kReplacerMod:
												return "Replacer mod"sv;
											}
											return "INVALID"sv;
										};

										auto getScopeTooltip = [](const Conditions::StateDataScope a_scope) {
											switch (a_scope) {
											case Conditions::StateDataScope::kLocal:
												return "The variant data (random value, played history) is unique per active animation clip."sv;
											case Conditions::StateDataScope::kSubMod:
												return "The variant data (random value, optionally played history) is shared between all animation clips in the submod, as long as their variant scope is set to the same value.\n\nThe data will be kept alive until all active clips from narrower scopes are inactive."sv;
											case Conditions::StateDataScope::kReplacerMod:
												return "The variant data (random value, optionally played history) is shared between all animation clips in the entire replacer mod, as long as their variant scope is set to the same value.\n\nThe data will be kept alive until all active clips from narrower scopes are inactive."sv;
											}
											return "INVALID"sv;
										};

										if (_editMode != EditMode::kNone) {
											const std::string variantScopeLabel = "Variant state scope##" + std::to_string(reinterpret_cast<std::uintptr_t>(&variants));
											if (ImGui::BeginCombo(variantScopeLabel.data(), getScopeName(variantScope).data())) {
												for (Conditions::StateDataScope i = Conditions::StateDataScope::kLocal; i <= Conditions::StateDataScope::kReplacerMod; i = static_cast<Conditions::StateDataScope>(static_cast<int32_t>(i) << 1)) {
													const bool bIsCurrent = i == variantScope;
													if (ImGui::Selectable(getScopeName(i).data(), bIsCurrent)) {
														if (!bIsCurrent) {
															variants.SetVariantStateScope(i);
															a_subMod->SetDirty(true);
															OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
														}
													}
													if (bIsCurrent) {
														ImGui::SetItemDefaultFocus();
													}
													UICommon::AddTooltip(getScopeTooltip(i).data());
												}
												ImGui::EndCombo();
											}
										} else {
											const auto scopeText = std::format("Variant state scope: {}", getScopeName(variantScope));
											ImGui::TextUnformatted(scopeText.data());
											UICommon::AddTooltip(getScopeTooltip(variantScope).data());
										}
									}

									// blend between variants
									{
										bool tempShouldBlend = variants.ShouldBlendBetweenVariants();
										const std::string shouldBlendLabel = "Blend between variants##" + std::to_string(reinterpret_cast<std::uintptr_t>(&variants));
										ImGui::BeginDisabled(_editMode == EditMode::kNone);
										if (ImGui::Checkbox(shouldBlendLabel.data(), &tempShouldBlend)) {
											variants.SetShouldBlendBetweenVariants(tempShouldBlend);
											a_subMod->SetDirty(true);
										}
										ImGui::EndDisabled();
										ImGui::SameLine();
										UICommon::HelpMarker("If disabled, variants will not have any blend time between each other on loop and echo.");
									}

									// reset random on loop / echo
									{
										bool tempShouldResetRandom = variants.ShouldResetRandomOnLoopOrEcho();
										const std::string shouldResetRandomLabel = "Reset random on loop or echo##" + std::to_string(reinterpret_cast<std::uintptr_t>(&variants));
										ImGui::BeginDisabled(_editMode == EditMode::kNone);
										if (ImGui::Checkbox(shouldResetRandomLabel.data(), &tempShouldResetRandom)) {
											variants.SetShouldResetRandomOnLoopOrEcho(tempShouldResetRandom);
											a_subMod->SetDirty(true);
										}
										ImGui::EndDisabled();
										ImGui::SameLine();
										UICommon::HelpMarker("If enabled, the random number that is used to select a random variant will be reset on every loop or echo.");
									}

									// share played history
									{
										if (variants.GetVariantStateScope() > Conditions::StateDataScope::kLocal) {
											bool tempShouldSharePlayedHistory = variants.ShouldSharePlayedHistory();
											const std::string shouldSharePlayedHistoryLabel = "Share played history##" + std::to_string(reinterpret_cast<std::uintptr_t>(&variants));
											ImGui::BeginDisabled(_editMode == EditMode::kNone);
											if (ImGui::Checkbox(shouldSharePlayedHistoryLabel.data(), &tempShouldSharePlayedHistory)) {
												variants.SetShouldSharePlayedHistory(tempShouldSharePlayedHistory);
												a_subMod->SetDirty(true);
												OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
											}
											ImGui::EndDisabled();
											ImGui::SameLine();
											UICommon::HelpMarker("If enabled, the played history (for the \"Play once\" setting) will be shared between all variants with the same variant state scope.");
										}
									}

									// variant list
									int32_t index = 0;
									a_replacementAnimation->ForEachVariant([&](Variant& a_variant) {
										const bool bVariantDisabled = a_variant.IsDisabled();
										if (bVariantDisabled) {
											const auto& style = ImGui::GetStyle();
											ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.DisabledAlpha);
										}

										ImGui::TableNextRow();
										ImGui::TableSetColumnIndex(0);
										ImGui::AlignTextToFramePadding();

										ImGui::Indent();
										if (_editMode != EditMode::kNone) {
											ImGui::PushID(&a_variant);
											bool bEnabled = !a_variant.IsDisabled();
											if (ImGui::Checkbox("##disableVariant", &bEnabled)) {
												a_variant.SetDisabled(!bEnabled);
												a_replacementAnimation->UpdateVariantCache();
												a_subMod->SetDirty(true);
												OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
											}
											UICommon::AddTooltip("If unchecked, the replacement animation variant will be disabled and will not be considered.");
											ImGui::SameLine();

											if (variantMode == VariantMode::kSequential || a_variant.ShouldPlayOnce()) {
												ImGui::BeginDisabled(index == 0);
												if (ImGui::ArrowButton("Move variant up", ImGuiDir_Up)) {
													variants.SwapVariants(index, index - 1);
													a_replacementAnimation->UpdateVariantCache();
													a_subMod->SetDirty(true);
													OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
												}
												ImGui::EndDisabled();
												ImGui::SameLine();

												ImGui::BeginDisabled(index >= variants.GetVariantCount() - 1);
												if (ImGui::ArrowButton("Move variant down", ImGuiDir_Down)) {
													variants.SwapVariants(index, index + 1);
													a_replacementAnimation->UpdateVariantCache();
													a_subMod->SetDirty(true);
													OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
												}
												ImGui::EndDisabled();
												ImGui::SameLine();
											} else if (variantMode == VariantMode::kRandom) {
												float tempWeight = a_variant.GetWeight();
												ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
												if (ImGui::InputFloat("Weight", &tempWeight, .01f, 1.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) {
													tempWeight = std::max(0.f, tempWeight);
													a_variant.SetWeight(tempWeight);
													a_replacementAnimation->UpdateVariantCache();
													a_subMod->SetDirty(true);
													OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
												}
												UICommon::AddTooltip("The weight of this variant used for the weighted random selection (e.g. a variant with a weight of 2 will be twice as likely to be picked than a variant with a weight of 1)");
												ImGui::SameLine();
											}

											bool tempPlayOnce = a_variant.ShouldPlayOnce();
											if (ImGui::Checkbox(variantMode == VariantMode::kRandom ? "Play first and once" : "Play once", &tempPlayOnce)) {
												a_variant.SetPlayOnce(tempPlayOnce);
												a_replacementAnimation->UpdateVariantCache();
												a_subMod->SetDirty(true);
												OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
											}
											UICommon::AddTooltip(variantMode == VariantMode::kRandom ? "Variants marked with this will play in sequence before other random variants, and will not repeat, until the animation data resets after a while of inactivity." : "If checked, the variant will only play once until the animation data resets after a while of inactivity.");
											ImGui::SameLine();
											ImGui::PopID();
										} else {
											switch (variantMode) {
											case VariantMode::kRandom:
												{
													UICommon::TextUnformattedDisabled("Weight:");
													ImGui::SameLine();
													ImGui::TextUnformatted(std::format("{}", a_variant.GetWeight()).data());
													UICommon::AddTooltip("The weight of this variant used for the weighted random selection (e.g. a variant with a weight of 2 will be twice as likely to be picked than a variant with a weight of 1)");
													ImGui::SameLine();
												}
												break;
											case VariantMode::kSequential:
												{
													if (a_variant.ShouldPlayOnce()) {
														ImGui::TextUnformatted("[Play once]");
														UICommon::AddTooltip("The variant will only play once until the animation data resets after a while of inactivity.");
														ImGui::SameLine();
													}
												}
												break;
											}
										}

										UICommon::TextUnformattedDisabled("Filename:");
										ImGui::SameLine();

										bIsPreviewing = IsPreviewingAnimation(refrToEvaluate, a_replacementAnimation, a_variant.GetIndex());

										float variantPreviewButtonWidth = 0.f;
										if (bCanPreview || bIsPreviewing) {
											variantPreviewButtonWidth = GetPreviewButtonsWidth(a_replacementAnimation, bIsPreviewing);
										}

										std::filesystem::path fullVariantPath = a_replacementAnimation->GetAnimPath();
										fullVariantPath /= a_variant.GetFilename();

										UICommon::TextUnformattedEllipsisShort(fullVariantPath.string().data(), a_variant.GetFilename().data(), nullptr, ImGui::GetContentRegionAvail().x - variantPreviewButtonWidth);

										// preview variant button
										DrawPreviewButtons(refrToEvaluate, a_replacementAnimation, variantPreviewButtonWidth, bCanPreview, bIsPreviewing, &a_variant);

										ImGui::Unindent();

										if (bVariantDisabled) {
											ImGui::PopStyleVar();
										}

										++index;

										return RE::BSVisit::BSVisitControl::kContinue;
									});
									ImGui::EndTable();
								}
								if (variantScope > Conditions::StateDataScope::kLocal) {
									ImGui::PopStyleColor();
								}
							}

							if (bAnimationDisabled) {
								ImGui::PopStyleVar();
							}
						});
						ImGui::EndTable();
					}

					ImGui::PopStyleVar();
				}
			}

			// Submod conditions
			std::string conditionsTreeNodeLabel = "Conditions##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "conditionsNode";
			if (ImGui::CollapsingHeader(conditionsTreeNodeLabel.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

				ImGui::Indent();
				const ImGuiStyle& style = ImGui::GetStyle();
				ImVec2 pos = ImGui::GetCursorScreenPos();
				pos.x += style.FramePadding.x;
				pos.y += style.FramePadding.y;
				ImGui::PushID(a_subMod->GetConditionSet());
				DrawConditionSet(a_subMod->GetConditionSet(), a_subMod, _editMode, Conditions::ConditionType::kNormal, UIManager::GetSingleton().GetRefrToEvaluate(), true, pos);
				ImGui::PopID();
				ImGui::Unindent();

				ImGui::Spacing();
				ImGui::PopStyleVar();
			}

			// Submod paired conditions
			if (a_subMod->HasSynchronizedAnimations()) {
				std::string pairedConditionsTreeNodeLabel = "Paired Conditions##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "pairedConditionsNode";
				if (ImGui::CollapsingHeader(pairedConditionsTreeNodeLabel.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

					ImGui::Indent();
					const ImGuiStyle& style = ImGui::GetStyle();
					ImVec2 pos = ImGui::GetCursorScreenPos();
					pos.x += style.FramePadding.x;
					pos.y += style.FramePadding.y;
					ImGui::PushID(a_subMod->GetSynchronizedConditionSet());
					DrawConditionSet(a_subMod->GetSynchronizedConditionSet(), a_subMod, _editMode, Conditions::ConditionType::kNormal, UIManager::GetSingleton().GetRefrToEvaluate(), true, pos);
					ImGui::PopID();
					ImGui::Unindent();

					ImGui::Spacing();
					ImGui::PopStyleVar();
				}
			}

			// Submod functions
			auto drawFunctionSet = [&](Functions::FunctionSetType a_functionSetType) {
				std::string functionsTreeNodeLabel;
				std::string helpMarkerText;
				switch (a_functionSetType) {
				case Functions::FunctionSetType::kOnActivate:
					functionsTreeNodeLabel = "On Activate##";
					helpMarkerText = "Functions from this set will run when an animation from this submod starts.";
					break;
				case Functions::FunctionSetType::kOnDeactivate:
					functionsTreeNodeLabel = "On Deactivate##";
					helpMarkerText = "Functions from this set will run when an animation from this submod ends.";
					break;
				case Functions::FunctionSetType::kOnTrigger:
					functionsTreeNodeLabel = "On Trigger##";
					helpMarkerText = "Functions from this set will run when a specified animation event is called while an animation from this submod is playing.";
					break;
				}

				functionsTreeNodeLabel += std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "functionsNode";
				bool bIsExpanded = ImGui::CollapsingHeader(functionsTreeNodeLabel.data());
				ImGui::SameLine();
				UICommon::HelpMarker(helpMarkerText.data());
				if (bIsExpanded) {
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

					ImGui::Indent();
					const ImGuiStyle& style = ImGui::GetStyle();
					ImVec2 pos = ImGui::GetCursorScreenPos();
					pos.x += style.FramePadding.x;
					pos.y += style.FramePadding.y;
					ImGui::PushID(a_subMod->GetFunctionSet(a_functionSetType));
					DrawFunctionSet(a_subMod->GetFunctionSet(a_functionSetType), a_subMod, _editMode, a_functionSetType, UIManager::GetSingleton().GetRefrToEvaluate(), true, pos);
					ImGui::PopID();
					ImGui::Unindent();

					ImGui::Spacing();
					ImGui::PopStyleVar();
				}
			};

			if (_editMode != EditMode::kNone || a_subMod->HasAnyFunctionSet()) {
				std::string allFunctionsTreeNodeLabel = "Functions##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "functionsNode";
				if (ImGui::CollapsingHeader(allFunctionsTreeNodeLabel.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::Indent();
					if (_editMode != EditMode::kNone || a_subMod->HasFunctionSet(Functions::FunctionSetType::kOnActivate)) {
						drawFunctionSet(Functions::FunctionSetType::kOnActivate);
					}
					if (_editMode != EditMode::kNone || a_subMod->HasFunctionSet(Functions::FunctionSetType::kOnDeactivate)) {
						drawFunctionSet(Functions::FunctionSetType::kOnDeactivate);
					}
					if (_editMode != EditMode::kNone || a_subMod->HasFunctionSet(Functions::FunctionSetType::kOnTrigger)) {
						drawFunctionSet(Functions::FunctionSetType::kOnTrigger);
					}
					ImGui::Unindent();
				}
			}

			if (_editMode != EditMode::kNone) {
				if (_editMode == EditMode::kAuthor && a_replacerMod->IsLegacy()) {
					// Save migration config
					ImGui::BeginDisabled(a_subMod->HasInvalidConditions() || a_subMod->HasInvalidFunctions());
					UICommon::ButtonWithConfirmationModal("Save author submod config for migration", "This author config won't be read because this is a legacy mod.\nThe functionality is here for convenience only.\nYou can copy the resulting file to the proper folder when migrating your mods to the new structure.\n\n"sv, [&]() {
						a_subMod->SaveConfig(_editMode, false);
					});
					ImGui::EndDisabled();
				} else {
					// Save submod config
					bool bIsDirty = a_subMod->IsDirty();
					if (!bIsDirty) {
						ImGui::BeginDisabled();
					}
					ImGui::BeginDisabled(a_subMod->HasInvalidConditions() || a_subMod->HasInvalidFunctions());
					if (ImGui::Button(_editMode == EditMode::kAuthor ? "Save submod config (Author)" : "Save submod config (User)")) {
						a_subMod->SaveConfig(_editMode);
					}
					ImGui::EndDisabled();
					if (!bIsDirty) {
						ImGui::EndDisabled();
					}
				}

				// Reload submod config
				ImGui::SameLine();
				UICommon::ButtonWithConfirmationModal("Reload submod config", "Are you sure you want to reload the config?\nThis operation cannot be undone!\n\n"sv, [&]() {
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReloadSubModConfigJob>(a_subMod);
				});

				// delete user config
				const bool bUserConfigExists = Utils::DoesUserConfigExist(a_subMod->GetPath());
				if (!bUserConfigExists) {
					ImGui::BeginDisabled();
				}
				ImGui::SameLine();
				UICommon::ButtonWithConfirmationModal("Delete submod user config", "Are you sure you want to delete the user config?\nThis operation cannot be undone!\n\n"sv, [&]() {
					Utils::DeleteUserConfig(a_subMod->GetPath());
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReloadSubModConfigJob>(a_subMod);
				});
				if (!bUserConfigExists) {
					ImGui::EndDisabled();
				}

				// copy json to clipboard button
				if (a_replacerMod->IsLegacy() && _editMode == EditMode::kAuthor) {
					ImGui::SameLine();
					ImGui::BeginDisabled(a_subMod->HasInvalidConditions() || a_subMod->HasInvalidFunctions());
					if (ImGui::Button("Copy submod config to clipboard")) {
						ImGui::LogToClipboard();
						ImGui::LogText(a_subMod->SerializeToString().data());
						ImGui::LogFinish();
					}
					ImGui::EndDisabled();
				}
			}

			ImGui::Spacing();

			ImGui::TreePop();
		}
	}

	void UIMain::DrawReplacementAnimations()
	{
		static char animPathFilterBuf[32] = "";
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 18);
		ImGui::InputTextWithHint("Filter", "Animation name...", animPathFilterBuf, IM_ARRAYSIZE(animPathFilterBuf));
		ImGui::SameLine();
		UICommon::HelpMarker("Type a part of an animation path to filter the list of replacement animations.");

		ImGui::Separator();

		UnloadedAnimationsWarning();

		ImGui::Spacing();

		if (ImGui::BeginChild("Datas")) {
			OpenAnimationReplacer::GetSingleton().ForEachReplacerProjectData([&](RE::hkbCharacterStringData* a_stringData, const auto& a_projectData) {
				const auto& name = a_stringData->name;

				ImGui::PushID(a_stringData);
				if (ImGui::TreeNode(name.data())) {
					auto animCount = a_stringData->animationNames.size();
					auto totalCount = a_projectData->projectDBData->bindings->bindings.size();
					const float animPercent = static_cast<float>(totalCount) / static_cast<float>(Settings::uAnimationLimit);
					const std::string animPercentStr = std::format("{} ({} + {}) / {}", totalCount, animCount, totalCount - animCount, Settings::uAnimationLimit);
					ImGui::ProgressBar(animPercent, ImVec2(0.f, 0.f), animPercentStr.data());

					auto& map = a_projectData->originalIndexToAnimationReplacementsMap;

					std::vector<AnimationReplacements*> sortedReplacements;
					sortedReplacements.reserve(map.size());

					for (auto& [index, animReplacements] : map) {
						// Filter
						if (std::strlen(animPathFilterBuf) && !Utils::ContainsStringIgnoreCase(animReplacements->GetOriginalPath(), animPathFilterBuf)) {
							continue;
						}

						auto it = std::lower_bound(sortedReplacements.begin(), sortedReplacements.end(), animReplacements, [](const auto& a_lhs, const auto& a_rhs) {
							return a_lhs->GetOriginalPath() < a_rhs->GetOriginalPath();
						});
						sortedReplacements.insert(it, animReplacements.get());
					}

					for (auto& animReplacements : sortedReplacements) {
						ImGui::PushID(animReplacements);
						if (ImGui::TreeNode(animReplacements->GetOriginalPath().data())) {
							ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
							if (ImGui::BeginTable(animReplacements->GetOriginalPath().data(), 1, ImGuiTableFlags_BordersOuter)) {
								animReplacements->ForEachReplacementAnimation([this](auto animation) { DrawReplacementAnimation(animation); });

								ImGui::EndTable();
							}
							ImGui::PopStyleVar();
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			});
		}
		ImGui::EndChild();
	}

	void UIMain::DrawReplacementAnimation(ReplacementAnimation* a_replacementAnimation)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		// Evaluate
		auto evalResult = ConditionEvaluateResult::kFailure;
		const auto refrToEvaluate = UIManager::GetSingleton().GetRefrToEvaluate();
		if (refrToEvaluate) {
			if (Utils::ConditionSetHasRandomResult(a_replacementAnimation->GetConditionSet())) {
				evalResult = ConditionEvaluateResult::kRandom;
			} else if (a_replacementAnimation->EvaluateConditions(refrToEvaluate, nullptr)) {
				evalResult = ConditionEvaluateResult::kSuccess;
			}
		}

		const std::string nodeName = std::format("Priority: {}##{}", std::to_string(a_replacementAnimation->GetPriority()), reinterpret_cast<uintptr_t>(a_replacementAnimation));

		const bool bAnimationDisabled = a_replacementAnimation->IsDisabled();
		if (bAnimationDisabled) {
			auto& style = ImGui::GetStyle();
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.DisabledAlpha);
		}

		ImGui::PushID(&a_replacementAnimation);
		const bool bNodeOpen = ImGui::TreeNodeEx(nodeName.data(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);

		if (const auto parentSubMod = a_replacementAnimation->GetParentSubMod()) {
			if (parentSubMod->IsFromLegacyConfig()) {
				ImGui::SameLine();
				UICommon::TextUnformattedDisabled("Legacy");
			} else if (const auto parentMod = parentSubMod->GetParentMod()) {
				ImGui::SameLine();
				UICommon::TextUnformattedDisabled(parentMod->GetName().data());
				ImGui::SameLine();
				UICommon::TextUnformattedDisabled("-");
				ImGui::SameLine();
				UICommon::TextUnformattedDisabled(parentSubMod->GetName().data());
			}
		}

		// Evaluate success/failure indicator
		if (refrToEvaluate) {
			UICommon::DrawConditionEvaluateResult(evalResult);
		}

		if (bNodeOpen) {
			const bool bCanPreview = CanPreviewAnimation(refrToEvaluate, a_replacementAnimation);
			bool bIsPreviewing = IsPreviewingAnimation(refrToEvaluate, a_replacementAnimation);

			const auto animPath = a_replacementAnimation->GetAnimPath();

			float previewButtonWidth = 0.f;
			if (bCanPreview || bIsPreviewing) {
				previewButtonWidth = GetPreviewButtonsWidth(a_replacementAnimation, bIsPreviewing);
			}
			UICommon::TextUnformattedEllipsis(animPath.data(), nullptr, ImGui::GetContentRegionAvail().x - previewButtonWidth);

			// preview button
			DrawPreviewButtons(refrToEvaluate, a_replacementAnimation, previewButtonWidth, bCanPreview, bIsPreviewing);

			// draw variants
			if (a_replacementAnimation->HasVariants()) {
				ImGui::TextUnformatted("Variants:");
				ImGui::Indent();
				a_replacementAnimation->ForEachVariant([&](Variant& a_variant) {
					const bool bVariantDisabled = a_variant.IsDisabled();
					if (bVariantDisabled) {
						auto& style = ImGui::GetStyle();
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.DisabledAlpha);
					}

					UICommon::TextUnformattedDisabled("Weight:");
					ImGui::SameLine();
					ImGui::TextUnformatted(std::format("{}", a_variant.GetWeight()).data());
					UICommon::AddTooltip("The weight of this variant used for the weighted random selection (e.g. a variant with a weight of 2 will be twice as likely to be picked than a variant with a weight of 1)");
					ImGui::SameLine();
					UICommon::TextUnformattedDisabled("Filename:");

					bIsPreviewing = IsPreviewingAnimation(refrToEvaluate, a_replacementAnimation, a_variant.GetIndex());

					float variantPreviewButtonWidth = 0.f;
					if (bCanPreview || bIsPreviewing) {
						variantPreviewButtonWidth = GetPreviewButtonsWidth(a_replacementAnimation, bIsPreviewing);
					}
					ImGui::SameLine();
					UICommon::TextUnformattedEllipsis(a_variant.GetFilename().data(), nullptr, ImGui::GetContentRegionAvail().x - variantPreviewButtonWidth);

					// preview variant button
					DrawPreviewButtons(refrToEvaluate, a_replacementAnimation, variantPreviewButtonWidth, bCanPreview, bIsPreviewing, &a_variant);

					if (bVariantDisabled) {
						ImGui::PopStyleVar();
					}

					return RE::BSVisit::BSVisitControl::kContinue;
				});
				ImGui::Unindent();
			}
			DrawConditionSet(a_replacementAnimation->GetConditionSet(), a_replacementAnimation->GetParentSubMod(), EditMode::kNone, Conditions::ConditionType::kNormal, refrToEvaluate, true, ImGui::GetCursorScreenPos());
			ImGui::TreePop();
		}
		ImGui::PopID();

		if (bAnimationDisabled) {
			ImGui::PopStyleVar();
		}
	}

	bool UIMain::DrawConditionSet(Conditions::ConditionSet* a_conditionSet, SubMod* a_parentSubMod, EditMode a_editMode, Conditions::ConditionType a_conditionType, RE::TESObjectREFR* a_refrToEvaluate, bool a_bDrawLines, const ImVec2& a_drawStartPos)
	{
		if (!a_conditionSet) {
			return false;
		}

		//ImGui::TableNextRow();
		//ImGui::TableSetColumnIndex(0);

		bool bSetDirty = false;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImGuiStyle& style = ImGui::GetStyle();

		ImVec2 vertLineStart = a_drawStartPos;
		vertLineStart.y += style.FramePadding.x;
		vertLineStart.x -= style.IndentSpacing * 0.6f;
		ImVec2 vertLineEnd = vertLineStart;

		const float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;

		if (!a_conditionSet->IsEmpty()) {
			a_conditionSet->ForEach([&](std::unique_ptr<Conditions::ICondition>& a_condition) {
				const ImRect nodeRect = DrawCondition(a_condition, a_conditionSet, a_parentSubMod, a_editMode, a_conditionType, a_refrToEvaluate, bSetDirty);
				if (a_bDrawLines) {
					const float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
					constexpr float horLineLength = 10.f;
					drawList->AddLine(ImVec2(vertLineStart.x, midPoint), ImVec2(vertLineStart.x + horLineLength, midPoint), ImGui::GetColorU32(UICommon::TREE_LINE_COLOR));
					vertLineEnd.y = midPoint;
				}

				return RE::BSVisit::BSVisitControl::kContinue;
			});
		} else {
			DrawBlankCondition(a_conditionSet, a_editMode, a_conditionType);
		}

		if (a_bDrawLines) {
			drawList->AddLine(vertLineStart, vertLineEnd, ImGui::GetColorU32(UICommon::TREE_LINE_COLOR));
		}

		if (a_editMode > EditMode::kNone) {
			const bool bIsConditionPreset = a_conditionType == Conditions::ConditionType::kPreset;

			// Add condition button
			if (ImGui::Button("Add new condition")) {
				if (bIsConditionPreset && _lastAddNewConditionName == "PRESET") {
					_lastAddNewConditionName.clear();
				}
				if (_lastAddNewConditionName.empty()) {
					auto isFormCondition = Conditions::CreateCondition("IsForm"sv);
					a_conditionSet->Add(isFormCondition, true);
					bSetDirty = true;
				} else {
					auto newCondition = Conditions::CreateCondition(_lastAddNewConditionName);
					a_conditionSet->Add(newCondition, true);
					bSetDirty = true;
				}
			}

			// Condition set functions button
			ImGui::SameLine(0.f, 20.f);
			const auto popupId = std::string("Condition set functions##") + std::to_string(reinterpret_cast<uintptr_t>(a_conditionSet));
			if (UICommon::PopupToggleButton("Condition set...", popupId.data())) {
				ImGui::OpenPopup(popupId.data());
			}

			if (ImGui::BeginPopupContextItem(popupId.data())) {
				const auto xButtonSize = ImGui::CalcTextSize("Paste condition set").x + style.FramePadding.x * 2 + style.ItemSpacing.x;

				// Copy conditions button
				ImGui::BeginDisabled(a_conditionSet->IsEmpty() || !a_conditionSet->IsValid());
				if (ImGui::Button("Copy condition set", ImVec2(xButtonSize, 0))) {
					ImGui::CloseCurrentPopup();
					_conditionSetCopy = DuplicateConditionSet(a_conditionSet);
				}
				ImGui::EndDisabled();

				// Paste conditions button
				const bool bPasteEnabled = _conditionSetCopy && !(bIsConditionPreset && ConditionSetContainsPreset(_conditionSetCopy.get()));
				ImGui::BeginDisabled(!bPasteEnabled);
				if (ImGui::Button("Paste condition set", ImVec2(xButtonSize, 0))) {
					ImGui::CloseCurrentPopup();
					const auto duplicatedSet = DuplicateConditionSet(_conditionSetCopy.get());
					a_conditionSet->Append(duplicatedSet.get());
					bSetDirty = true;
				}
				ImGui::EndDisabled();
				// Paste tooltip
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
					ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
					ImGui::BeginTooltip();
					DrawConditionSet(_conditionSetCopy.get(), nullptr, EditMode::kNone, a_conditionType, nullptr, false, a_drawStartPos);
					ImGui::EndTooltip();
				}

				// Clear conditions button
				ImGui::BeginDisabled(a_conditionSet->IsEmpty());
				UICommon::ButtonWithConfirmationModal(
					"Clear condition set"sv, "Are you sure you want to clear the condition set?\nThis operation cannot be undone!\n\n"sv, [&]() {
						ImGui::ClosePopupsExceptModals();
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ClearConditionSetJob>(a_conditionSet);
						bSetDirty = true;
					},
					ImVec2(xButtonSize, 0));
				ImGui::EndDisabled();
				ImGui::EndPopup();
			}
		}

		return bSetDirty;
	}

	bool UIMain::DrawFunctionSet(Functions::FunctionSet* a_functionSet, SubMod* a_parentSubMod, EditMode a_editMode, Functions::FunctionSetType a_functionSetType, RE::TESObjectREFR* a_refrToEvaluate, bool a_bDrawLines, const ImVec2& a_drawStartPos)
	{
		bool bSetDirty = false;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImGuiStyle& style = ImGui::GetStyle();

		ImVec2 vertLineStart = a_drawStartPos;
		vertLineStart.y += style.FramePadding.x;
		vertLineStart.x -= style.IndentSpacing * 0.6f;
		ImVec2 vertLineEnd = vertLineStart;

		const float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;

		if (a_functionSet && !a_functionSet->IsEmpty()) {
			a_functionSet->ForEach([&](std::unique_ptr<Functions::IFunction>& a_function) {
				const ImRect nodeRect = DrawFunction(a_function, a_functionSet, a_parentSubMod, a_editMode, a_functionSetType, a_refrToEvaluate, bSetDirty);
				if (a_bDrawLines) {
					const float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
					constexpr float horLineLength = 10.f;
					drawList->AddLine(ImVec2(vertLineStart.x, midPoint), ImVec2(vertLineStart.x + horLineLength, midPoint), ImGui::GetColorU32(UICommon::TREE_LINE_COLOR));
					vertLineEnd.y = midPoint;
				}

				return RE::BSVisit::BSVisitControl::kContinue;
			});
		} else {
			DrawBlankFunction(a_functionSet, a_parentSubMod, a_editMode, a_functionSetType);
		}

		if (a_bDrawLines) {
			drawList->AddLine(vertLineStart, vertLineEnd, ImGui::GetColorU32(UICommon::TREE_LINE_COLOR));
		}

		if (a_editMode > EditMode::kNone) {
			// Add function button
			if (ImGui::Button("Add new function")) {
				if (!a_functionSet) {
					a_functionSet = a_parentSubMod->CreateOrGetFunctionSet(a_functionSetType);
				}
				if (_lastAddNewFunctionName.empty()) {
					auto defaultFunction = Functions::CreateFunction("AddSpell"sv);
					a_functionSet->Add(defaultFunction, true);
					bSetDirty = true;
				} else {
					auto newFunction = Functions::CreateFunction(_lastAddNewFunctionName);
					a_functionSet->Add(newFunction, true);
					bSetDirty = true;
				}
			}

			// Function set functions button
			ImGui::SameLine(0.f, 20.f);
			const auto popupId = std::string("Function set functions##") + std::to_string(reinterpret_cast<uintptr_t>(a_parentSubMod)) + std::to_string(static_cast<uint8_t>(a_functionSetType)) + std::to_string(reinterpret_cast<uintptr_t>(a_functionSet));
			if (UICommon::PopupToggleButton("Function set...", popupId.data())) {
				ImGui::OpenPopup(popupId.data());
			}

			if (ImGui::BeginPopupContextItem(popupId.data())) {
				const auto xButtonSize = ImGui::CalcTextSize("Paste function set").x + style.FramePadding.x * 2 + style.ItemSpacing.x;

				// Copy functions button
				ImGui::BeginDisabled(!a_functionSet || a_functionSet->IsEmpty() || !a_functionSet->IsValid());
				if (ImGui::Button("Copy function set", ImVec2(xButtonSize, 0))) {
					ImGui::CloseCurrentPopup();
					_functionSetCopy = DuplicateFunctionSet(a_functionSet);
				}
				ImGui::EndDisabled();

				// Paste functions button
				const bool bPasteEnabled = _functionSetCopy != nullptr;
				ImGui::BeginDisabled(!bPasteEnabled);
				if (ImGui::Button("Paste function set", ImVec2(xButtonSize, 0))) {
					ImGui::CloseCurrentPopup();
					const auto duplicatedSet = DuplicateFunctionSet(_functionSetCopy.get());
					if (!a_functionSet) {
						a_functionSet = a_parentSubMod->CreateOrGetFunctionSet(a_functionSetType);
					}
					a_functionSet->Append(duplicatedSet.get());
					bSetDirty = true;
				}
				ImGui::EndDisabled();
				// Paste tooltip
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
					ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
					ImGui::BeginTooltip();
					DrawFunctionSet(_functionSetCopy.get(), nullptr, EditMode::kNone, a_functionSetType, nullptr, false, a_drawStartPos);
					ImGui::EndTooltip();
				}

				// Clear functions button
				ImGui::BeginDisabled(!a_functionSet || a_functionSet->IsEmpty());
				UICommon::ButtonWithConfirmationModal(
					"Clear function set"sv, "Are you sure you want to clear the function set?\nThis operation cannot be undone!\n\n"sv, [&]() {
						ImGui::ClosePopupsExceptModals();
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ClearFunctionSetJob>(a_functionSet);
						bSetDirty = true;
					},
					ImVec2(xButtonSize, 0));
				ImGui::EndDisabled();
				ImGui::EndPopup();
			}
		}

		return bSetDirty;
	}

	ImRect UIMain::DrawCondition(std::unique_ptr<Conditions::ICondition>& a_condition, Conditions::ConditionSet* a_conditionSet, SubMod* a_parentSubMod, EditMode a_editMode, Conditions::ConditionType a_conditionType, RE::TESObjectREFR* a_refrToEvaluate, bool& a_bOutSetDirty)
	{
		ImRect conditionRect;

		// Evaluate
		auto evalResult = ConditionEvaluateResult::kFailure;
		if (Utils::ConditionHasRandomResult(a_condition.get())) {
			evalResult = ConditionEvaluateResult::kRandom;
		} else if (a_refrToEvaluate) {
			bEvaluatingConditionsForUI = true;
			const bool bEvaluationResult = a_condition->Evaluate(a_refrToEvaluate, nullptr, a_parentSubMod);
			bEvaluatingConditionsForUI = false;
			if (bEvaluationResult) {
				evalResult = ConditionEvaluateResult::kSuccess;
			}
		}

		//ImGui::BeginGroup();
		ImRect nodeRect;

		std::string conditionTableId = std::format("{}conditionTable", reinterpret_cast<uintptr_t>(a_condition.get()));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
		const bool bIsConditionPreset = a_condition->GetConditionType() == Conditions::ConditionType::kPreset;
		const bool bHasSharedState = Utils::ConditionHasStateComponentWithSharedScope(a_condition.get());
		if (bIsConditionPreset) {
			ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, UICommon::CONDITION_PRESET_BORDER_COLOR);
		} else if (bHasSharedState) {
			ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, UICommon::CONDITION_SHARED_STATE_BORDER_COLOR);
		}

		if (ImGui::BeginTable(conditionTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			//ImGui::AlignTextToFramePadding();

			if (bIsConditionPreset || bHasSharedState) {
				ImGui::PopStyleColor();  // ImGuiCol_TableBorderStrong
			}

			const auto conditionName = a_condition->GetName();
			std::string nodeName = conditionName.data();
			if (a_condition->IsNegated()) {
				nodeName.insert(0, "NOT ");
			}

			bool bStyleVarPushed = false;
			if (a_condition->IsDisabled()) {
				auto& style = ImGui::GetStyle();
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * style.DisabledAlpha);
				bStyleVarPushed = true;
			}

			float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;
			ImVec2 nodePos = ImGui::GetCursorScreenPos();

			// Open node
			bool bNodeOpen = false;
			if (a_editMode > EditMode::kNone || a_condition->GetNumComponents() > 0) {
				bNodeOpen = ImGui::TreeNodeEx(a_condition.get(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
			} else {
				bNodeOpen = UICommon::TreeNodeCollapsedLeaf(a_condition.get(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
				//bNodeOpen = ImGui::TreeNodeEx(a_condition.get(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
			}

			// Condition context menu
			if (a_editMode > EditMode::kNone) {
				if (ImGui::BeginPopupContextItem()) {
					// copy button
					auto& style = ImGui::GetStyle();
					auto xButtonSize = ImGui::CalcTextSize("Paste condition below").x + style.FramePadding.x * 2 + style.ItemSpacing.x;
					ImGui::BeginDisabled(!a_condition->IsValid());
					if (ImGui::Button("Copy condition", ImVec2(xButtonSize, 0))) {
						_conditionCopy = DuplicateCondition(a_condition);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					// paste button
					const bool bPasteEnabled = _conditionCopy && !(a_conditionType == Conditions::ConditionType::kPreset && ConditionContainsPreset(_conditionCopy.get()));
					ImGui::BeginDisabled(!bPasteEnabled);
					if (ImGui::Button("Paste condition below", ImVec2(xButtonSize, 0))) {
						auto duplicate = DuplicateCondition(_conditionCopy);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::InsertConditionJob>(duplicate, a_conditionSet, a_condition);
						a_bOutSetDirty = true;
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();
					// paste tooltip
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
						ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
						ImGui::BeginTooltip();
						bool bDummy = false;
						DrawCondition(_conditionCopy, a_conditionSet, nullptr, EditMode::kNone, a_conditionType, nullptr, bDummy);
						ImGui::EndTooltip();
					}

					// delete button
					UICommon::ButtonWithConfirmationModal(
						"Delete condition"sv, "Are you sure you want to remove the condition?\nThis operation cannot be undone!\n\n"sv, [&]() {
							ImGui::ClosePopupsExceptModals();
							OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveConditionJob>(a_condition, a_conditionSet);
							a_bOutSetDirty = true;
						},
						ImVec2(xButtonSize, 0));

					ImGui::EndPopup();
				}
			}

			// Condition description tooltip
			DrawInfoTooltip(a_condition.get());

			nodeRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			// Drag & Drop source
			if (a_editMode > EditMode::kNone && a_condition->IsValid()) {
				if (BeginDragDropSourceEx(ImGuiDragDropFlags_SourceNoHoldToOpenOthers, ImVec2(tooltipWidth, 0))) {
					DragConditionPayload payload(a_condition, a_conditionSet);
					ImGui::SetDragDropPayload("DND_CONDITION", &payload, sizeof(DragConditionPayload));
					bool bDummy = false;
					DrawCondition(a_condition, a_conditionSet, nullptr, EditMode::kNone, a_conditionType, nullptr, bDummy);
					ImGui::EndDragDropSource();
				}
			}

			// Drag & Drop target - tree node
			if (a_editMode > EditMode::kNone) {
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_CONDITION", ImGuiDragDropFlags_AcceptPeekOnly)) {
						DragConditionPayload payload = *static_cast<DragConditionPayload*>(imguiPayload->Data);

						const ImGuiStyle& style = ImGui::GetStyle();
						if (!bNodeOpen) {
							// Draw our own preview of the drop because we want to draw a line either above or below the condition
							float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
							const auto upperHalf = ImRect(nodeRect.Min.x, nodeRect.Min.y, nodeRect.Max.x, midPoint);
							const auto lowerHalf = ImRect(nodeRect.Min.x, midPoint, nodeRect.Max.x, nodeRect.Max.y);

							bool bInsertAfter = ImGui::IsMouseHoveringRect(lowerHalf.Min, lowerHalf.Max);

							ImDrawList* drawList = ImGui::GetWindowDrawList();
							auto lineY = bInsertAfter ? nodeRect.Max.y + style.ItemSpacing.y * 0.5f : nodeRect.Min.y - style.ItemSpacing.y * 0.5f;
							drawList->AddLine(ImVec2(nodeRect.Min.x, lineY), ImVec2(nodeRect.Max.x, lineY), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

							if (imguiPayload->IsDelivery()) {
								OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveConditionJob>(payload.condition, payload.conditionSet, a_condition, a_conditionSet, bInsertAfter);
							}
						} else {
							// Draw our own preview of the drop because we want to draw a line above the condition if we're hovering over the upper half of the node. Ignore anything below, we have the invisible button for that
							float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
							const auto upperHalf = ImRect(nodeRect.Min.x, nodeRect.Min.y, nodeRect.Max.x, midPoint);

							if (ImGui::IsMouseHoveringRect(upperHalf.Min, upperHalf.Max)) {
								ImDrawList* drawList = ImGui::GetWindowDrawList();
								drawList->AddLine(ImVec2(nodeRect.Min.x, nodeRect.Min.y - style.ItemSpacing.y * 0.5f), ImVec2(nodeRect.Max.x, nodeRect.Min.y - style.ItemSpacing.y * 0.5f), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

								if (imguiPayload->IsDelivery()) {
									OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveConditionJob>(payload.condition, payload.conditionSet, a_condition, a_conditionSet, false);
								}
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}

			// Disable checkbox
			ImGui::SameLine();
			if (a_editMode > EditMode::kNone) {
				std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_condition.get()));
				ImGui::PushID(idString.data());
				bool bEnabled = !a_condition->IsDisabled();
				if (ImGui::Checkbox("##toggleCondition", &bEnabled)) {
					a_condition->SetDisabled(!bEnabled);
					a_conditionSet->SetDirty(true);
					a_bOutSetDirty = true;
				}
				UICommon::AddTooltip("Toggles the condition on/off");
				ImGui::PopID();
			}

			// Condition name
			ImGui::SameLine();
			if (a_condition->IsValid()) {
				auto requiredPluginName = a_condition->GetRequiredPluginName();
				if (!requiredPluginName.empty()) {
					UICommon::TextUnformattedColored(UICommon::CUSTOM_CONDITION_COLOR, nodeName.data());
				} else if (a_condition->GetConditionType() == Conditions::ConditionType::kPreset) {
					UICommon::TextUnformattedColored(UICommon::CONDITION_PRESET_COLOR, nodeName.data());
				} else {
					ImGui::TextUnformatted(nodeName.data());
				}
			} else {
				UICommon::TextUnformattedColored(UICommon::INVALID_CONDITION_COLOR, nodeName.data());
			}

			ImVec2 cursorPos = ImGui::GetCursorScreenPos();

			// Right column, argument text
			UICommon::SecondColumn(_firstColumnWidthPercent);
			const auto argument = a_condition->GetArgument();
			ImGui::TextUnformatted(argument.data());

			//ImGui::TableSetColumnIndex(0);

			// Evaluate success/failure indicator
			if (a_refrToEvaluate) {
				UICommon::DrawConditionEvaluateResult(evalResult);
			}

			conditionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			// Node contents
			if (bNodeOpen) {
				ImGui::Spacing();

				if (a_editMode > EditMode::kNone) {
					// negate checkbox
					bool bNOT = a_condition->IsNegated();
					if (ImGui::Checkbox("Negate", &bNOT)) {
						a_condition->SetNegated(bNOT);
						a_conditionSet->SetDirty(true);
						a_bOutSetDirty = true;
					}
					UICommon::AddTooltip("Negates the condition");

					// select condition type
					ImGui::SameLine();
					const float conditionComboWidth = UICommon::FirstColumnWidth(_firstColumnWidthPercent);
					ImGui::SetNextItemWidth(conditionComboWidth);

					const auto& conditionInfos = _conditionComboFilter.GetConditionInfos(a_conditionType);
					if (conditionInfos.empty()) {
						_conditionComboFilter.CacheInfos();
					}

					int selectedItem = -1;
					const Info* currentConditionInfo = nullptr;

					auto it = std::ranges::find_if(conditionInfos, [&](const Info& a_conditionInfo) {
						return a_conditionInfo.name == std::string_view(conditionName);
					});

					if (it != conditionInfos.end()) {
						selectedItem = static_cast<int>(std::distance(conditionInfos.begin(), it));
						currentConditionInfo = &*it;
					}

					if (_conditionComboFilter.ComboFilter("##Condition type", selectedItem, conditionInfos, currentConditionInfo, ImGuiComboFlags_HeightLarge, &UIMain::DrawInfoTooltip)) {
						if (selectedItem >= 0 && selectedItem < conditionInfos.size() && OpenAnimationReplacer::GetSingleton().HasConditionFactory(conditionInfos[selectedItem].name)) {
							_lastAddNewConditionName = conditionInfos[selectedItem].name;
							OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReplaceConditionJob>(a_condition, conditionInfos[selectedItem].name, a_conditionSet);
							a_bOutSetDirty = true;
						}
					}

					// remove condition button
					UICommon::SecondColumn(_firstColumnWidthPercent);

					UICommon::ButtonWithConfirmationModal("Delete condition"sv, "Are you sure you want to remove the condition?\nThis operation cannot be undone!\n\n"sv, [&]() {
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveConditionJob>(a_condition, a_conditionSet);
						a_bOutSetDirty = true;
					});

					// Essential state
					{
						if (a_condition->GetConditionType() == Conditions::ConditionType::kCustom && a_condition->GetConditionAPIVersion() >= Conditions::ConditionAPIVersion::kNew) {
							static std::map<Conditions::EssentialState, std::string_view> enumMap = {
								{ Conditions::EssentialState::kEssential, "Essential" },
								{ Conditions::EssentialState::kNonEssential_True, "Non-essential - Return true" },
								{ Conditions::EssentialState::kNonEssential_False, "Non-essential - Return false" }
							};
							ImGui::SameLine();
							std::string idString = std::format("{}##essential", reinterpret_cast<uintptr_t>(a_condition.get()));
							ImGui::PushID(idString.data());

							Conditions::EssentialState currentValue = a_condition->GetEssential();
							std::string currentEnumName;
							if (const auto search = enumMap.find(currentValue); search != enumMap.end()) {
								currentEnumName = search->second;
							} else {
								currentEnumName = std::format("Unknown ({})", static_cast<uint8_t>(currentValue));
							}

							if (ImGui::BeginCombo(idString.data(), currentEnumName.data())) {
								for (auto& [enumValue, enumName] : enumMap) {
									const bool bIsCurrent = enumValue == currentValue;
									if (ImGui::Selectable(enumName.data(), bIsCurrent)) {
										if (!bIsCurrent) {
											currentValue = enumValue;
											a_condition->SetEssential(currentValue);
											a_conditionSet->SetDirty(true);
											a_bOutSetDirty = true;
										}
									}
									if (bIsCurrent) {
										ImGui::SetItemDefaultFocus();
									}
								}
								ImGui::EndCombo();
							}
							ImGui::PopID();
							UICommon::AddTooltip("Users missing a plugin implementing this condition won't be notified about an error and the condition will evaluate to either true or false, depending on the selected option.");
						}
					}
				}

				if (auto numComponents = a_condition->GetNumComponents(); numComponents > 0) {
					bool bHasStateComponent = false;
					for (uint32_t i = 0; i < numComponents; i++) {
						auto component = a_condition->GetComponent(i);
						const bool bIsMultiConditionComponent = component->GetType() == Conditions::ConditionComponentType::kMulti;
						const bool bIsConditionPresetComponent = component->GetType() == Conditions::ConditionComponentType::kPreset;
						if (component->GetType() == Conditions::ConditionComponentType::kState) {
							bHasStateComponent = true;
						}
						if (bIsMultiConditionComponent || bIsConditionPresetComponent) {
							const auto multiConditionComponent = static_cast<Conditions::IMultiConditionComponent*>(component);
							// draw conditions
							auto conditionType = a_conditionType == Conditions::ConditionType::kPreset || bIsConditionPresetComponent ? Conditions::ConditionType::kPreset : Conditions::ConditionType::kNormal;
							if (DrawConditionSet(multiConditionComponent->GetConditions(), a_parentSubMod, bIsConditionPresetComponent ? EditMode::kNone : a_editMode, conditionType, multiConditionComponent->GetShouldDrawEvaluateResultForChildConditions() ? a_condition->GetRefrToEvaluate(a_refrToEvaluate) : nullptr, true, cursorPos)) {
								a_conditionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
							// display component
							if (component->DisplayInUI(a_editMode != EditMode::kNone, _firstColumnWidthPercent)) {
								a_conditionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
						} else {
							ImGui::Separator();
							// write component name aligned to the right
							const auto componentName = component->GetName();
							UICommon::TextDescriptionRightAligned(componentName.data());
							// show component description on mouseover
							const auto componentDescription = component->GetDescription();
							if (!componentDescription.empty()) {
								UICommon::AddTooltip(componentDescription.data());
							}
							// display component
							if (component->DisplayInUI(a_editMode != EditMode::kNone, _firstColumnWidthPercent)) {
								a_conditionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
						}
					}
					if (bHasStateComponent && a_bOutSetDirty) {
						OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
					}
				}

				// Display current value, if applicable
				const auto current = a_condition->GetCurrent(a_refrToEvaluate);
				if (!current.empty()) {
					ImGui::Separator();
					UICommon::TextUnformattedDisabled("Current:");
					ImGui::SameLine();
					ImGui::TextUnformatted(current.data());
				}

				ImGui::Spacing();

				ImGui::TreePop();
			}

			if (bStyleVarPushed) {
				ImGui::PopStyleVar();
			}

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();  // ImGuiStyleVar_CellPadding

		auto rectMax = ImGui::GetItemRectMax();

		auto width = ImGui::GetItemRectSize().x;
		auto groupEnd = ImGui::GetCursorPos();
		const ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 invisibleButtonStart = groupEnd;
		invisibleButtonStart.y -= style.ItemSpacing.y;
		ImGui::SetCursorPos(invisibleButtonStart);
		std::string conditionInvisibleDragAreaId = std::format("{}conditionInvisibleDragArea", reinterpret_cast<uintptr_t>(a_condition.get()));
		ImGui::InvisibleButton(conditionInvisibleDragAreaId.data(), ImVec2(width, style.ItemSpacing.y));

		// Drag & Drop target - invisible button
		if (a_editMode > EditMode::kNone) {
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_CONDITION", ImGuiDragDropFlags_AcceptPeekOnly)) {
					DragConditionPayload payload = *static_cast<DragConditionPayload*>(imguiPayload->Data);
					// Draw our own preview of the drop because we want to draw a line below the condition

					ImDrawList* drawList = ImGui::GetWindowDrawList();
					drawList->AddLine(ImVec2(nodeRect.Min.x, rectMax.y + style.ItemSpacing.y * 0.5f), ImVec2(nodeRect.Max.x, rectMax.y + style.ItemSpacing.y * 0.5f), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

					if (imguiPayload->IsDelivery()) {
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveConditionJob>(payload.condition, payload.conditionSet, a_condition, a_conditionSet, true);
					}
				}
				ImGui::EndDragDropTarget();
			}
		}

		ImGui::SetCursorPos(groupEnd);

		//ImGui::EndGroup();

		return conditionRect;
	}

	ImRect UIMain::DrawFunction(std::unique_ptr<Functions::IFunction>& a_function, Functions::FunctionSet* a_functionSet, SubMod* a_parentSubMod, EditMode a_editMode, Functions::FunctionSetType a_functionSetType, RE::TESObjectREFR* a_refrToEvaluate, bool& a_bOutSetDirty)
	{
		ImRect functionRect;

		ImRect nodeRect;

		std::string functionTableId = std::format("{}functionTable", reinterpret_cast<uintptr_t>(a_function.get()));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));

		if (ImGui::BeginTable(functionTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			//ImGui::AlignTextToFramePadding();

			const auto functionName = a_function->GetName();
			std::string nodeName = functionName.data();

			bool bStyleVarPushed = false;
			if (a_function->IsDisabled()) {
				auto& style = ImGui::GetStyle();
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * style.DisabledAlpha);
				bStyleVarPushed = true;
			}

			float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;
			ImVec2 nodePos = ImGui::GetCursorScreenPos();

			// Open node
			bool bNodeOpen = false;
			if (a_editMode > EditMode::kNone || a_function->GetNumComponents() > 0) {
				bNodeOpen = ImGui::TreeNodeEx(a_function.get(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
			} else {
				bNodeOpen = UICommon::TreeNodeCollapsedLeaf(a_function.get(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
				//bNodeOpen = ImGui::TreeNodeEx(a_condition.get(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
			}

			// Function context menu
			if (a_editMode > EditMode::kNone) {
				if (ImGui::BeginPopupContextItem()) {
					// copy button
					auto& style = ImGui::GetStyle();
					auto xButtonSize = ImGui::CalcTextSize("Paste function below").x + style.FramePadding.x * 2 + style.ItemSpacing.x;
					ImGui::BeginDisabled(!a_function->IsValid());
					if (ImGui::Button("Copy function", ImVec2(xButtonSize, 0))) {
						_functionCopy = DuplicateFunction(a_function);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					// paste button
					const bool bPasteEnabled = _functionCopy != nullptr;
					ImGui::BeginDisabled(!bPasteEnabled);
					if (ImGui::Button("Paste function below", ImVec2(xButtonSize, 0))) {
						auto duplicate = DuplicateFunction(_functionCopy);
						if (!a_functionSet) {
							a_functionSet = a_parentSubMod->CreateOrGetFunctionSet(a_functionSetType);
						}
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::InsertFunctionJob>(duplicate, a_functionSet, a_function);
						a_bOutSetDirty = true;
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					// paste tooltip
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
						ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
						ImGui::BeginTooltip();
						bool bDummy = false;
						DrawFunction(_functionCopy, a_functionSet, nullptr, EditMode::kNone, a_functionSetType, nullptr, bDummy);
						ImGui::EndTooltip();
					}

					// delete button
					UICommon::ButtonWithConfirmationModal(
						"Delete function"sv, "Are you sure you want to remove the function?\nThis operation cannot be undone!\n\n"sv, [&]() {
							ImGui::ClosePopupsExceptModals();
							OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveFunctionJob>(a_function, a_functionSet);
							a_bOutSetDirty = true;
						},
						ImVec2(xButtonSize, 0));

					ImGui::EndPopup();
				}
			}

			// Function description tooltip
			DrawInfoTooltip(a_function.get());

			nodeRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			// Drag & Drop source
			if (a_editMode > EditMode::kNone && a_function->IsValid()) {
				if (BeginDragDropSourceEx(ImGuiDragDropFlags_SourceNoHoldToOpenOthers, ImVec2(tooltipWidth, 0))) {
					DragFunctionPayload payload(a_function, a_functionSet);
					ImGui::SetDragDropPayload("DND_FUNCTION", &payload, sizeof(DragConditionPayload));
					bool bDummy = false;
					DrawFunction(a_function, a_functionSet, nullptr, EditMode::kNone, a_functionSetType, nullptr, bDummy);
					ImGui::EndDragDropSource();
				}
			}

			// Drag & Drop target - tree node
			if (a_editMode > EditMode::kNone) {
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_FUNCTION", ImGuiDragDropFlags_AcceptPeekOnly)) {
						DragFunctionPayload payload = *static_cast<DragFunctionPayload*>(imguiPayload->Data);

						const ImGuiStyle& style = ImGui::GetStyle();
						if (!bNodeOpen) {
							// Draw our own preview of the drop because we want to draw a line either above or below the function
							float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
							const auto upperHalf = ImRect(nodeRect.Min.x, nodeRect.Min.y, nodeRect.Max.x, midPoint);
							const auto lowerHalf = ImRect(nodeRect.Min.x, midPoint, nodeRect.Max.x, nodeRect.Max.y);

							bool bInsertAfter = ImGui::IsMouseHoveringRect(lowerHalf.Min, lowerHalf.Max);

							ImDrawList* drawList = ImGui::GetWindowDrawList();
							auto lineY = bInsertAfter ? nodeRect.Max.y + style.ItemSpacing.y * 0.5f : nodeRect.Min.y - style.ItemSpacing.y * 0.5f;
							drawList->AddLine(ImVec2(nodeRect.Min.x, lineY), ImVec2(nodeRect.Max.x, lineY), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

							if (imguiPayload->IsDelivery()) {
								OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveFunctionJob>(payload.function, payload.functionSet, a_function, a_functionSet, bInsertAfter);
							}
						} else {
							// Draw our own preview of the drop because we want to draw a line above the function if we're hovering over the upper half of the node. Ignore anything below, we have the invisible button for that
							float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
							const auto upperHalf = ImRect(nodeRect.Min.x, nodeRect.Min.y, nodeRect.Max.x, midPoint);

							if (ImGui::IsMouseHoveringRect(upperHalf.Min, upperHalf.Max)) {
								ImDrawList* drawList = ImGui::GetWindowDrawList();
								drawList->AddLine(ImVec2(nodeRect.Min.x, nodeRect.Min.y - style.ItemSpacing.y * 0.5f), ImVec2(nodeRect.Max.x, nodeRect.Min.y - style.ItemSpacing.y * 0.5f), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

								if (imguiPayload->IsDelivery()) {
									OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveFunctionJob>(payload.function, payload.functionSet, a_function, a_functionSet, false);
								}
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}

			// Disable checkbox
			ImGui::SameLine();
			if (a_editMode > EditMode::kNone) {
				std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_function.get()));
				ImGui::PushID(idString.data());
				bool bEnabled = !a_function->IsDisabled();
				if (ImGui::Checkbox("##toggleFunction", &bEnabled)) {
					a_function->SetDisabled(!bEnabled);
					a_functionSet->SetDirty(true);
					a_bOutSetDirty = true;
				}
				UICommon::AddTooltip("Toggles the function on/off");
				ImGui::PopID();
			}

			// Function name
			ImGui::SameLine();
			if (a_function->IsValid()) {
				auto requiredPluginName = a_function->GetRequiredPluginName();
				if (!requiredPluginName.empty()) {
					UICommon::TextUnformattedColored(UICommon::CUSTOM_CONDITION_COLOR, nodeName.data());
				} else {
					ImGui::TextUnformatted(nodeName.data());
				}
			} else {
				UICommon::TextUnformattedColored(UICommon::INVALID_CONDITION_COLOR, nodeName.data());
			}

			ImVec2 cursorPos = ImGui::GetCursorScreenPos();

			// Right column, argument text
			UICommon::SecondColumn(_firstColumnWidthPercent);
			const auto argument = a_function->GetArgument();
			ImGui::TextUnformatted(argument.data());

			//ImGui::TableSetColumnIndex(0);

			functionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			// Node contents
			if (bNodeOpen) {
				ImGui::Spacing();

				if (a_editMode > EditMode::kNone) {
					// select function type
					//ImGui::SameLine();
					const float functionComboWidth = UICommon::FirstColumnWidth(_firstColumnWidthPercent);
					ImGui::SetNextItemWidth(functionComboWidth);

					const auto& functionInfos = _functionComboFilter.GetFunctionInfos();
					if (functionInfos.empty()) {
						_functionComboFilter.CacheInfos();
					}

					int selectedItem = -1;
					const Info* currentFunctionInfo = nullptr;

					auto it = std::ranges::find_if(functionInfos, [&](const Info& a_functionInfo) {
						return a_functionInfo.name == std::string_view(functionName);
					});

					if (it != functionInfos.end()) {
						selectedItem = static_cast<int>(std::distance(functionInfos.begin(), it));
						currentFunctionInfo = &*it;
					}

					if (_functionComboFilter.ComboFilter("##Function type", selectedItem, functionInfos, currentFunctionInfo, ImGuiComboFlags_HeightLarge, &UIMain::DrawInfoTooltip)) {
						if (selectedItem >= 0 && selectedItem < functionInfos.size() && OpenAnimationReplacer::GetSingleton().HasFunctionFactory(functionInfos[selectedItem].name)) {
							_lastAddNewFunctionName = functionInfos[selectedItem].name;
							OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReplaceFunctionJob>(a_function, functionInfos[selectedItem].name, a_functionSet);
							a_bOutSetDirty = true;
						}
					}

					// remove function button
					UICommon::SecondColumn(_firstColumnWidthPercent);

					UICommon::ButtonWithConfirmationModal("Delete function"sv, "Are you sure you want to remove the function?\nThis operation cannot be undone!\n\n"sv, [&]() {
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveFunctionJob>(a_function, a_functionSet);
						a_bOutSetDirty = true;
					});

					// Essential state
					{
						if (a_function->GetFunctionType() == Functions::FunctionType::kCustom) {
							static std::map<Functions::EssentialState, std::string_view> enumMap = {
								{ Functions::EssentialState::kEssential, "Essential" },
								{ Functions::EssentialState::kNonEssential_True, "Non-essential - Return true" },
								{ Functions::EssentialState::kNonEssential_False, "Non-essential - Return false" }
							};
							ImGui::SameLine();
							std::string idString = std::format("{}##essential", reinterpret_cast<uintptr_t>(a_function.get()));
							ImGui::PushID(idString.data());

							Functions::EssentialState currentValue = a_function->GetEssential();
							std::string currentEnumName;
							if (const auto search = enumMap.find(currentValue); search != enumMap.end()) {
								currentEnumName = search->second;
							} else {
								currentEnumName = std::format("Unknown ({})", static_cast<uint8_t>(currentValue));
							}

							if (ImGui::BeginCombo(idString.data(), currentEnumName.data())) {
								for (auto& [enumValue, enumName] : enumMap) {
									const bool bIsCurrent = enumValue == currentValue;
									if (ImGui::Selectable(enumName.data(), bIsCurrent)) {
										if (!bIsCurrent) {
											currentValue = enumValue;
											a_function->SetEssential(currentValue);
											a_functionSet->SetDirty(true);
											a_bOutSetDirty = true;
										}
									}
									if (bIsCurrent) {
										ImGui::SetItemDefaultFocus();
									}
								}
								ImGui::EndCombo();
							}
							ImGui::PopID();
							UICommon::AddTooltip("Users missing a plugin implementing this function won't be notified about an error and the function's return value will either be true or false, depending on the selected option.");
						}
					}
				}

				if (auto numComponents = a_function->GetNumComponents(); numComponents > 0) {
					for (uint32_t i = 0; i < numComponents; i++) {
						auto component = a_function->GetComponent(i);
						const bool bIsMultiFunctionComponent = component->GetType() == Functions::FunctionComponentType::kMulti;
						const bool bIsConditionComponent = component->GetType() == Functions::FunctionComponentType::kCondition;
						if (bIsMultiFunctionComponent) {
							const auto multiFunctionComponent = static_cast<Functions::IMultiFunctionComponent*>(component);
							// draw functions
							if (DrawFunctionSet(multiFunctionComponent->GetFunctions(), a_parentSubMod, a_editMode, Functions::FunctionSetType::kNone, a_refrToEvaluate, true, cursorPos)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
							// display component
							if (component->DisplayInUI(a_editMode != EditMode::kNone, _firstColumnWidthPercent)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
						} else if (bIsConditionComponent) {
							const auto conditionFunctionComponent = static_cast<Functions::IConditionFunctionComponent*>(component);
							// draw conditions
							if (DrawConditionSet(conditionFunctionComponent->GetConditions(), a_parentSubMod, a_editMode, Conditions::ConditionType::kNormal, a_refrToEvaluate, true, cursorPos)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
							ImGui::Separator();
							// draw functions
							if (DrawFunctionSet(conditionFunctionComponent->GetFunctions(), a_parentSubMod, a_editMode, Functions::FunctionSetType::kNone, a_refrToEvaluate, true, cursorPos)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
							// display component
							if (component->DisplayInUI(a_editMode != EditMode::kNone, _firstColumnWidthPercent)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
						} else {
							ImGui::Spacing();
							// write component name aligned to the right
							const auto componentName = component->GetName();
							UICommon::TextDescriptionRightAligned(componentName.data());
							// show component description on mouseover
							const auto componentDescription = component->GetDescription();
							if (!componentDescription.empty()) {
								UICommon::AddTooltip(componentDescription.data());
							}
							// display component
							if (component->DisplayInUI(a_editMode != EditMode::kNone, _firstColumnWidthPercent)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
						}
					}
				}

				// triggers
				if (a_functionSetType == Functions::FunctionSetType::kOnTrigger) {
					std::string triggersLabel = std::format("Triggers##{}", reinterpret_cast<uintptr_t>(a_function.get()));
					bool bIsOpen = ImGui::CollapsingHeader(triggersLabel.data(), ImGuiTreeNodeFlags_DefaultOpen);
					ImGui::SameLine();
					UICommon::HelpMarker("Animation events with an optional payload that will trigger this function.");
					if (bIsOpen) {
						ImGui::Indent();
						uint32_t i = 0;
						a_function->ForEachTrigger([&](const auto& a_trigger) {
							// draw trigger
							ImGui::TextUnformatted(a_trigger.event.data());
							if (a_trigger.payload.length() > 0) {
								ImGui::SameLine(0.f, 0.f);
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::EVENT_LOG_PAYLOAD_COLOR);
								ImGui::TextUnformatted(".");
								ImGui::SameLine(0.f, 0.f);
								UICommon::TextUnformattedEllipsis(a_trigger.payload.data());
								ImGui::PopStyleColor();
							}

							// remove trigger button
							UICommon::SecondColumn(_firstColumnWidthPercent);
							std::string buttonLabel = std::format("Delete trigger##{}{}", reinterpret_cast<uintptr_t>(a_function.get()), i++);
							if (ImGui::Button(buttonLabel.data())) {
								OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveTriggerJob>(a_function, a_functionSet, a_trigger);
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
							return RE::BSVisit::BSVisitControl::kContinue;
						});

						// add new trigger
						if (a_editMode > EditMode::kNone) {
							constexpr auto popupName = "Adding new trigger"sv;
							std::string buttonLabel = std::format("Add new trigger##{}", reinterpret_cast<uintptr_t>(a_function.get()));
							if (ImGui::Button(buttonLabel.data())) {
								const auto popupPos = ImGui::GetCursorScreenPos();
								ImGui::SetNextWindowPos(popupPos);
								ImGui::OpenPopup(popupName.data());
							}

							if (ImGui::BeginPopupModal(popupName.data(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
								static std::string eventBuffer;
								static std::string payloadBuffer;
								ImGui::InputTextWithHint("##NewTriggerEvent", "Event name", &eventBuffer, ImGuiInputTextFlags_CharsNoBlank);
								ImGui::SameLine();
								ImGui::TextUnformatted(".");
								ImGui::SameLine();
								ImGui::InputTextWithHint("##NewTriggerPayload", "Payload (optional)", &payloadBuffer, ImGuiInputTextFlags_CharsNoBlank);
								std::string addButtonLabel = std::format("Add trigger##{}", reinterpret_cast<uintptr_t>(a_function.get()));
								ImGui::BeginDisabled(eventBuffer.empty());
								if (ImGui::Button(addButtonLabel.data())) {
									auto trigger = Functions::Trigger(eventBuffer.data(), payloadBuffer.data());
									OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::AddTriggerJob>(a_function, a_functionSet, trigger);
									a_functionSet->SetDirty(true);
									a_bOutSetDirty = true;
									eventBuffer.clear();
									payloadBuffer.clear();
									ImGui::CloseCurrentPopup();
								}
								ImGui::EndDisabled();
								ImGui::SetItemDefaultFocus();
								ImGui::SameLine();
								if (ImGui::Button("Cancel")) {
									eventBuffer.clear();
									payloadBuffer.clear();
									ImGui::CloseCurrentPopup();
								}
								ImGui::EndPopup();
							}
						}

						ImGui::Unindent();
					}
				}

				ImGui::Spacing();

				ImGui::TreePop();
			}

			if (bStyleVarPushed) {
				ImGui::PopStyleVar();
			}

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();  // ImGuiStyleVar_CellPadding

		auto rectMax = ImGui::GetItemRectMax();

		auto width = ImGui::GetItemRectSize().x;
		auto groupEnd = ImGui::GetCursorPos();
		const ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 invisibleButtonStart = groupEnd;
		invisibleButtonStart.y -= style.ItemSpacing.y;
		ImGui::SetCursorPos(invisibleButtonStart);
		std::string functionInvisibleDragAreaId = std::format("{}functionInvisibleDragArea", reinterpret_cast<uintptr_t>(a_function.get()));
		ImGui::InvisibleButton(functionInvisibleDragAreaId.data(), ImVec2(width, style.ItemSpacing.y));

		// Drag & Drop target - invisible button
		if (a_editMode > EditMode::kNone) {
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_FUNCTION", ImGuiDragDropFlags_AcceptPeekOnly)) {
					DragFunctionPayload payload = *static_cast<DragFunctionPayload*>(imguiPayload->Data);
					// Draw our own preview of the drop because we want to draw a line below the condition

					ImDrawList* drawList = ImGui::GetWindowDrawList();
					drawList->AddLine(ImVec2(nodeRect.Min.x, rectMax.y + style.ItemSpacing.y * 0.5f), ImVec2(nodeRect.Max.x, rectMax.y + style.ItemSpacing.y * 0.5f), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

					if (imguiPayload->IsDelivery()) {
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveFunctionJob>(payload.function, payload.functionSet, a_function, a_functionSet, true);
					}
				}
				ImGui::EndDragDropTarget();
			}
		}

		ImGui::SetCursorPos(groupEnd);

		//ImGui::EndGroup();

		return functionRect;
	}

	ImRect UIMain::DrawBlankCondition(Conditions::ConditionSet* a_conditionSet, EditMode a_editMode, Conditions::ConditionType a_conditionType)
	{
		ImRect conditionRect;

		const std::string conditionTableId = std::format("{}blankConditionTable", reinterpret_cast<uintptr_t>(a_conditionSet));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
		if (ImGui::BeginTable(conditionTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			const float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;

			const std::string nodeId = std::format("No conditions##{}", reinterpret_cast<uintptr_t>(a_conditionSet));
			UICommon::TreeNodeCollapsedLeaf(nodeId.data(), ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);

			if (a_editMode > EditMode::kNone) {
				// Paste condition context menu
				if (ImGui::BeginPopupContextItem()) {
					// paste button
					const bool bPasteEnabled = _conditionCopy && !(a_conditionType == Conditions::ConditionType::kPreset && ConditionContainsPreset(_conditionCopy.get()));
					ImGui::BeginDisabled(!bPasteEnabled);
					if (ImGui::Button("Paste condition below")) {
						auto duplicate = DuplicateCondition(_conditionCopy);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::InsertConditionJob>(duplicate, a_conditionSet, nullptr);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();
					// paste tooltip
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
						ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
						ImGui::BeginTooltip();
						bool bDummy = false;
						DrawCondition(_conditionCopy, a_conditionSet, nullptr, EditMode::kNone, Conditions::ConditionType::kNormal, nullptr, bDummy);
						ImGui::EndTooltip();
					}

					ImGui::EndPopup();
				}

				// Drag & Drop target - blank condition set
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_CONDITION")) {
						DragConditionPayload payload = *static_cast<DragConditionPayload*>(imguiPayload->Data);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveConditionJob>(payload.condition, payload.conditionSet, nullptr, a_conditionSet, true);
					}
					ImGui::EndDragDropTarget();
				}
			}

			conditionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();  // ImGuiStyleVar_CellPadding

		return conditionRect;
	}

	ImRect UIMain::DrawBlankFunction(Functions::FunctionSet* a_functionSet, SubMod* a_parentSubMod, EditMode a_editMode, Functions::FunctionSetType a_functionSetType)
	{
		ImRect functionRect;

		auto id = reinterpret_cast<uintptr_t>(a_parentSubMod) + static_cast<uint8_t>(a_functionSetType) + reinterpret_cast<uintptr_t>(a_functionSet);
		const std::string functionTableId = std::format("{}blankFunctionTable", id);
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
		if (ImGui::BeginTable(functionTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			const float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;

			const std::string nodeId = std::format("No functions##{}", id);
			UICommon::TreeNodeCollapsedLeaf(nodeId.data(), ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);

			if (a_editMode > EditMode::kNone) {
				// Paste function context menu
				if (ImGui::BeginPopupContextItem()) {
					// paste button
					const bool bPasteEnabled = _functionCopy != nullptr;
					ImGui::BeginDisabled(!bPasteEnabled);
					if (ImGui::Button("Paste function below")) {
						auto duplicate = DuplicateFunction(_functionCopy);
						if (!a_functionSet) {
							a_functionSet = a_parentSubMod->CreateOrGetFunctionSet(a_functionSetType);
						}
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::InsertFunctionJob>(duplicate, a_functionSet, nullptr);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();
					// paste tooltip
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
						ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
						ImGui::BeginTooltip();
						bool bDummy = false;
						DrawFunction(_functionCopy, a_functionSet, nullptr, EditMode::kNone, a_functionSetType, nullptr, bDummy);
						ImGui::EndTooltip();
					}

					ImGui::EndPopup();
				}

				// Drag & Drop target - blank function set
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_FUNCTION")) {
						DragFunctionPayload payload = *static_cast<DragFunctionPayload*>(imguiPayload->Data);
						if (!a_functionSet) {
							a_functionSet = a_parentSubMod->CreateOrGetFunctionSet(a_functionSetType);
						}
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveFunctionJob>(payload.function, payload.functionSet, nullptr, a_functionSet, true);
					}
					ImGui::EndDragDropTarget();
				}
			}

			functionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();  // ImGuiStyleVar_CellPadding

		return functionRect;
	}

	bool UIMain::DrawConditionPreset(ReplacerMod* a_replacerMod, Conditions::ConditionPreset* a_conditionPreset, bool& a_bOutWasPresetRenamed)
	{
		bool bSetDirty = false;

		const std::string conditionPresetTableId = std::format("{}conditionPresetTable", reinterpret_cast<uintptr_t>(a_conditionPreset));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, UICommon::CONDITION_PRESET_BORDER_COLOR);
		if (ImGui::BeginTable(conditionPresetTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			ImGui::PopStyleColor();  // ImGuiCol_TableBorderStrong

			const bool bNodeOpen = ImGui::TreeNodeEx(a_conditionPreset, ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");

			// set dirty on all submods that have the preset
			auto setDirtyOnContainingSubMods = [&](Conditions::ConditionPreset* conditionPreset) {
				a_replacerMod->ForEachSubMod([&](SubMod* a_subMod) {
					if (ConditionSetContainsPreset(a_subMod->GetConditionSet(), conditionPreset)) {
						a_subMod->SetDirty(true);
					}
					return RE::BSVisit::BSVisitControl::kContinue;
				});
			};

			// context menu
			if (_editMode != EditMode::kNone) {
				if (ImGui::BeginPopupContextItem()) {
					// delete button
					const std::string buttonText = "Delete condition preset";
					const auto& style = ImGui::GetStyle();
					const auto xButtonSize = ImGui::CalcTextSize(buttonText.data()).x + style.FramePadding.x * 2 + style.ItemSpacing.x;
					UICommon::ButtonWithConfirmationModal(
						buttonText, "Are you sure you want to remove this condition preset?\nThis operation cannot be undone!\n\n"sv, [&]() {
							ImGui::ClosePopupsExceptModals();
							OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveConditionPresetJob>(a_replacerMod, a_conditionPreset->GetName());
							setDirtyOnContainingSubMods(a_conditionPreset);
							bSetDirty = true;
						},
						ImVec2(xButtonSize, 0));
					ImGui::EndPopup();
				}
			}

			// condition preset name
			ImGui::SameLine();
			if (!a_conditionPreset->IsEmpty() && a_conditionPreset->IsValid()) {
				ImGui::TextUnformatted(a_conditionPreset->GetName().data());
			} else {
				UICommon::TextUnformattedColored(UICommon::INVALID_CONDITION_COLOR, a_conditionPreset->GetName().data());
			}

			// right column, condition count text
			UICommon::SecondColumn(_firstColumnWidthPercent);
			ImGui::TextUnformatted(a_conditionPreset->NumText().data());

			if (bNodeOpen) {
				const ImGuiStyle& style = ImGui::GetStyle();

				ImGui::Spacing();
				// rename / delete condition preset
				if (_editMode != EditMode::kNone) {
					const std::string nameId = "##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "name";
					ImGui::SetNextItemWidth(UICommon::FirstColumnWidth(_firstColumnWidthPercent));
					std::string tempName(a_conditionPreset->GetName());
					if (ImGui::InputText(nameId.data(), &tempName, ImGuiInputTextFlags_EnterReturnsTrue)) {
						if (tempName.size() > 2 && !a_replacerMod->HasConditionPreset(tempName)) {
							a_conditionPreset->SetName(tempName);
							setDirtyOnContainingSubMods(a_conditionPreset);
							a_bOutWasPresetRenamed = true;
							bSetDirty = true;
						}
					}

					UICommon::SecondColumn(_firstColumnWidthPercent);

					UICommon::ButtonWithConfirmationModal("Delete condition preset"sv, "Are you sure you want to remove this condition preset?\nThis operation cannot be undone!\n\n"sv, [&]() {
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveConditionPresetJob>(a_replacerMod, a_conditionPreset->GetName());
						setDirtyOnContainingSubMods(a_conditionPreset);
						bSetDirty = true;
					});

					ImGui::Spacing();
				}

				// description
				const std::string descriptionId = "##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_conditionPreset)) + "description";
				if (_editMode != EditMode::kNone) {
					std::string tempDescription(a_conditionPreset->GetDescription());
					ImGui::SetNextItemWidth(UICommon::FirstColumnWidth(_firstColumnWidthPercent));
					if (ImGui::InputText(descriptionId.data(), &tempDescription)) {
						a_conditionPreset->SetDescription(tempDescription);
						a_replacerMod->SetDirty(true);
					}
					UICommon::SecondColumn(_firstColumnWidthPercent);
					UICommon::TextUnformattedDisabled("Condition preset description");
					ImGui::Spacing();
				} else if (!a_conditionPreset->GetDescription().empty()) {
					UICommon::TextUnformattedWrapped(a_conditionPreset->GetDescription().data());
					ImGui::Spacing();
				}

				ImVec2 pos = ImGui::GetCursorScreenPos();
				pos.x += style.FramePadding.x;
				pos.y += style.FramePadding.y;
				ImGui::PushID(a_conditionPreset);
				if (DrawConditionSet(a_conditionPreset, nullptr, _editMode, Conditions::ConditionType::kPreset, UIManager::GetSingleton().GetRefrToEvaluate(), true, pos)) {
					bSetDirty = true;
				}
				ImGui::PopID();

				ImGui::TreePop();
			}

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();  // ImGuiStyleVar_CellPadding

		return bSetDirty;
	}

	void UIMain::DrawInfoTooltip(const Info& a_info, ImGuiHoveredFlags a_flags)
	{
		if (ImGui::IsItemHovered(a_flags)) {
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8, 8 });
			if (ImGui::BeginTooltip()) {
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
				ImGui::TextUnformatted(a_info.description.data());
				if (!a_info.requiredPluginName.empty()) {
					ImGui::TextUnformatted("Source plugin:");
					ImGui::SameLine();
					UICommon::TextUnformattedColored(a_info.textColor, a_info.requiredPluginName.data());
					ImGui::SameLine();
					UICommon::TextUnformattedDisabled(a_info.requiredVersion.string("."sv).data());
					if (!a_info.requiredPluginAuthor.empty()) {
						ImGui::SameLine();
						ImGui::TextUnformatted("by");
						ImGui::SameLine();
						UICommon::TextUnformattedColored(a_info.textColor, a_info.requiredPluginAuthor.data());
					}
				}
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
			ImGui::PopStyleVar();
		}
	}

	void UIMain::UnloadedAnimationsWarning()
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Animations from behavior projects that are not loaded will not show up here!");
		ImGui::SameLine();
		UICommon::HelpMarker("If the ones you're looking for are missing, make sure a character using them has been loaded by the game in this gameplay session.");
	}

	bool UIMain::CanPreviewAnimation(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation)
	{
		if (a_refr) {
			RE::BSAnimationGraphManagerPtr graphManager = nullptr;
			a_refr->GetAnimationGraphManager(graphManager);
			if (graphManager) {
				const auto& activeGraph = graphManager->graphs[graphManager->GetRuntimeData().activeGraph];
				if (activeGraph->behaviorGraph) {
					if (const auto stringData = activeGraph->characterInstance.setup->data->stringData) {
						if (stringData->name.data() == a_replacementAnimation->GetProjectName()) {
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	bool UIMain::IsPreviewingAnimation(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex)
	{
		if (a_refr) {
			RE::BSAnimationGraphManagerPtr graphManager = nullptr;
			a_refr->GetAnimationGraphManager(graphManager);
			if (graphManager) {
				const auto& activeGraph = graphManager->graphs[graphManager->GetRuntimeData().activeGraph];
				if (activeGraph->behaviorGraph) {
					if (const auto activeAnimationPreview = OpenAnimationReplacer::GetSingleton().GetActiveAnimationPreview(activeGraph->behaviorGraph)) {
						if (activeAnimationPreview->GetReplacementAnimation() == a_replacementAnimation) {
							if (!a_variantIndex) {
								return true;
							}

							if (activeAnimationPreview->GetCurrentBindingIndex() == a_variantIndex) {
								return true;
							}
						}
					}
				}
			}
		}

		return false;
	}

	float UIMain::GetPreviewButtonsWidth(const ReplacementAnimation* a_replacementAnimation, bool a_bIsPreviewing)
	{
		const auto& style = ImGui::GetStyle();

		if (a_bIsPreviewing) {
			return ImGui::CalcTextSize("Stop").x + style.FramePadding.x * 2 + style.ItemSpacing.x;
		}

		if (a_replacementAnimation->IsSynchronizedAnimation()) {
			return (ImGui::CalcTextSize("Preview source").x + style.FramePadding.x * 2 + style.ItemSpacing.x) + (ImGui::CalcTextSize("Preview target").x + style.FramePadding.x * 2 + style.ItemSpacing.x);
		}

		return (ImGui::CalcTextSize("Preview").x + style.FramePadding.x * 2 + style.ItemSpacing.x);
	}

	void UIMain::DrawPreviewButtons(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, float a_previewButtonWidth, bool a_bCanPreview, bool a_bIsPreviewing, Variant* a_variant)
	{
		const float offset = a_variant ? -10.f : 0.f;

		auto obj = a_variant ? reinterpret_cast<const void*>(a_variant) : reinterpret_cast<const void*>(a_replacementAnimation);

		if (a_bIsPreviewing) {
			ImGui::SameLine(ImGui::GetContentRegionMax().x - a_previewButtonWidth + offset);
			const std::string label = std::format("Stop##{}", reinterpret_cast<uintptr_t>(obj));
			if (ImGui::Button(label.data())) {
				OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::StopPreviewAnimationJob>(a_refr);
			}
		} else if (a_bCanPreview) {
			ImGui::SameLine(ImGui::GetContentRegionMax().x - a_previewButtonWidth + offset);
			if (a_replacementAnimation->IsSynchronizedAnimation()) {
				const std::string sourceLabel = std::format("Preview source##{}", reinterpret_cast<uintptr_t>(obj));
				if (ImGui::Button(sourceLabel.data())) {
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::BeginPreviewAnimationJob>(a_refr, a_replacementAnimation, Settings::synchronizedClipSourcePrefix, a_variant);
				}
				ImGui::SameLine();
				const std::string targetLabel = std::format("Preview target##{}", reinterpret_cast<uintptr_t>(obj));
				if (ImGui::Button(targetLabel.data())) {
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::BeginPreviewAnimationJob>(a_refr, a_replacementAnimation, Settings::synchronizedClipTargetPrefix, a_variant);
				}
			} else {
				const std::string label = std::format("Preview##{}", reinterpret_cast<uintptr_t>(obj));
				if (ImGui::Button(label.data())) {
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::BeginPreviewAnimationJob>(a_refr, a_replacementAnimation, a_variant);
				}
			}
		}
	}

	int UIMain::ReferenceInputTextCallback(struct ImGuiInputTextCallbackData* a_data)
	{
		RE::FormID formID;
		auto [ptr, ec]{ std::from_chars(a_data->Buf, a_data->Buf + a_data->BufTextLen, formID, 16) };
		if (ec == std::errc()) {
			UIManager::GetSingleton().SetRefrToEvaluate(RE::TESForm::LookupByID<RE::TESObjectREFR>(formID));
		} else {
			UIManager::GetSingleton().SetRefrToEvaluate(nullptr);
		}

		return 0;
	}

	bool UIMain::BeginDragDropSourceEx(ImGuiDragDropFlags a_flags /*= 0*/, ImVec2 a_tooltipSize /*= ImVec2(0, 0)*/)
	{
		using namespace ImGui;

		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;

		// FIXME-DRAGDROP: While in the common-most "drag from non-zero active id" case we can tell the mouse button,
		// in both SourceExtern and id==0 cases we may requires something else (explicit flags or some heuristic).
		ImGuiMouseButton mouse_button = ImGuiMouseButton_Left;

		bool source_drag_active = false;
		ImGuiID source_id = 0;
		ImGuiID source_parent_id = 0;
		if (!(a_flags & ImGuiDragDropFlags_SourceExtern)) {
			source_id = g.LastItemData.ID;
			if (source_id != 0) {
				// Common path: items with ID
				if (g.ActiveId != source_id)
					return false;
				if (g.ActiveIdMouseButton != -1)
					mouse_button = g.ActiveIdMouseButton;
				if (g.IO.MouseDown[mouse_button] == false || window->SkipItems)
					return false;
				g.ActiveIdAllowOverlap = false;
			} else {
				// Uncommon path: items without ID
				if (g.IO.MouseDown[mouse_button] == false || window->SkipItems)
					return false;
				if ((g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HoveredRect) == 0 && (g.ActiveId == 0 || g.ActiveIdWindow != window))
					return false;

				// If you want to use BeginDragDropSource() on an item with no unique identifier for interaction, such as Text() or Image(), you need to:
				// A) Read the explanation below, B) Use the ImGuiDragDropFlags_SourceAllowNullID flag.
				if (!(a_flags & ImGuiDragDropFlags_SourceAllowNullID)) {
					IM_ASSERT(0);
					return false;
				}

				// Magic fallback to handle items with no assigned ID, e.g. Text(), Image()
				// We build a throwaway ID based on current ID stack + relative AABB of items in window.
				// THE IDENTIFIER WON'T SURVIVE ANY REPOSITIONING/RESIZINGG OF THE WIDGET, so if your widget moves your dragging operation will be canceled.
				// We don't need to maintain/call ClearActiveID() as releasing the button will early out this function and trigger !ActiveIdIsAlive.
				// Rely on keeping other window->LastItemXXX fields intact.
				source_id = g.LastItemData.ID = window->GetIDFromRectangle(g.LastItemData.Rect);
				KeepAliveID(source_id);
				const bool is_hovered = ItemHoverable(g.LastItemData.Rect, source_id);
				if (is_hovered && g.IO.MouseClicked[mouse_button]) {
					SetActiveID(source_id, window);
					FocusWindow(window);
				}
				if (g.ActiveId == source_id)  // Allow the underlying widget to display/return hovered during the mouse release frame, else we would get a flicker.
					g.ActiveIdAllowOverlap = is_hovered;
			}
			if (g.ActiveId != source_id)
				return false;
			source_parent_id = window->IDStack.back();
			source_drag_active = IsMouseDragging(mouse_button);

			// Disable navigation and key inputs while dragging + cancel existing request if any
			SetActiveIdUsingAllKeyboardKeys();
		} else {
			window = nullptr;
			source_id = ImHashStr("#SourceExtern");
			source_drag_active = true;
		}

		if (source_drag_active) {
			if (!g.DragDropActive) {
				IM_ASSERT(source_id != 0);
				ClearDragDrop();
				ImGuiPayload& payload = g.DragDropPayload;
				payload.SourceId = source_id;
				payload.SourceParentId = source_parent_id;
				g.DragDropActive = true;
				g.DragDropSourceFlags = a_flags;
				g.DragDropMouseButton = mouse_button;
				if (payload.SourceId == g.ActiveId)
					g.ActiveIdNoClearOnFocusLoss = true;
			}
			g.DragDropSourceFrameCount = g.FrameCount;
			g.DragDropWithinSource = true;

			if (!(a_flags & ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
				// Target can request the Source to not display its tooltip (we use a dedicated flag to make this request explicit)
				// We unfortunately can't just modify the source flags and skip the call to BeginTooltip, as caller may be emitting contents.
				SetNextWindowSize(a_tooltipSize);  // <--- ADDED
				BeginTooltipEx(ImGuiTooltipFlags_None, ImGuiWindowFlags_None);
				if (g.DragDropAcceptIdPrev && (g.DragDropAcceptFlags & ImGuiDragDropFlags_AcceptNoPreviewTooltip)) {
					ImGuiWindow* tooltip_window = g.CurrentWindow;
					tooltip_window->Hidden = tooltip_window->SkipItems = true;
					tooltip_window->HiddenFramesCanSkipItems = 1;
				}
			}

			if (!(a_flags & ImGuiDragDropFlags_SourceNoDisableHover) && !(a_flags & ImGuiDragDropFlags_SourceExtern))
				g.LastItemData.StatusFlags &= ~ImGuiItemStatusFlags_HoveredRect;

			return true;
		}
		return false;
	}

	bool UIMain::ConditionContainsPreset(Conditions::ICondition* a_condition, Conditions::ConditionPreset* a_conditionPreset) const
	{
		if (a_condition == nullptr) {
			return false;
		}

		//if (a_condition->GetConditionType() == Conditions::ConditionType::kPreset) {
		//	return true;
		//}

		if (const auto numComponents = a_condition->GetNumComponents(); numComponents > 0) {
			for (uint32_t i = 0; i < numComponents; i++) {
				const auto component = a_condition->GetComponent(i);
				if (component->GetType() == Conditions::ConditionComponentType::kPreset) {
					if (a_conditionPreset == nullptr) {
						return true;
					}
					// check if equals the given condition preset
					const auto conditionPresetComponent = static_cast<Conditions::ConditionPresetComponent*>(component);
					if (conditionPresetComponent->conditionPreset == a_conditionPreset) {
						return true;
					}
				}

				if (component->GetType() == Conditions::ConditionComponentType::kMulti) {
					const auto multiConditionComponent = static_cast<Conditions::IMultiConditionComponent*>(component);
					if (const auto conditionSet = multiConditionComponent->GetConditions()) {
						if (ConditionSetContainsPreset(conditionSet, a_conditionPreset)) {
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	bool UIMain::ConditionSetContainsPreset(Conditions::ConditionSet* a_conditionSet, Conditions::ConditionPreset* a_conditionPreset) const
	{
		if (a_conditionSet == nullptr) {
			return false;
		}

		const auto result = a_conditionSet->ForEach([&](std::unique_ptr<Conditions::ICondition>& a_condition) {
			if (ConditionContainsPreset(a_condition.get(), a_conditionPreset)) {
				return RE::BSVisit::BSVisitControl::kStop;
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		return result == RE::BSVisit::BSVisitControl::kStop;
	}
}
