#include "UIMain.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include "UIManager.h"
#include "UICommon.h"
#include "ActiveClip.h"
#include "OpenAnimationReplacer.h"
#include "AnimationFileHashCache.h"
#include "DetectedProblems.h"
#include "Jobs.h"
#include "Parsing.h"

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
            playerCamera->idleTimer = 0.f;
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

                if (const auto consoleRefr = UIManager::GetConsoleRefr()) {
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

            const std::string settingsButtonName = _bShowSettings ? "Settings <" : "Settings >";
            const float settingsButtonWidth = (ImGui::CalcTextSize(settingsButtonName.data()).x + style.FramePadding.x * 2 + style.ItemSpacing.x);

            // Bottom bar
            if (ImGui::BeginChild("BottomBar", ImVec2(ImGui::GetContentRegionAvail().x - animationLogButtonWidth - settingsButtonWidth, 0.f), true)) {
                ImGui::AlignTextToFramePadding();
                // Status text

                const auto& problems = DetectedProblems::GetSingleton();

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
                        ImGui::OpenPopup("Problems");
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
                    if (problems.HasSubModsSharingPriority()) {
                        height += 200.f;
                        height += problems.NumSubModsSharingPriority() * ImGui::GetTextLineHeightWithSpacing();
                    }
                    const float buttonHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f;
                    height += buttonHeight;

                    height = std::fmin(height, 600.f);

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
            ImGui::SameLine(ImGui::GetWindowWidth() - animationLogButtonWidth - settingsButtonWidth);
            if (ImGui::Button(animationLogButtonName.data(), ImVec2(0.f, bottomBarHeight - style.ItemSpacing.y))) {
                Settings::bEnableAnimationLog = !Settings::bEnableAnimationLog;
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
    }

    void UIMain::OnClose()
    {
        UIManager::GetSingleton().RemoveInputConsumer();
    }

    void UIMain::DrawSettings(const ImVec2& a_pos)
    {
        ImGui::SetNextWindowPos(a_pos, ImGuiCond_None, ImVec2(0.f, 1.f));

        if (ImGui::Begin("Settings", &_bShowSettings, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            // General settings
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("General Settings");
            ImGui::Spacing();

            if (ImGui::Checkbox("Show Welcome Banner", &Settings::bShowWelcomeBanner)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Enable to show the welcome banner on startup.");

            constexpr uint16_t animLimitMin = 0x4000;
            const uint16_t animLimitMax = Settings::GetMaxAnimLimit();
            if (ImGui::SliderScalar("Animation Limit", ImGuiDataType_U16, &Settings::uAnimationLimit, &animLimitMin, &animLimitMax, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Set the animation limit per behavior project. The game will crash if you set this too high without increasing the heap size. The game is incapable of playing animations past the upper limit set here, there's no point trying to circumvent it through the .ini file.");

            constexpr uint32_t heapMin = 0x20000000;
            constexpr uint32_t heapMax = 0x7FC00000;
            if (ImGui::SliderScalar("Havok Heap Size", ImGuiDataType_U32, &Settings::uHavokHeapSize, &heapMin, &heapMax, "0x%X", ImGuiSliderFlags_AlwaysClamp)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Set the havok heap size. Takes effect after restarting the game. (Vanilla value is 0x20000000)");

            if (ImGui::Checkbox("Async Parsing", &Settings::bAsyncParsing)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Enable to asynchronously parse all the replacer mods on load. This dramatically speeds up the process. No real reason to disable this setting.");

            if (UICommon::InputKey("Menu Key", Settings::uToggleUIKeyData)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Set the key to toggle the UI.");

            static float tempScale = Settings::fUIScale;
            ImGui::SliderFloat("UI Scale", &tempScale, 1.f, 2.f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
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

            // Animation queue progress bar settings
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Animation Queue Progress Bar Settings");
            ImGui::Spacing();

            if (ImGui::Checkbox("Enable", &Settings::bEnableAnimationQueueProgressBar)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Enable to show a progress bar while animations are loading.");

            if (ImGui::SliderFloat("Linger Time", &Settings::fAnimationQueueLingerTime, 0.f, 10.f, "%.1f s", ImGuiSliderFlags_AlwaysClamp)) {
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

            constexpr uint32_t entriesMin = 1;
            constexpr uint32_t entriesMax = 20;
            if (ImGui::SliderScalar("Max Entries", ImGuiDataType_U32, &Settings::uAnimationLogMaxEntries, &entriesMin, &entriesMax, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                AnimationLog::GetSingleton().ClampLog();
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Set the maximum number of entries in the animation log.");

            const char* logTypes[] = { "When Replaced", "With Potential Replacements", "All" };
			if (ImGui::SliderInt("Activate Log Mode", reinterpret_cast<int*>(&Settings::uAnimationActivateLogMode), 0, 2, logTypes[Settings::uAnimationActivateLogMode])) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Set conditions in which animation clip activation should be logged.");

            if (ImGui::SliderInt("Echo Log Mode", reinterpret_cast<int*>(&Settings::uAnimationEchoLogMode), 0, 2, logTypes[Settings::uAnimationEchoLogMode])) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set conditions in which animation clip echo should be logged. (Echo is when an animation clip transitions into itself. Happens with some non-looping clips)");

			if (ImGui::SliderInt("Loop Log Mode", reinterpret_cast<int*>(&Settings::uAnimationLoopLogMode), 0, 2, logTypes[Settings::uAnimationLoopLogMode])) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Set conditions in which animation clip looping should be logged.");
			
            if (ImGui::Checkbox("Only Log Current Project", &Settings::bAnimationLogOnlyActiveGraph)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Enable to only log animation clips from the active behavior graph - filter out first person animations when in third person, etc.");

            if (ImGui::Checkbox("Write to Log File", &Settings::bAnimationLogWriteToTextLog)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Enable to also log the clips into the file at 'Documents\\My Games\\Skyrim Special Edition\\SKSE\\OpenAnimationReplacer.log'.");

            ImGui::Spacing();
            ImGui::Separator();

            // Experimental settings
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Experimental Settings");
            ImGui::SameLine();
            UICommon::HelpMarker("These settings are considered experimental and might not work correctly for you. They take effect after restarting the game.");
            ImGui::Spacing();

            if (ImGui::Checkbox("Disable Preloading", &Settings::bDisablePreloading)) {
                if (Settings::bDisablePreloading) {
                    Settings::bLoadDefaultBehaviorsInMainMenu = false;
                }
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Set to disable preloading all animations when the behavior is first loaded. This might not be necessary if your animations are loading fast enough. In my experience everything works perfectly fine with preloading disabled, but I'm leaving it enabled by default.");

            ImGui::BeginDisabled(Settings::bDisablePreloading);
            if (ImGui::Checkbox("Load Default Behaviors in Main Menu", &Settings::bLoadDefaultBehaviorsInMainMenu)) {
                Settings::WriteSettings();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            UICommon::HelpMarker("Enable to start loading default male/female behaviors in the main menu. Ignored with preloading disabled as there's no benefit in doing so in that case.");

            if (ImGui::Checkbox("Filter Out Duplicate Animations", &Settings::bFilterOutDuplicateAnimations)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Enable to check for duplicates before adding an animation. Only one copy of an animation binding will be used in multiple replacer animations. This should massively cut down on the number of loaded animations as replacer mods tend to use multiple copies of the same animation with different condition. Should work exactly the same but needs testing to ensure that it's flawless.");

            ImGui::BeginDisabled(!Settings::bFilterOutDuplicateAnimations);
            if (ImGui::Checkbox("Cache Animation File Hashes", &Settings::bCacheAnimationFileHashes)) {
                Settings::WriteSettings();
            }
            ImGui::SameLine();
            UICommon::HelpMarker("Enable to save a cache of animation file hashes, so the hashes don't have to be recalculated on every game launch. It's saved to a .bin file next to the .dll. This should speed up the loading process a little bit.");
            ImGui::SameLine();
            if (ImGui::Button("Clear Cache")) {
                AnimationFileHashCache::GetSingleton().DeleteCache();
            }
            UICommon::AddTooltip("Delete the animation file hash cache. This will cause the hashes to be recalculated on the next game launch.");
            ImGui::EndDisabled();

            if (ImGui::Checkbox("Increase Animation Limit", &Settings::bIncreaseAnimationLimit)) {
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
            ImGui::TableSetupColumn("Required", ImGuiTableColumnFlags_WidthFixed, 100.f);
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
                        ImGui::TextUnformatted(a_subMod->GetPath().data());
                        // tooltip for full path in case it doesn't fit
                        UICommon::AddTooltip(a_subMod->GetPath().data());
                    }
                    ImGui::TreePop();
                }
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
            OpenAnimationReplacer::GetSingleton().ForEachReplacerMod([&](ReplacerMod* a_replacerMod) {
                // Parse names for filtering and duplicate names
                std::unordered_map<std::string, SubModNameFilterResult> subModNameFilterResults{};
				bool bEntireReplacerModMatchesFilter = !std::strlen(nameFilterBuf) || Utils::ContainsStringIgnoreCase(a_replacerMod->GetName(), nameFilterBuf) || Utils::ContainsStringIgnoreCase(a_replacerMod->GetAuthor(), nameFilterBuf);
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
                    DrawReplacerMod(a_replacerMod, subModNameFilterResults);
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

        if (ImGui::TreeNode(a_replacerMod, a_replacerMod->GetName().data())) {
            // Mod name
            if (!a_replacerMod->IsLegacy() && _editMode == ConditionEditMode::kAuthor) {
                std::string nameId = "Mod name##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "name";
                ImGui::SetNextItemWidth(-150.f);
				std::string tempName(a_replacerMod->GetName());
                if (ImGui::InputTextWithHint(nameId.data(), "Mod name", &tempName)) {
					a_replacerMod->SetName(tempName);
                    a_replacerMod->SetDirty(true);
                }
            }

            // Mod author
            std::string authorId = "Author##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "author";
			if (!a_replacerMod->IsLegacy() && _editMode == ConditionEditMode::kAuthor) {
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
			if (!a_replacerMod->IsLegacy() && _editMode == ConditionEditMode::kAuthor) {
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
					ImGui::TextUnformatted(a_replacerMod->GetDescription().data());
                    ImGui::EndTable();
                }
                ImGui::Spacing();
            }

            // Submods
            ImGui::TextUnformatted("Submods:");
            a_replacerMod->ForEachSubMod([&](SubMod* a_subMod) {
                // Filter
                auto search = a_filterResults.find(a_subMod->GetName().data());
                if (search != a_filterResults.end()) {
                    const SubModNameFilterResult& filterResult = search->second;
                    if (filterResult.bDisplay) {
                        DrawSubMod(a_replacerMod, a_subMod, filterResult.bDuplicateName);
                    }
                }
                return RE::BSVisit::BSVisitControl::kContinue;
            });

            // Save mod config
			if (!a_replacerMod->IsLegacy() && _editMode == ConditionEditMode::kAuthor) {
                const bool bIsDirty = a_replacerMod->IsDirty();
                if (!bIsDirty) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("Save mod config")) {
                    a_replacerMod->SaveConfig(_editMode);
                }
                if (!bIsDirty) {
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
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * style.DisabledAlpha);
            bStyleVarPushed = true;
        }

        bool bNodeOpen = ImGui::TreeNodeEx(a_subMod, ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");

        // Disable checkbox
        ImGui::SameLine();
        if (_editMode > ConditionEditMode::kNone) {
            std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_subMod));
            ImGui::PushID(idString.data());
			bool bEnabled = !a_subMod->IsDisabled();
            if (ImGui::Checkbox("##disableSubMod", &bEnabled)) {
                a_subMod->SetDisabled(!bEnabled);
                OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
                a_subMod->SetDirty(true);
            }
            UICommon::AddTooltip("If unchecked, the submod will be disabled and none of its replacement animations will be considered.");
            ImGui::PopID();
        }

        if (a_subMod->IsDirty()) {
            ImGui::SameLine();
            ImGui::TextColored(UICommon::DIRTY_COLOR, "*");
        }

        switch (a_subMod->GetConfigSource()) {
        case Parsing::ConfigSource::kUser:
            ImGui::SameLine();
            ImGui::TextColored(UICommon::USER_MOD_COLOR, "(User)");
            break;
        case Parsing::ConfigSource::kLegacy:
            ImGui::SameLine();
            ImGui::TextDisabled("(Legacy)");
            break;
        case Parsing::ConfigSource::kLegacyActorBase:
            ImGui::SameLine();
            ImGui::TextDisabled("(Legacy ActorBase)");
            break;
        }

        // node name
        ImGui::SameLine();
		ImGui::TextUnformatted(a_subMod->GetName().data());
        ImGui::SameLine();

        if (a_bAddPathToName) {
            ImGui::TextDisabled(a_subMod->GetPath().data());
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
                if (_editMode != ConditionEditMode::kNone) {
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
                if (_editMode != ConditionEditMode::kNone) {
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
						ImGui::TextUnformatted(a_subMod->GetDescription().data());
                        ImGui::EndTable();
                    }
                }
            }

            // Submod priority
            {
                std::string priorityLabel = "Priority##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "priority";
                if (_editMode != ConditionEditMode::kNone) {
					int32_t tempPriority = a_subMod->GetPriority();
                    if (InputPriority(priorityLabel.data(), &tempPriority)) {
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
                if (_editMode != ConditionEditMode::kNone) {
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
                if (_editMode != ConditionEditMode::kNone) {
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
                std::string noTriggersFlagLabel = "Ignore No Triggers flag##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "ignoreNoTriggersFlag";
                std::string noTriggersTooltip = "If checked, the animation clip flag \"Don't convert annotations to triggers\", set on some animation clips in vanilla, will be ignored. This means that animation triggers (events), included in the replacer animation as annotations, will now run instead of being ignored.";

                if (_editMode != ConditionEditMode::kNone) {
					bool tempIgnoreNoTriggersFlag = a_subMod->IsIgnoringNoTriggersFlag();
                    if (ImGui::Checkbox(noTriggersFlagLabel.data(), &tempIgnoreNoTriggersFlag)) {
						a_subMod->SetIgnoringNoTriggersFlag(tempIgnoreNoTriggersFlag);
                        OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
                        a_subMod->SetDirty(true);
                    }
                    ImGui::SameLine();
                    UICommon::HelpMarker(noTriggersTooltip.data());
                } else if (a_subMod->IsIgnoringNoTriggersFlag()) {
                    ImGui::BeginDisabled();
					bool tempIgnoreNoTriggersFlag = a_subMod->IsIgnoringNoTriggersFlag();
                    ImGui::Checkbox(noTriggersFlagLabel.data(), &tempIgnoreNoTriggersFlag);
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    UICommon::HelpMarker(noTriggersTooltip.data());
                }
            }

            // Submod interruptible
            {
                std::string interruptibleLabel = "Interruptible##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "interruptible";
                std::string interruptibleTooltip = "If checked, the conditions will be checked every frame and the clip will be switched to another one if needed. Mostly useful for looping animations.";

                if (_editMode != ConditionEditMode::kNone) {
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

            // Submod keep random results on loop
            {
                std::string keepRandomLabel = "Keep random results on loop##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "keepRandom";
                std::string keepRandomTooltip = "If checked, the results of the random conditions won't be reset when the animation loops. Mostly useful for random movement animations that don't want to be interrupted after every step or two.";

                if (_editMode != ConditionEditMode::kNone) {
					bool tempKeepRandomResultsOnLoop = a_subMod->IsKeepingRandomResultsOnLoop();
                    if (ImGui::Checkbox(keepRandomLabel.data(), &tempKeepRandomResultsOnLoop)) {
						a_subMod->SetKeepRandomResultsOnLoop(tempKeepRandomResultsOnLoop);
                        OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
                        a_subMod->SetDirty(true);
                    }
                    ImGui::SameLine();
                    UICommon::HelpMarker(keepRandomTooltip.data());
                } else if (a_subMod->IsKeepingRandomResultsOnLoop()) {
                    ImGui::BeginDisabled();
					bool tempKeepRandomResultsOnLoop = a_subMod->IsKeepingRandomResultsOnLoop();
                    ImGui::Checkbox(keepRandomLabel.data(), &tempKeepRandomResultsOnLoop);
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    UICommon::HelpMarker(keepRandomTooltip.data());
                }
            }

            // Submod animations
            {
                std::string animationsTreeNodeLabel = "Replacement Animations##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "animationsNode";
                if (ImGui::CollapsingHeader(animationsTreeNodeLabel.data())) {
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                    std::string animationsTableId = std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "animationsTable";
                    if (ImGui::BeginTable(animationsTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
                        a_subMod->ForEachReplacementAnimation([&](const auto a_replacementAnimation) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::AlignTextToFramePadding();

                            if (_editMode != ConditionEditMode::kNone) {
                                const std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_replacementAnimation));
                                ImGui::PushID(idString.data());
                                bool bEnabled = !a_subMod->HasDisabledAnimation(a_replacementAnimation);
                                if (ImGui::Checkbox("##disableReplacementAnimation", &bEnabled)) {
                                    if (bEnabled) {
                                        a_subMod->RemoveDisabledAnimation(a_replacementAnimation);
                                    } else {
                                        a_subMod->AddDisabledAnimation(a_replacementAnimation);
                                    }
                                    OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
                                    a_subMod->SetDirty(true);
                                }
                                UICommon::AddTooltip("If unchecked, the replacement animation will be disabled and will not be considered.");
                                ImGui::PopID();
                                ImGui::SameLine();
                            }

                            ImGui::TextDisabled(std::format("[{}]", a_replacementAnimation->GetProjectName()).data());
                            ImGui::SameLine();
                            if (a_replacementAnimation->GetDisabled()) {
                                const ImGuiContext& ctx = *ImGui::GetCurrentContext();
                                ImGui::PushStyleColor(ImGuiCol_Text, ctx.Style.Colors[ImGuiCol_TextDisabled]);
                            }
                            ImGui::TextUnformatted(a_replacementAnimation->GetAnimPath().data());
                            if (a_replacementAnimation->GetDisabled()) {
                                ImGui::PopStyleColor();
                            }
                            // tooltip for full path in case it doesn't fit
                            UICommon::AddTooltip(a_replacementAnimation->GetAnimPath().data());
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
                DrawConditionSet(a_subMod->GetConditionSet(), _editMode, true, pos);
                ImGui::Unindent();

                ImGui::Spacing();
                ImGui::PopStyleVar();
            }

            if (_editMode != ConditionEditMode::kNone) {
                // Save submod config
                bool bIsDirty = a_subMod->IsDirty();
                if (!bIsDirty) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("Save submod config")) {
                    a_subMod->SaveConfig(_editMode);
                }
                if (!bIsDirty) {
                    ImGui::EndDisabled();
                }

                // Reload submod config
                ImGui::SameLine();
                UICommon::ButtonWithConfirmationModal("Reload config", "Are you sure you want to reload the config?\nThis operation cannot be undone!\n\n"sv, [&]() {
                    OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReloadSubModConfigJob>(a_subMod);
                });

                // delete user config
                if (_editMode == ConditionEditMode::kUser) {
                    bool bUserConfigExists = a_subMod->DoesUserConfigExist();
                    if (!bUserConfigExists) {
                        ImGui::BeginDisabled();
                    }
                    ImGui::SameLine();
                    UICommon::ButtonWithConfirmationModal("Delete user config", "Are you sure you want to delete the user config?\nThis operation cannot be undone!\n\n"sv, [&]() {
                        a_subMod->DeleteUserConfig();
                        OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReloadSubModConfigJob>(a_subMod);
                    });
                    if (!bUserConfigExists) {
                        ImGui::EndDisabled();
                    }
                }

                // copy json to clipboard button
				if (a_replacerMod->IsLegacy() && _editMode == ConditionEditMode::kAuthor) {
                    ImGui::SameLine();
                    if (ImGui::Button("Copy submod config to clipboard")) {
                        ImGui::LogToClipboard();
                        ImGui::LogText(a_subMod->SerializeToString().data());
                        ImGui::LogFinish();
                    }
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

        if (ImGui::BeginChild("Datas")) {
            OpenAnimationReplacer::GetSingleton().ForEachReplacerProjectData([&](RE::hkbCharacterStringData* a_stringData, const auto& a_projectData) {
                auto& name = a_stringData->name;

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

    void UIMain::DrawReplacementAnimation(const ReplacementAnimation* a_replacementAnimation)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        // Evaluate
        auto evalResult = ConditionEvaluateResult::kFailure;
        if (const auto refrToEvaluate = UIManager::GetSingleton().GetRefrToEvaluate()) {
			if (a_replacementAnimation->EvaluateConditions(refrToEvaluate, nullptr)) {
				if (Utils::ConditionSetHasRandomResult(a_replacementAnimation->GetConditionSet())) {
					evalResult = ConditionEvaluateResult::kRandom;
				} else {
					evalResult = ConditionEvaluateResult::kSuccess;
				}
			}
        }

        const std::string priorityText = "Priority: " + std::to_string(a_replacementAnimation->GetPriority());

        ImGui::PushID(&a_replacementAnimation);
        const bool bNodeOpen = ImGui::TreeNodeEx(priorityText.data(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);

        // Evaluate success/failure indicator
        if (UIManager::GetSingleton().GetRefrToEvaluate()) {
            UICommon::DrawConditionEvaluateResult(evalResult);
        }

        if (bNodeOpen) {
            const auto animPath = a_replacementAnimation->GetAnimPath();
            ImGui::TextUnformatted(animPath.data());
            UICommon::AddTooltip(animPath.data()); // tooltip for full path in case it doesn't fit
			DrawConditionSet(a_replacementAnimation->GetConditionSet(), ConditionEditMode::kNone, true, ImGui::GetCursorScreenPos());
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void UIMain::DrawConditionSet(Conditions::ConditionSet* a_conditionSet, ConditionEditMode a_editMode, bool a_bDrawLines, const ImVec2& a_drawStartPos)
    {
        //ImGui::TableNextRow();
        //ImGui::TableSetColumnIndex(0);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImGuiStyle& style = ImGui::GetStyle();

        ImVec2 vertLineStart = a_drawStartPos;
        vertLineStart.y += style.FramePadding.x;
        vertLineStart.x -= style.IndentSpacing * 0.6f;
        ImVec2 vertLineEnd = vertLineStart;

        const float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;

        if (!a_conditionSet->IsEmpty()) {
            a_conditionSet->ForEachCondition([&](std::unique_ptr<Conditions::ICondition>& a_condition) {
                const ImRect nodeRect = DrawCondition(a_condition, a_conditionSet, a_editMode);
                if (a_bDrawLines) {
                    const float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
                    constexpr float horLineLength = 10.f;
                    drawList->AddLine(ImVec2(vertLineStart.x, midPoint), ImVec2(vertLineStart.x + horLineLength, midPoint), ImGui::GetColorU32(UICommon::TREE_LINE_COLOR));
                    vertLineEnd.y = midPoint;
                }

                return RE::BSVisit::BSVisitControl::kContinue;
            });
        } else {
            DrawBlankCondition(a_conditionSet, a_editMode);
        }

        if (a_bDrawLines) {
            drawList->AddLine(vertLineStart, vertLineEnd, ImGui::GetColorU32(UICommon::TREE_LINE_COLOR));
        }

        if (a_editMode > ConditionEditMode::kNone) {
            // Add condition button
            if (ImGui::Button("Add new condition")) {
                if (auto newCondition = Conditions::CreateCondition(_lastAddNewConditionName)) {
                    a_conditionSet->AddCondition(newCondition, true);
                } else {
					auto isFormCondition = Conditions::CreateCondition("IsForm"sv);
					a_conditionSet->AddCondition(isFormCondition, true);
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

                if (ImGui::Button("Copy condition set", ImVec2(xButtonSize, 0))) {
                    ImGui::CloseCurrentPopup();
                    _conditionSetCopy = DuplicateConditionSet(a_conditionSet);
                }

                // Paste conditions button
                ImGui::BeginDisabled(_conditionSetCopy == nullptr);
                if (ImGui::Button("Paste condition set", ImVec2(xButtonSize, 0))) {
                    ImGui::CloseCurrentPopup();
                    const auto duplicatedSet = DuplicateConditionSet(_conditionSetCopy.get());
                    a_conditionSet->AppendConditions(duplicatedSet.get());
                }
                ImGui::EndDisabled();
                // Paste tooltip
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
                    ImGui::BeginTooltip();
					DrawConditionSet(_conditionSetCopy.get(), ConditionEditMode::kNone, false, a_drawStartPos);
                    ImGui::EndTooltip();
                }

                // Clear conditions button
                UICommon::ButtonWithConfirmationModal(
                    "Clear condition set"sv, "Are you sure you want to clear the condition set?\nThis operation cannot be undone!\n\n"sv, [&]() {
                        ImGui::ClosePopupsExceptModals();
                        OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ClearConditionSetJob>(a_conditionSet);
                    },
                    ImVec2(xButtonSize, 0));
                ImGui::EndPopup();
            }
        }
    }

    ImRect UIMain::DrawCondition(std::unique_ptr<Conditions::ICondition>& a_condition, Conditions::ConditionSet* a_conditionSet, ConditionEditMode a_editMode)
    {
        ImRect conditionRect;

        // Evaluate
        auto evalResult = ConditionEvaluateResult::kFailure;
        if (auto refrToEvaluate = UIManager::GetSingleton().GetRefrToEvaluate()) {
			if (a_condition->Evaluate(refrToEvaluate, nullptr)) {
				if (Utils::ConditionHasRandomResult(a_condition.get())) {
					evalResult = ConditionEvaluateResult::kRandom;
				} else {
					evalResult = ConditionEvaluateResult::kSuccess;
				}
			}
        }

        //ImGui::BeginGroup();
        ImRect nodeRect;

        std::string conditionTableId = std::format("{}conditionTable", reinterpret_cast<uintptr_t>(a_condition.get()));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
        if (ImGui::BeginTable(conditionTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            //ImGui::AlignTextToFramePadding();

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
            if (a_editMode > ConditionEditMode::kNone || a_condition->GetNumComponents() > 0) {
                bNodeOpen = ImGui::TreeNodeEx(a_condition.get(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
            } else {
                bNodeOpen = TreeNodeCollapsedLeaf(a_condition.get(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
                //bNodeOpen = ImGui::TreeNodeEx(a_condition.get(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
            }

            // Condition context menu
            if (a_editMode > ConditionEditMode::kNone) {
                if (ImGui::BeginPopupContextItem()) {
                    // copy button
                    auto& style = ImGui::GetStyle();
                    auto xButtonSize = ImGui::CalcTextSize("Paste condition below").x + style.FramePadding.x * 2 + style.ItemSpacing.x;
                    if (ImGui::Button("Copy condition", ImVec2(xButtonSize, 0))) {
                        _conditionCopy = DuplicateCondition(a_condition);
                        ImGui::CloseCurrentPopup();
                    }

                    // paste button
                    ImGui::BeginDisabled(!_conditionCopy);
                    if (ImGui::Button("Paste condition below", ImVec2(xButtonSize, 0))) {
						auto duplicate = DuplicateCondition(_conditionCopy);
                        OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::InsertConditionJob>(duplicate, a_conditionSet, a_condition);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndDisabled();
                    // paste tooltip
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                        ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
                        ImGui::BeginTooltip();
                        DrawCondition(_conditionCopy, a_conditionSet, ConditionEditMode::kNone);
                        ImGui::EndTooltip();
                    }

                    // delete button
                    UICommon::ButtonWithConfirmationModal("Delete condition"sv, "Are you sure you want to remove the condition?\nThis operation cannot be undone!\n\n"sv, [&]() {
                        ImGui::ClosePopupsExceptModals();
                        OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveConditionJob>(a_condition, a_conditionSet);
                    }, ImVec2(xButtonSize, 0));

                    ImGui::EndPopup();
                }
            }

            // Condition description tooltip
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8, 8 });
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
                const auto description = a_condition->GetDescription();
                ImGui::TextUnformatted(description.data());
                const auto requiredPluginName = a_condition->GetRequiredPluginName();
                if (!requiredPluginName.empty()) {
                    ImGui::TextUnformatted("Source plugin:");
                    ImGui::SameLine();
                    ImGui::TextColored(UICommon::CUSTOM_CONDITION_COLOR, requiredPluginName.data());
                    ImGui::SameLine();
                    ImGui::TextDisabled(a_condition->GetRequiredVersion().string("."sv).data());
					const auto requiredPluginAuthor = a_condition->GetRequiredPluginAuthor();
					if (!requiredPluginAuthor.empty()) {
					    ImGui::SameLine();
                        ImGui::TextUnformatted("by");
                        ImGui::SameLine();
                        ImGui::TextColored(UICommon::CUSTOM_CONDITION_COLOR, requiredPluginAuthor.data());
					}
                }
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                ImGui::PopStyleVar();
            }

            nodeRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

            // Drag & Drop source
            if (a_editMode > ConditionEditMode::kNone) {
                if (BeginDragDropSourceEx(ImGuiDragDropFlags_SourceNoHoldToOpenOthers, ImVec2(tooltipWidth, 0))) {
                    DragConditionPayload payload(a_condition, a_conditionSet);
                    ImGui::SetDragDropPayload("DND_CONDITION", &payload, sizeof(DragConditionPayload));
                    DrawCondition(a_condition, a_conditionSet, ConditionEditMode::kNone);
                    ImGui::EndDragDropSource();
                }
            }

            // Drag & Drop target - tree node
            if (a_editMode > ConditionEditMode::kNone) {
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
            if (a_editMode > ConditionEditMode::kNone) {
                std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_condition.get()));
                ImGui::PushID(idString.data());
                bool bEnabled = !a_condition->IsDisabled();
                if (ImGui::Checkbox("##toggleCondition", &bEnabled)) {
                    a_condition->SetDisabled(!bEnabled);
                    a_conditionSet->SetDirty(true);
                }
                UICommon::AddTooltip("Toggles the condition on/off");
                ImGui::PopID();
            }

            // Condition name
            ImGui::SameLine();
            auto requiredPluginName = a_condition->GetRequiredPluginName();
            if (!requiredPluginName.empty()) {
                ImGui::TextColored(UICommon::CUSTOM_CONDITION_COLOR, nodeName.data());
            } else {
                ImGui::TextUnformatted(nodeName.data());
            }

            ImVec2 cursorPos = ImGui::GetCursorScreenPos();

            // Right column, argument text
            UICommon::SecondColumn(_firstColumnWidthPercent);
            const auto argument = a_condition->GetArgument();
            ImGui::TextUnformatted(argument.data());

            //ImGui::TableSetColumnIndex(0);

            // Evaluate success/failure indicator
            if (UIManager::GetSingleton().GetRefrToEvaluate()) {
                UICommon::DrawConditionEvaluateResult(evalResult);
            }

            conditionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

            // Node contents
            if (bNodeOpen) {
                ImGui::Spacing();

                if (a_editMode > ConditionEditMode::kNone) {
                    // negate checkbox
                    bool bNOT = a_condition->IsNegated();
                    if (ImGui::Checkbox("Negate", &bNOT)) {
                        a_condition->SetNegated(bNOT);
                        a_conditionSet->SetDirty(true);
                    }
                    UICommon::AddTooltip("Negates the condition");

                    // select condition type
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(UICommon::FirstColumnWidth(_firstColumnWidthPercent));
                    bool bIsCustomCondition = OpenAnimationReplacer::GetSingleton().IsCustomCondition(conditionName);
                    if (bIsCustomCondition) {
                        ImGui::PushStyleColor(ImGuiCol_Text, UICommon::CUSTOM_CONDITION_COLOR);
                    }
                    if (ImGui::BeginCombo("##Condition type", conditionName.data())) {
                        if (bIsCustomCondition) {
                            ImGui::PopStyleColor();
                        }
                        OpenAnimationReplacer::GetSingleton().ForEachConditionFactory([&](std::string_view a_name, [[maybe_unused]] auto a_factory) {
                            const bool bIsCurrent = a_name.data() == conditionName;
                            const bool bEntryIsCustomCondition = OpenAnimationReplacer::GetSingleton().IsCustomCondition(a_name);
                            if (bEntryIsCustomCondition) {
                                ImGui::PushStyleColor(ImGuiCol_Text, UICommon::CUSTOM_CONDITION_COLOR);
                            }
                            if (ImGui::Selectable(a_name.data(), bIsCurrent)) {
                                if (!bIsCurrent) {
                                    OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReplaceConditionJob>(a_condition, a_name, a_conditionSet);
                                }
                                _lastAddNewConditionName = a_name;
                            }
                            if (bEntryIsCustomCondition) {
                                ImGui::PopStyleColor();
                            }
                            if (bIsCurrent) {
                                ImGui::SetItemDefaultFocus();
                            }
                        });
                        ImGui::EndCombo();
                    } else {
                        if (bIsCustomCondition) {
                            ImGui::PopStyleColor();
                        }
                    }

                    // remove condition button
                    UICommon::SecondColumn(_firstColumnWidthPercent);

                    UICommon::ButtonWithConfirmationModal("Delete condition"sv, "Are you sure you want to remove the condition?\nThis operation cannot be undone!\n\n"sv, [&]() {
                        OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveConditionJob>(a_condition, a_conditionSet);
                    });
                }

                if (auto numComponents = a_condition->GetNumComponents(); numComponents > 0) {
                    for (uint32_t i = 0; i < numComponents; i++) {
                        auto component = a_condition->GetComponent(i);

                        if (auto multiConditionComponent = dynamic_cast<Conditions::IMultiConditionComponent*>(component)) {
                            DrawConditionSet(multiConditionComponent->GetConditions(), a_editMode, true, cursorPos);
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
                            if (component->DisplayInUI(a_editMode != ConditionEditMode::kNone, _firstColumnWidthPercent)) {
                                a_conditionSet->SetDirty(true);
                            }
                        }
                    }
                }

                // Display current value, if applicable
                const auto current = a_condition->GetCurrent(UIManager::GetSingleton().GetRefrToEvaluate());
                if (!current.empty()) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Current:");
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
        ImGui::PopStyleVar(); // ImGuiStyleVar_CellPadding

        auto rectMin = ImGui::GetItemRectMin();
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
        if (a_editMode > ConditionEditMode::kNone) {
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

    ImRect UIMain::DrawBlankCondition(Conditions::ConditionSet* a_conditionSet, ConditionEditMode a_editMode)
    {
        ImRect conditionRect;

        const std::string conditionTableId = std::format("{}blankConditionTable", reinterpret_cast<uintptr_t>(a_conditionSet));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
        if (ImGui::BeginTable(conditionTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            const float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;

            const std::string nodeId = std::format("No conditions##{}", reinterpret_cast<uintptr_t>(a_conditionSet));
            TreeNodeCollapsedLeaf(nodeId.data(), ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);

            if (a_editMode > ConditionEditMode::kNone) {
                // Paste condition context menu
                if (ImGui::BeginPopupContextItem()) {
                    // paste button
                    ImGui::BeginDisabled(!_conditionCopy);
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
                        DrawCondition(_conditionCopy, a_conditionSet, ConditionEditMode::kNone);
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
        ImGui::PopStyleVar(); // ImGuiStyleVar_CellPadding

        return conditionRect;
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

    bool UIMain::InputPriority(const char* a_label, int* a_v, int a_step /*= 1*/, int a_stepFast /*= 100*/) const
    {
        const ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        ImGuiStyle& style = g.Style;

        const char* format = ImGui::DataTypeGetInfo(ImGuiDataType_S32)->PrintFmt;

        char buf[64];
        ImGui::DataTypeFormatString(buf, IM_ARRAYSIZE(buf), ImGuiDataType_S32, a_v, format);

#pragma warning(suppress: 5054)
        const ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoMarkEdited; // We call MarkItemEdited() ourselves by comparing the actual data rather than the string.

        bool value_changed = false;

        const float button_size = ImGui::GetFrameHeight();

        ImGui::BeginGroup(); // The only purpose of the group here is to allow the caller to query item data e.g. IsItemActive()
        ImGui::PushID(a_label);
        ImGui::SetNextItemWidth(ImMax(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * 2));
        if (ImGui::InputText("", buf, IM_ARRAYSIZE(buf), flags)) {
            // PushId(label) + "" gives us the expected ID from outside point of view
            value_changed = ImGui::DataTypeApplyFromText(buf, ImGuiDataType_S32, a_v, format);
        }
        IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags);

        // Step buttons
        const ImVec2 backup_frame_padding = style.FramePadding;
        style.FramePadding.x = style.FramePadding.y;
        const ImGuiButtonFlags button_flags = ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;
        if constexpr (false) {
            ImGui::BeginDisabled();
        }
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        if (ImGui::ButtonEx("-", ImVec2(button_size, button_size), button_flags)) {
            ImGui::DataTypeApplyOp(ImGuiDataType_S32, '-', a_v, a_v, g.IO.KeyCtrl ? &a_stepFast : &a_step);
            value_changed = true;
        }
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        if (ImGui::ButtonEx("+", ImVec2(button_size, button_size), button_flags)) {
            ImGui::DataTypeApplyOp(ImGuiDataType_S32, '+', a_v, a_v, g.IO.KeyCtrl ? &a_stepFast : &a_step);
            value_changed = true;
        }
        if constexpr (false) {
            ImGui::EndDisabled();
        }

        const char* label_end = ImGui::FindRenderedTextEnd(a_label);
        if (a_label != label_end) {
            ImGui::SameLine(0, style.ItemInnerSpacing.x);
            ImGui::TextEx(a_label, label_end);
        }
        style.FramePadding = backup_frame_padding;

        ImGui::PopID();
        ImGui::EndGroup();

        if (value_changed) {
            ImGui::MarkItemEdited(g.LastItemData.ID);
        }

        return value_changed;
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
                if (g.ActiveId == source_id) // Allow the underlying widget to display/return hovered during the mouse release frame, else we would get a flicker.
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
                SetNextWindowSize(a_tooltipSize); // <--- ADDED
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

    bool UIMain::TreeNodeCollapsedLeaf(const char* a_label, ImGuiTreeNodeFlags a_flags) const
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        return TreeNodeCollapsedLeafBehavior(window->GetID(a_label), a_flags, a_label);
    }

    bool UIMain::TreeNodeCollapsedLeaf(const void* a_ptrId, ImGuiTreeNodeFlags a_flags, const char* a_label) const
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        return TreeNodeCollapsedLeafBehavior(window->GetID(a_ptrId), a_flags, a_label);
    }

    bool UIMain::TreeNodeCollapsedLeafBehavior(ImGuiID a_id, ImGuiTreeNodeFlags a_flags, const char* a_label) const
    {
        using namespace ImGui;

        const ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const bool display_frame = (a_flags & ImGuiTreeNodeFlags_Framed) != 0;
        const ImVec2 padding = (display_frame || (a_flags & ImGuiTreeNodeFlags_FramePadding)) ? style.FramePadding : ImVec2(style.FramePadding.x, ImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));

        const char* label_end = FindRenderedTextEnd(a_label);
        const ImVec2 label_size = CalcTextSize(a_label, label_end, false);

        // We vertically grow up to current line height up the typical widget height.
        const float frame_height = ImMax(ImMin(window->DC.CurrLineSize.y, g.FontSize + style.FramePadding.y * 2), label_size.y + padding.y * 2);
        ImRect frame_bb;
        frame_bb.Min.x = (a_flags & ImGuiTreeNodeFlags_SpanFullWidth) ? window->WorkRect.Min.x : window->DC.CursorPos.x;
        frame_bb.Min.y = window->DC.CursorPos.y;
        frame_bb.Max.x = window->WorkRect.Max.x;
        frame_bb.Max.y = window->DC.CursorPos.y + frame_height;
        if (display_frame) {
            // Framed header expand a little outside the default padding, to the edge of InnerClipRect
            // (FIXME: May remove this at some point and make InnerClipRect align with WindowPadding.x instead of WindowPadding.x*0.5f)
            frame_bb.Min.x -= IM_FLOOR(window->WindowPadding.x * 0.5f - 1.0f);
            frame_bb.Max.x += IM_FLOOR(window->WindowPadding.x * 0.5f);
        }

        const float text_offset_x = g.FontSize + (display_frame ? padding.x * 3 : padding.x * 2);          // Collapser arrow width + Spacing
        const float text_offset_y = ImMax(padding.y, window->DC.CurrLineTextBaseOffset);                   // Latch before ItemSize changes it
        const float text_width = g.FontSize + (label_size.x > 0.0f ? label_size.x + padding.x * 2 : 0.0f); // Include collapser
        ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y);
        ItemSize(ImVec2(text_width, frame_height), padding.y);

        // For regular tree nodes, we arbitrary allow to click past 2 worth of ItemSpacing
        ImRect interact_bb = frame_bb;
        if (!display_frame && (a_flags & (ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth)) == 0)
            interact_bb.Max.x = frame_bb.Min.x + text_width + style.ItemSpacing.x * 2.0f;

        const bool item_add = ItemAdd(interact_bb, a_id);
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDisplayRect;
        g.LastItemData.DisplayRect = frame_bb;

        if (!item_add) {
            IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags);
            return false;
        }

        ImGuiButtonFlags button_flags = ImGuiTreeNodeFlags_None;
        if (a_flags & ImGuiTreeNodeFlags_AllowItemOverlap)
            button_flags |= ImGuiButtonFlags_AllowItemOverlap;

        if (window != g.HoveredWindow)
            button_flags |= ImGuiButtonFlags_NoKeyModifiers;

        if (a_flags & ImGuiTreeNodeFlags_OpenOnDoubleClick)
            button_flags |= ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnDoubleClick;
        else
            button_flags |= ImGuiButtonFlags_PressedOnClickRelease;

        const bool selected = (a_flags & ImGuiTreeNodeFlags_Selected) != 0;
        const bool was_selected = selected;

        bool hovered, held;
        ButtonBehavior(interact_bb, a_id, &hovered, &held, button_flags);

        if (a_flags & ImGuiTreeNodeFlags_AllowItemOverlap)
            SetItemAllowOverlap();

        // In this branch, TreeNodeBehavior() cannot toggle the selection so this will never trigger.
        if (selected != was_selected) //-V547
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

        // Render
        const ImU32 text_col = GetColorU32(ImGuiCol_Text);
        const ImGuiNavHighlightFlags nav_highlight_flags = ImGuiNavHighlightFlags_TypeThin;
        if (display_frame) {
            // Framed type
            const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
            RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, true, style.FrameRounding);
            RenderNavHighlight(frame_bb, a_id, nav_highlight_flags);
            if (a_flags & ImGuiTreeNodeFlags_Bullet)
                RenderBullet(window->DrawList, ImVec2(text_pos.x - text_offset_x * 0.60f, text_pos.y + g.FontSize * 0.5f), text_col);
            else // Leaf without bullet, left-adjusted text
                text_pos.x -= text_offset_x;
            if (a_flags & ImGuiTreeNodeFlags_ClipLabelForTrailingButton)
                frame_bb.Max.x -= g.FontSize + style.FramePadding.x;

            if (g.LogEnabled)
                LogSetNextTextDecoration("###", "###");
            RenderTextClipped(text_pos, frame_bb.Max, a_label, label_end, &label_size);
        } else {
            // Unframed typed for tree nodes
            if (hovered || selected) {
                const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
                RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, false);
            }
            RenderNavHighlight(frame_bb, a_id, nav_highlight_flags);
            if (a_flags & ImGuiTreeNodeFlags_Bullet)
                RenderBullet(window->DrawList, ImVec2(text_pos.x - text_offset_x * 0.5f, text_pos.y + g.FontSize * 0.5f), text_col);
            if (g.LogEnabled)
                LogSetNextTextDecoration(">", nullptr);
            RenderText(text_pos, a_label, label_end, false);
        }

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return false;
    }
}
