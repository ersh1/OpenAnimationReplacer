#include "Hooks.h"

#include <xbyak/xbyak.h>

#include "Jobs.h"
#include "Offsets.h"
#include "OpenAnimationReplacer.h"
#include "UI/UIManager.h"
#include "Utils.h"

//#include <imgui_impl_win32.h>
//extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Hooks
{
    void Install()
    {
        logger::trace("Hooking...");

        HavokHooks::Hook();

        if (Settings::bEnableUI) {
            UIHooks::Hook();
        }

        logger::trace("...success");
    }

    void HavokHooks::Nullsub()
    {
        OpenAnimationReplacer::gameTimeCounter += g_deltaTime;
        OpenAnimationReplacer::GetSingleton().RunJobs();
        _Nullsub();
    }

    void HavokHooks::hkbClipGenerator_Activate(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context)
    {
        bool bAdded;
        const auto activeClip = OpenAnimationReplacer::GetSingleton().AddOrGetActiveClip(a_this, a_context, bAdded);

        activeClip->OnActivate(a_this, a_context);

        auto& animationLog = AnimationLog::GetSingleton();
        const auto event = activeClip->IsOriginal() ? AnimationLogEntry::Event::kActivate : AnimationLogEntry::Event::kActivateReplace;
        if (bAdded && animationLog.ShouldLogAnimations() && !activeClip->IsTransitioning() && animationLog.ShouldLogAnimationsForActiveClip(activeClip, event)) {
            animationLog.LogAnimation(event, activeClip, a_context.character);
        }

        _hkbClipGenerator_Activate(a_this, a_context);

        /*if (activeClip->IsInterruptible()) {
            activeClip->LoadInterruptibleReplacements(a_context.character);
        }*/
    }

    void HavokHooks::hkbClipGenerator_Update(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context, float a_timestep)
    {
        const auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_this);
        if (activeClip) {
            activeClip->PreUpdate(a_this, a_context, a_timestep);
        }

        _hkbClipGenerator_Update(a_this, a_context, a_timestep);
    }

    void HavokHooks::hkbClipGenerator_Deactivate(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context)
    {
        _hkbClipGenerator_Deactivate(a_this, a_context);

        OpenAnimationReplacer::GetSingleton().RemoveActiveClip(a_this);
    }

    void HavokHooks::hkbClipGenerator_Generate(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context, const RE::hkbGeneratorOutput** a_activeChildrenOutput, RE::hkbGeneratorOutput& a_output, float a_timeOffset)
    {
        const auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_this);
        if (activeClip && activeClip->IsBlending()) {
            activeClip->PreGenerate(a_this, a_context, a_output);

            // if the animation is not fully loaded yet, call generate with our fake clip generator containing the previous animation instead - this avoids seeing the reference pose for a frame
            if (a_this->userData != 0xC) {
                _hkbClipGenerator_Generate(activeClip->GetBlendFromClipGenerator(), a_context, a_activeChildrenOutput, a_output, a_timeOffset);
                return;
            }
        }

        _hkbClipGenerator_Generate(a_this, a_context, a_activeChildrenOutput, a_output, a_timeOffset);

        if (activeClip && activeClip->IsBlending()) {
            activeClip->OnGenerate(a_this, a_context, a_output);
        }
    }

    void HavokHooks::hkbClipGenerator_StartEcho(RE::hkbClipGenerator* a_this, float a_echoDuration)
    {
        bool bReplaced = false;

        const auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_this);
        if (activeClip) {
            bReplaced = activeClip->OnEcho(a_this, a_echoDuration);
        }

        _hkbClipGenerator_StartEcho(a_this, a_echoDuration);

        if (!bReplaced) {
            auto& animationLog = AnimationLog::GetSingleton();
            const auto event = AnimationLogEntry::Event::kEcho;
            if (activeClip && animationLog.ShouldLogAnimations() && animationLog.ShouldLogAnimationsForActiveClip(activeClip, event)) {
                animationLog.LogAnimation(event, activeClip, activeClip->GetCharacter());
            }
        }
    }

    void HavokHooks::hkbClipGenerator_computeBeginAndEndLocalTime(RE::hkbClipGenerator* a_this, const float a_timestep, float& a_outBeginLocalTime, float& a_outEndLocalTime, int32_t& a_outLoops, bool& a_outEndOfClip)
	{
		_hkbClipGenerator_computeBeginAndEndLocalTime(a_this, a_timestep, a_outBeginLocalTime, a_outEndLocalTime, a_outLoops, a_outEndOfClip);

		if (a_this->mode == RE::hkbClipGenerator::PlaybackMode::kModeLooping && a_outLoops > 0) {
			// if we're here, the animation just looped
			// recheck conditions to see if we should replace the animation
			if (const auto activeClip = OpenAnimationReplacer::GetSingleton().GetActiveClip(a_this)) {
				if (!activeClip->OnLoop(a_this)) {
					auto& animationLog = AnimationLog::GetSingleton();
					constexpr auto event = AnimationLogEntry::Event::kLoop;
					if (animationLog.ShouldLogAnimations() && animationLog.ShouldLogAnimationsForActiveClip(activeClip, event)) {
						animationLog.LogAnimation(event, activeClip, activeClip->GetCharacter());
					}
				}
			}
		}
	}

    void HavokHooks::BSSynchronizedClipGenerator_Activate(RE::BSSynchronizedClipGenerator* a_this, const RE::hkbContext& a_context)
    {
        // save the original value
        const uint16_t originalSynchronizedIndex = a_this->synchronizedAnimIndex;

        // get the offset and alter the index
        a_this->synchronizedAnimIndex += OpenAnimationReplacer::GetSingleton().GetSynchronizedClipsIDOffset(a_context.character);

        _BSSynchronizedClipGenerator_Activate(a_this, a_context);

        // restore
        a_this->synchronizedAnimIndex = originalSynchronizedIndex;
    }

    void HavokHooks::LoadClips(RE::hkbCharacterStringData* a_stringData, RE::hkbAnimationBindingSet* a_bindingSet, void* a_assetLoader, RE::hkbBehaviorGraph* a_rootBehavior, const char* a_animationPath, RE::BSTHashMap<RE::BSFixedString, uint32_t>* a_annotationToEventIdMap)
    {
		// parse the animation directory for all the replacement animations, create all our objects like replacement animations and conditions etc
		// do this before LoadClips actually runs because it reads the animation name array to create all the animation bindings and we need to add our new animations to it before that happens

        const auto projectDBData = SKSE::stl::adjust_pointer<RE::BShkbHkxDB::ProjectDBData>(a_annotationToEventIdMap, -0xA0);
        OpenAnimationReplacer::GetSingleton().CreateReplacementAnimations(a_animationPath, a_stringData, projectDBData);

        _LoadClips(a_stringData, a_bindingSet, a_assetLoader, a_rootBehavior, a_animationPath, a_annotationToEventIdMap);
    }

    bool HavokHooks::CreateSynchronizedClips(RE::hkbBehaviorGraph* a_behaviorGraph, RE::hkbCharacter* a_character, RE::BSTHashMap<RE::BSFixedString, uint32_t>* a_annotationToEventIdMap)
    {
		// check if we maybe went over the limit after everything (synchronized clips add more entries to the binding array) and error out the game because it's in a broken state
        const bool ret = _CreateSynchronizedClips(a_behaviorGraph, a_character, a_annotationToEventIdMap);

        if (const RE::hkbAnimationBindingSet* bindingSet = hkbCharacter_GetAnimationBindingSet(a_character)) {
            if (bindingSet->bindings.size() > Settings::uAnimationLimit) {
                Utils::ErrorTooManyAnimations();
            }
        }

        return ret;
    }

    bool HavokHooks::Unk3(RE::BShkbAnimationGraph* a_graph, const char* a_fileName, bool a3)
    {
		// queue all the replacement animations to be preloaded
		// I think this technically means they will never unload
		// but this is how we definitely avoid the reference pose showing up
		// in my experience this is not necessary with all the other hooks properly loading the correct animations, but it might rely on your system being fast enough to load them in time
		// this is safer for everyone so it's enabled by default, but can be disabled in the settings
        const bool ret = _Unk3(a_graph, a_fileName, a3);

        if (!Settings::bDisablePreloading) {
            if (a_graph) {
                if (const auto& setup = a_graph->characterInstance.setup) {
                    if (const auto& characterData = setup->data) {
                        if (const auto& stringData = characterData->stringData) {
                            if (const auto projectData = OpenAnimationReplacer::GetSingleton().GetReplacerProjectData(stringData.get())) {
                                projectData->QueueReplacementAnimations(&a_graph->characterInstance);
                            }
                        }
                    }
                }
            }
        }

        return ret;
    }

    RE::bhkThreadMemorySource* HavokHooks::bhkThreadMemorySource_ctor(RE::bhkThreadMemorySource* a_this, [[maybe_unused]] uint32_t a_size)
    {
        return _bhkThreadMemorySource_ctor(a_this, Settings::uHavokHeapSize);
    }

    void HavokHooks::PatchSynchronizedClips()
    {
		// we need to do this patch because when a synchronized anim is initially set up by the game, the binding index would be only set once and would get reused for both DefaultMale and DefaultFemale
		// the binding indexes for synchronized clips start right after all the normal animation bindings
		// but with animation replacements adding more animations, some that are only relevant for one project (male-only or female-only anims), the animation count is very likely to be different for DefaultMale and DefaultFemale
		// this means the game could try to play the wrong animation and break synchronized anims for the project that wasn't loaded first
		// so instead we save an index offset to our replacer project data, which is unique per project, save the index with the offset subtracted (so they start at 0), and add the correct offset back when the synchronized anim is activated

        REL::Relocation<uintptr_t> BSSynchronizedClipGeneratorVtbl{ RE::VTABLE_BSSynchronizedClipGenerator[0] };
        _BSSynchronizedClipGenerator_Activate = BSSynchronizedClipGeneratorVtbl.write_vfunc(0x4, BSSynchronizedClipGenerator_Activate);

		// xbyak patch to call our function which will replace the value of the synchronized anim index with one that isn't relative to the amount of animations in the project
        static REL::Relocation<uintptr_t> func{ REL::VariantID(63017, 63942, 0xB40550) }; // B05710, B2A980, B40550  hkbBehaviorGraph::unk

        uint8_t patchNopD[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }; // nop

        auto& trampoline = SKSE::GetTrampoline();

        struct Patch : Xbyak::CodeGenerator
        {
            explicit Patch(uintptr_t a_funcAddr)
            {
                Xbyak::Label originalLabel;

                // original code
                mov(eax, ptr[rsi + 0x18]);
                dec(ax);
                mov(ptr[rdi + 0x128], ax);

                // scratch space
                sub(rsp, 0x20);

                // call our function
                mov(rax, a_funcAddr);
                mov(rcx, rdi); // synchronizedClipGenerator
                if (REL::Module::IsAE()) {
                    mov(rdx, r15); // hkbCharacter
                } else {
                    mov(rdx, r12); // hkbCharacter
                }
                call(rax);

                add(rsp, 0x20);

                jmp(ptr[rip + originalLabel]);

                if (REL::Module::IsAE()) {
                    L(originalLabel);
                    dq(func.address() + 0x1E0);
                } else {
                    L(originalLabel);
                    dq(func.address() + 0x1FB);
                }
            }
        };

        Patch patch(reinterpret_cast<uintptr_t>(SetSynchronizedClipID));
        patch.ready();
        SKSE::AllocTrampoline(8 + patch.getSize());
        REL::safe_write<uint8_t>(func.address() + REL::VariantOffset(0x1EE, 0x1D3, 0x1EE).offset(), patchNopD);
        trampoline.write_branch<6>(func.address() + REL::VariantOffset(0x1EE, 0x1D3, 0x1EE).offset(), trampoline.allocate(patch));
    }

    void HavokHooks::SetSynchronizedClipID(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, RE::hkbCharacter* a_character)
    {
        a_synchronizedClipGenerator->synchronizedAnimIndex -= OpenAnimationReplacer::GetSingleton().GetSynchronizedClipsIDOffset(a_character);
    }

    void HavokHooks::PatchUnsignedAnimationBindingIndex()
    {
		// patch all the places I could find where the game treats the animation binding index as int16 to treat it as uint16 instead
		// the game uses the animation binding index as an array index, so it can't be negative
		// the game uses -1 as a special value to indicate that the animation binding index is invalid
		// the rest of the negative values are unused, so we can use them to store more animation bindings, just reserve the -1 value for invalid animation bindings

        if (Settings::bIncreaseAnimationLimit) {
            const REL::Relocation<uintptr_t> func1{ REL::VariantID(58602, 59252, 0xA466D0) }; // A0BB80, A30390, A466D0  hkbClipGenerator::activate
            REL::Relocation<uintptr_t> func2{ REL::VariantID(62927, 63850, 0xB3BAC0) };       // B00CE0, B252F0, B3BAC0  called at some point after loading anims
            const REL::Relocation<uintptr_t> func3{ REL::VariantID(62618, 63559, 0xB25EC0) }; // AEB360, B10300, B25EC0  BShkbAnimationGraph::unk
            const REL::Relocation<uintptr_t> func4{ REL::VariantID(62655, 63600, 0xB2BAA0) }; // AF0D10, B16840, B2BAA0  BShkbAnimationGraph::GetHashedAnimFromAnimIndex

            const REL::Relocation<uintptr_t> funcA{ REL::VariantID(63017, 63942, 0xB40550) }; // B05710, B2A980, B40550  hkbBehaviorGraph::unk (Synchronized clips?)
            const REL::Relocation<uintptr_t> funcB{ REL::VariantID(62391, 63332, 0xB18B40) }; // ADE070, B02880, B18B40  hkbCharacter::unk (Synchronized clips?)
            const REL::Relocation<uintptr_t> funcC{ REL::VariantID(62122, 63058, 0xB0D7B0) }; // AD2D10, AF7520, B0D7B0  BSSynchronizedClipGenerator::unk

            // ADE1D0?

            uint8_t patchMovzx[] = { 0x0F, 0xB7 }; // movzx
            //uint8_t patchNop6[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
            uint8_t patchNop9[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };                   // nop
            uint8_t patchNopA[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };             // nop
            uint8_t patchNopC[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }; // nop

            auto& trampoline = SKSE::GetTrampoline();

            // Func1
            REL::safe_write<uint8_t>(func1.address() + 0x160, patchMovzx); // movsx -> movzx
            REL::safe_write<uint8_t>(func1.address() + 0x176, patchMovzx); // movsx -> movzx

            // Func2
            //REL::safe_write<uint8_t>(func2.address() + 0x94, patchNop6);  // js -> nop

            // Func3
            struct PatchFunc3A : Xbyak::CodeGenerator
            {
                explicit PatchFunc3A(uintptr_t a_originalAddr, uintptr_t a_jumpAddr)
                {
                    Xbyak::Label originalLabel;
                    Xbyak::Label jumpLabel;

                    cmp(edx, static_cast<uint16_t>(-1));
                    je(jumpLabel);

                    jmp(originalLabel); // AEB392

                    L(originalLabel);
                    jmp(ptr[rip]);
                    dq(a_originalAddr);

                    L(jumpLabel);
                    jmp(ptr[rip]);
                    dq(a_jumpAddr);
                }
            };
            PatchFunc3A patchFunc3A(func3.address() + REL::VariantOffset(0x32, 0x35, 0x32).offset(), func3.address() + REL::VariantOffset(0x300, 0x353, 0x300).offset());
            patchFunc3A.ready();
            SKSE::AllocTrampoline(8 + patchFunc3A.getSize());
            REL::safe_write<uint8_t>(func3.address() + REL::VariantOffset(0x29, 0x2C, 0x29).offset(), patchNop9);
            trampoline.write_branch<6>(func3.address() + REL::VariantOffset(0x29, 0x2C, 0x29).offset(), trampoline.allocate(patchFunc3A));

            REL::safe_write<uint8_t>(func3.address() + REL::VariantOffset(0x4D, 0x50, 0x4D).offset(), patchMovzx); // movsx -> movzx

            struct PatchFunc3B : Xbyak::CodeGenerator
            {
                explicit PatchFunc3B(uintptr_t a_originalAddr, uintptr_t a_jumpAddr)
                {
                    Xbyak::Label originalLabel;
                    Xbyak::Label jumpLabel;

                    cmp(eax, static_cast<uint16_t>(-1));
                    je(jumpLabel);

                    jmp(originalLabel); // AEB576

                    L(originalLabel);
                    jmp(ptr[rip]);
                    dq(a_originalAddr);

                    L(jumpLabel);
                    jmp(ptr[rip]);
                    dq(a_jumpAddr);
                }
            };
            PatchFunc3B patchFunc3B(func3.address() + REL::VariantOffset(0x216, 0x22B, 0x216).offset(), func3.address() + REL::VariantOffset(0x2D2, 0x2DC, 0x2D2).offset());
            patchFunc3B.ready();
            SKSE::AllocTrampoline(8 + patchFunc3B.getSize());
            REL::safe_write<uint8_t>(func3.address() + REL::VariantOffset(0x20D, 0x222, 0x20D).offset(), patchNop9);
            trampoline.write_branch<6>(func3.address() + REL::VariantOffset(0x20D, 0x222, 0x20D).offset(), trampoline.allocate(patchFunc3B));

            REL::safe_write<uint8_t>(func3.address() + REL::VariantOffset(0x21E, 0x232, 0x21E).offset(), patchMovzx); // movsx -> movzx

            // Func4
            struct PatchFunc4 : Xbyak::CodeGenerator
            {
                explicit PatchFunc4(uintptr_t a_originalAddr, uintptr_t a_jumpAddr)
                {
                    Xbyak::Label originalLabel;
                    Xbyak::Label jumpLabel;

                    cmp(edx, static_cast<uint16_t>(-1));
                    je(jumpLabel);
                    mov(rcx, ptr[rcx + 0x200]);
                    jmp(originalLabel); // AF0D1C

                    L(originalLabel);
                    jmp(ptr[rip]);
                    dq(a_originalAddr);

                    L(jumpLabel);
                    jmp(ptr[rip]);
                    dq(a_jumpAddr);
                }
            };
            PatchFunc4 patchFunc4(func4.address() + 0xC, func4.address() + 0x1D);
            patchFunc4.ready();
            SKSE::AllocTrampoline(8 + patchFunc4.getSize());
            REL::safe_write<uint8_t>(func4.address(), patchNopC);
            trampoline.write_branch<6>(func4.address(), trampoline.allocate(patchFunc4));

            REL::safe_write<uint8_t>(func4.address() + 0xC, patchMovzx); // movsx -> movzx

            // Synchronized clips

            // FuncA
            // AE has some differences in this case
            struct PatchFuncA : Xbyak::CodeGenerator
            {
                explicit PatchFuncA(uintptr_t a_originalAddr, uintptr_t a_jumpAddr)
                {
                    Xbyak::Label originalLabel;
                    Xbyak::Label jumpLabel;

                    if (REL::Module::IsAE()) {
                        cmp(eax, static_cast<uint16_t>(-1));
                        je(jumpLabel);
                        cmp(eax, ptr[rsi + 0x18]);
                        jae(jumpLabel);
                    } else {
                        cmp(ecx, static_cast<uint16_t>(-1));
                        je(jumpLabel);
                        movzx(eax, cx);
                        cmp(eax, ptr[rsi + 0x18]);
                        jae(jumpLabel);
                    }

                    jmp(originalLabel); // B05893

                    L(originalLabel);
                    jmp(ptr[rip]);
                    dq(a_originalAddr);

                    L(jumpLabel);
                    jmp(ptr[rip]);
                    dq(a_jumpAddr);
                }
            };

            PatchFuncA patchFuncA(funcA.address() + REL::VariantOffset(0x183, 0x165, 0x183).offset(), funcA.address() + REL::VariantOffset(0x1FB, 0x1E3, 0x1FB).offset());
            patchFuncA.ready();
            SKSE::AllocTrampoline(8 + patchFuncA.getSize());
            REL::safe_write<uint8_t>(funcA.address() + REL::VariantOffset(0x172, 0x152, 0x172).offset(), patchNop9);
            trampoline.write_branch<6>(funcA.address() + REL::VariantOffset(0x172, 0x152, 0x172).offset(), trampoline.allocate(patchFuncA));

            // FuncB
            struct PatchFuncB : Xbyak::CodeGenerator
            {
                explicit PatchFuncB(uintptr_t a_originalAddr, uintptr_t a_jumpAddr)
                {
                    Xbyak::Label originalLabel;
                    Xbyak::Label jumpLabel;

                    cmp(r8, static_cast<uint16_t>(-1));
                    je(jumpLabel);
                    mov(r9d, ptr[rbx + 0x18]);
                    movzx(eax, r8w);
                    cmp(eax, r9d);
                    jae(jumpLabel);
                    cmp(ecx, static_cast<uint16_t>(-1));
                    je(jumpLabel);
                    movzx(eax, cx);
                    cmp(eax, r9d);
                    jae(jumpLabel);

                    jmp(originalLabel); // ADE0F3

                    L(originalLabel);
                    jmp(ptr[rip]);
                    dq(a_originalAddr);

                    L(jumpLabel);
                    jmp(ptr[rip]);
                    dq(a_jumpAddr);
                }
            };

            PatchFuncB patchFuncB(funcB.address() + 0x83, funcB.address() + 0x143);
            patchFuncB.ready();
            SKSE::AllocTrampoline(8 + patchFuncB.getSize());
            REL::safe_write<uint8_t>(funcB.address() + 0x53, patchNopA);
            trampoline.write_branch<6>(funcB.address() + 0x53, trampoline.allocate(patchFuncB));

            //REL::safe_write<uint8_t>(funcB.address() + 0x61, patchMovzx);  // movsx -> movzx
            //REL::safe_write<uint8_t>(funcB.address() + 0x77, patchMovzx);  // movsx -> movzx
            REL::safe_write<uint8_t>(funcB.address() + 0x84, patchMovzx); // movsx -> movzx
            REL::safe_write<uint8_t>(funcB.address() + 0xAB, patchMovzx); // movsx -> movzx

            // FuncC
            REL::safe_write<uint8_t>(funcC.address() + 0x125, patchMovzx); // movsx -> movzx
            REL::safe_write<uint8_t>(funcC.address() + 0x135, patchMovzx); // movsx -> movzx
        }
    }

    void UIHooks::InputFunc(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events)
    {
        if (a_events) {
            UI::UIManager::GetSingleton().ProcessInputEvents(a_events);
        }

        if (UI::UIManager::GetSingleton().ShouldConsumeInput()) {
            constexpr RE::InputEvent* const dummy[] = { nullptr };
            _InputFunc(a_dispatcher, dummy);
        } else {
            _InputFunc(a_dispatcher, a_events);
        }
    }

    ATOM UIHooks::RegisterClassA_Hook(WNDCLASSA* a_wndClass)
    {
        _WndProcHandler = reinterpret_cast<uintptr_t>(a_wndClass->lpfnWndProc);
        a_wndClass->lpfnWndProc = &WndProcHandler;

        return _RegisterClassA_Hook(a_wndClass);
    }

    LRESULT UIHooks::WndProcHandler(HWND a_hwnd, UINT a_msg, WPARAM a_wParam, LPARAM a_lParam)
    {
        /*if (ImGui_ImplWin32_WndProcHandler(a_hwnd, a_msg, a_wParam, a_lParam)) {
            return true;
        }*/

        if (a_msg == WM_KILLFOCUS) {
            UI::UIManager::GetSingleton().OnFocusLost();
        }

        return _WndProcHandler(a_hwnd, a_msg, a_wParam, a_lParam);
    }

    void UIHooks::CreateD3D11()
    {
        _CreateD3D11();

        UI::UIManager::GetSingleton().Init();
    }

    void UIHooks::Present(uint32_t a1)
    {
        _Present(a1);

        UI::UIManager::GetSingleton().Render();
    }
}
