#pragma once
#include "OpenAnimationReplacer.h"

#include <windows.h>
#include <xbyak.h>

namespace Hooks
{
	class HavokHooks
	{
	public:
		static void Hook()
		{
			auto& trampoline = SKSE::GetTrampoline();

			// Hook main update to run jobs and update our game timer
			const REL::Relocation<uintptr_t> mainHook{ REL::VariantID(35565, 36564, 0x5BAB10) };  // 5B2FF0, 5D9F50, 5BAB10 main update
			SKSE::AllocTrampoline(14);
			_Nullsub = trampoline.write_call<5>(mainHook.address() + REL::VariantOffset(0x748, 0xC26, 0x7EE).offset(), Nullsub);  // 5B3738, 5DAB76, 5BB256

			// Hook hkbClipGenerator vfuncs to add/remove/update ActiveClips
			REL::Relocation<uintptr_t> hkbClipGeneratorVtbl{ RE::VTABLE_hkbClipGenerator[0] };
			_hkbClipGenerator_Activate = hkbClipGeneratorVtbl.write_vfunc(0x4, hkbClipGenerator_Activate);
			_hkbClipGenerator_Update = hkbClipGeneratorVtbl.write_vfunc(0x5, hkbClipGenerator_Update);
			_hkbClipGenerator_Deactivate = hkbClipGeneratorVtbl.write_vfunc(0x7, hkbClipGenerator_Deactivate);
			_hkbClipGenerator_Generate = hkbClipGeneratorVtbl.write_vfunc(0x17, hkbClipGenerator_Generate);
			_hkbClipGenerator_StartEcho = hkbClipGeneratorVtbl.write_vfunc(0x1B, hkbClipGenerator_StartEcho);

			// Hook synchronized clip related stuff
			REL::Relocation<uintptr_t> BSSynchronizedClipGeneratorVtbl{ RE::VTABLE_BSSynchronizedClipGenerator[0] };
			_BSSynchronizedClipGenerator_Activate = BSSynchronizedClipGeneratorVtbl.write_vfunc(0x4, BSSynchronizedClipGenerator_Activate);
			_BSSynchronizedClipGenerator_Update = BSSynchronizedClipGeneratorVtbl.write_vfunc(0x5, BSSynchronizedClipGenerator_Update);
			_BSSynchronizedClipGenerator_Deactivate = BSSynchronizedClipGeneratorVtbl.write_vfunc(0x7, BSSynchronizedClipGenerator_Deactivate);

			REL::Relocation<uintptr_t> BGSSynchronizedAnimationInstanceVtbl{ RE::VTABLE_BGSSynchronizedAnimationInstance[0] };
			_BGSSynchronizedAnimationInstance_dtor = BGSSynchronizedAnimationInstanceVtbl.write_vfunc(0x0, BGSSynchronizedAnimationInstance_dtor);
			_BGSSynchronizedAnimationInstance_OnDeactivate = BGSSynchronizedAnimationInstanceVtbl.write_vfunc(0x7, BGSSynchronizedAnimationInstance_OnDeactivate);
			_BGSSynchronizedAnimationInstance_Func9 = BGSSynchronizedAnimationInstanceVtbl.write_vfunc(0x9, BGSSynchronizedAnimationInstance_Func9);

			const REL::Relocation<uintptr_t> createSynchronizedAnimationInstanceHook{ REL::VariantID(32049, 32803, 0x4FD300) };  // 4ED090, 505C10, 4FD300  BGSSynchronizedAnimationInstance is created
			SKSE::AllocTrampoline(14);
			_BGSSynchronizedAnimationInstance_Init = trampoline.write_call<5>(createSynchronizedAnimationInstanceHook.address() + REL::VariantOffset(0x1DE, 0x1D7, 0x1DE).offset(), BGSSynchronizedAnimationInstance_Init);

			// Parse/add replacement animations
			const REL::Relocation<uintptr_t> bshkbAnimationGraphHook{ REL::VariantID(63028, 63846, 0xB414B0) };  // B06670, B24E00, B414B0 -  called by an internal BShkbAnimationGraph function on creation
			SKSE::AllocTrampoline(14);
			_LoadClips = trampoline.write_call<5>(bshkbAnimationGraphHook.address() + REL::VariantOffset(0x8D, 0x101, 0x8D).offset(), LoadClips);

			// Check animation binding set size
			const REL::Relocation<uintptr_t> bshkbAnimationGraphHook2{ REL::VariantID(62923, 63846, 0xB3B430) };  // B00650, B24E00, B3B430 -  called by an internal BShkbAnimationGraph function on creation
			SKSE::AllocTrampoline(14);
			_CreateSynchronizedClips = trampoline.write_call<5>(bshkbAnimationGraphHook2.address() + REL::VariantOffset(0xE1, 0x12F, 0xE1).offset(), CreateSynchronizedClips);

			// Fix crash with too many loaded anims - increase havok heap size
			const REL::Relocation<uintptr_t> havokMemoryCtorHook{ REL::VariantID(77470, 79348, 0xE4E140) };  // DF9140, E3C470, E4E140  bhkMemorySystem ctor
			SKSE::AllocTrampoline(14);
			_bhkThreadMemorySource_ctor = trampoline.write_call<5>(havokMemoryCtorHook.address() + 0x29, bhkThreadMemorySource_ctor);

			// Fix anim IDs getting overwritten
			const REL::Relocation<uintptr_t> loadClipsHook{ REL::VariantID(63032, 63885, 0xB41E00) };                      // B06FC0, B28940, B41E00  hkbBehaviorLoadingUtils::loadClips presumably, or an internal one in case of AE (got uninlined)
			uint8_t patch1[] = { 0x90, 0x90, 0x90 };                                                                       // nop
			REL::safe_write<uint8_t>(loadClipsHook.address() + REL::VariantOffset(0x24A, 0x161, 0x24A).offset(), patch1);  // overwrite mov [rcx+8], edi with nop   - so the anim index is not overwritten if the anim's already present in the map

			// Preload all animations
			const REL::Relocation<uintptr_t> hook7{ REL::VariantID(32149, 32893, 0x500BD0) };  // 4F0960, 509930, 500BD0 - internal IAnimationGraphManagerHolder function called on creation
			SKSE::AllocTrampoline(14);
			_Unk3 = trampoline.write_call<5>(hook7.address() + 0x11C, Unk3);  // same offset for all versions

			// Hook behavior graph for previewing animations
			REL::Relocation<uintptr_t> hkbBehaviorGraphVtbl{ RE::VTABLE_hkbBehaviorGraph[0] };
			_hkbBehaviorGraph_Update = hkbBehaviorGraphVtbl.write_vfunc(0x5, hkbBehaviorGraph_Update);
			_hkbBehaviorGraph_Generate = hkbBehaviorGraphVtbl.write_vfunc(0x17, hkbBehaviorGraph_Generate);

			// Hook animation events
			REL::Relocation<uintptr_t> TESObjectREFR_IAnimationGraphManagerHolderVtbl{ RE::VTABLE_TESObjectREFR[3] };
			_TESObjectREFR_IAnimationGraphManagerHolder_NotifyAnimationGraph = TESObjectREFR_IAnimationGraphManagerHolderVtbl.write_vfunc(0x1, TESObjectREFR_IAnimationGraphManagerHolder_NotifyAnimationGraph);
			REL::Relocation<uintptr_t> Character_IAnimationGraphManagerHolderVtbl{ RE::VTABLE_Character[3] };
			_Character_IAnimationGraphManagerHolder_NotifyAnimationGraph = Character_IAnimationGraphManagerHolderVtbl.write_vfunc(0x1, Character_IAnimationGraphManagerHolder_NotifyAnimationGraph);
			REL::Relocation<uintptr_t> PlayerCharacter_IAnimationGraphManagerHolderVtbl{ RE::VTABLE_PlayerCharacter[3] };
			_PlayerCharacter_IAnimationGraphManagerHolder_NotifyAnimationGraph = PlayerCharacter_IAnimationGraphManagerHolderVtbl.write_vfunc(0x1, PlayerCharacter_IAnimationGraphManagerHolder_NotifyAnimationGraph);

			PatchSynchronizedClips();
			PatchUnsignedAnimationBindingIndex();
		}

	private:
		static void Nullsub();

		static void hkbClipGenerator_Activate(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context);
		static void hkbClipGenerator_Update(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context, float a_timestep);
		static void hkbClipGenerator_Deactivate(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context);
		static void hkbClipGenerator_Generate(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context, const RE::hkbGeneratorOutput** a_activeChildrenOutput, RE::hkbGeneratorOutput& a_output, float a_timeOffset);
		static void hkbClipGenerator_StartEcho(RE::hkbClipGenerator* a_this, float a_echoDuration);
		static void BSSynchronizedClipGenerator_Activate(RE::BSSynchronizedClipGenerator* a_this, const RE::hkbContext& a_context);
		static void BSSynchronizedClipGenerator_Update(RE::BSSynchronizedClipGenerator* a_this, const RE::hkbContext& a_context, float a_timestep);
		static void BSSynchronizedClipGenerator_Deactivate(RE::BSSynchronizedClipGenerator* a_this, const RE::hkbContext& a_context);
		static void BGSSynchronizedAnimationInstance_dtor(RE::BGSSynchronizedAnimationInstance* a_this);
		static void BGSSynchronizedAnimationInstance_OnDeactivate(RE::BGSSynchronizedAnimationInstance* a_this);
		static void BGSSynchronizedAnimationInstance_Func9(RE::BGSSynchronizedAnimationInstance* a_this);
		static void BGSSynchronizedAnimationInstance_Init(RE::BGSSynchronizedAnimationInstance* a_this);
		static void hkbBehaviorGraph_Update(RE::hkbBehaviorGraph* a_this, const RE::hkbContext& a_context, float a_timestep);
		static void hkbBehaviorGraph_Generate(RE::hkbBehaviorGraph* a_this, const RE::hkbContext& a_context, const RE::hkbGeneratorOutput** a_activeChildrenOutput, RE::hkbGeneratorOutput& a_output, float a_timeOffset);
		static bool TESObjectREFR_IAnimationGraphManagerHolder_NotifyAnimationGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName);
		static bool Character_IAnimationGraphManagerHolder_NotifyAnimationGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName);
		static bool PlayerCharacter_IAnimationGraphManagerHolder_NotifyAnimationGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName);

		static void LoadClips(RE::hkbCharacterStringData* a_stringData, RE::hkbAnimationBindingSet* a_bindingSet, void* a_assetLoader, RE::hkbBehaviorGraph* a_rootBehavior, const char* a_animationPath, RE::BSTHashMap<RE::BSFixedString, uint32_t>* a_annotationToEventIdMap);
		static bool CreateSynchronizedClips(RE::hkbBehaviorGraph* a_behaviorGraph, RE::hkbCharacter* a_character, RE::BSTHashMap<RE::BSFixedString, uint32_t>* a_annotationToEventIdMap);

		static RE::bhkThreadMemorySource* bhkThreadMemorySource_ctor(RE::bhkThreadMemorySource* a_this, uint32_t a_size);

		static bool Unk3(RE::BShkbAnimationGraph* a_graph, const char* a_fileName, bool a3);

		static inline REL::Relocation<decltype(Nullsub)> _Nullsub;
		static inline REL::Relocation<decltype(hkbClipGenerator_Activate)> _hkbClipGenerator_Activate;
		static inline REL::Relocation<decltype(hkbClipGenerator_Update)> _hkbClipGenerator_Update;
		static inline REL::Relocation<decltype(hkbClipGenerator_Deactivate)> _hkbClipGenerator_Deactivate;
		static inline REL::Relocation<decltype(hkbClipGenerator_Generate)> _hkbClipGenerator_Generate;
		static inline REL::Relocation<decltype(hkbClipGenerator_StartEcho)> _hkbClipGenerator_StartEcho;
		static inline REL::Relocation<decltype(BSSynchronizedClipGenerator_Activate)> _BSSynchronizedClipGenerator_Activate;
		static inline REL::Relocation<decltype(BSSynchronizedClipGenerator_Update)> _BSSynchronizedClipGenerator_Update;
		static inline REL::Relocation<decltype(BSSynchronizedClipGenerator_Deactivate)> _BSSynchronizedClipGenerator_Deactivate;
		static inline REL::Relocation<decltype(BGSSynchronizedAnimationInstance_dtor)> _BGSSynchronizedAnimationInstance_dtor;
		static inline REL::Relocation<decltype(BGSSynchronizedAnimationInstance_OnDeactivate)> _BGSSynchronizedAnimationInstance_OnDeactivate;
		static inline REL::Relocation<decltype(BGSSynchronizedAnimationInstance_Func9)> _BGSSynchronizedAnimationInstance_Func9;
		static inline REL::Relocation<decltype(BGSSynchronizedAnimationInstance_Init)> _BGSSynchronizedAnimationInstance_Init;
		static inline REL::Relocation<decltype(hkbBehaviorGraph_Update)> _hkbBehaviorGraph_Update;
		static inline REL::Relocation<decltype(hkbBehaviorGraph_Generate)> _hkbBehaviorGraph_Generate;
		static inline REL::Relocation<decltype(TESObjectREFR_IAnimationGraphManagerHolder_NotifyAnimationGraph)> _TESObjectREFR_IAnimationGraphManagerHolder_NotifyAnimationGraph;
		static inline REL::Relocation<decltype(Character_IAnimationGraphManagerHolder_NotifyAnimationGraph)> _Character_IAnimationGraphManagerHolder_NotifyAnimationGraph;
		static inline REL::Relocation<decltype(PlayerCharacter_IAnimationGraphManagerHolder_NotifyAnimationGraph)> _PlayerCharacter_IAnimationGraphManagerHolder_NotifyAnimationGraph;

		static inline REL::Relocation<decltype(LoadClips)> _LoadClips;
		static inline REL::Relocation<decltype(CreateSynchronizedClips)> _CreateSynchronizedClips;

		static inline REL::Relocation<decltype(bhkThreadMemorySource_ctor)> _bhkThreadMemorySource_ctor;

		static inline REL::Relocation<decltype(Unk3)> _Unk3;

		static void PatchSynchronizedClips();
		static void PatchUnsignedAnimationBindingIndex();
		static void OnCreateSynchronizedClipBinding(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, RE::hkbCharacter* a_character);
	};

	class UIHooks
	{
	public:
		static void Hook()
		{
			const REL::Relocation<uintptr_t> inputHook{ REL::VariantID(67315, 68617, 0xC519E0) };           // C150B0, C3B360, C519E0
			const REL::Relocation<uintptr_t> registerWindowHook{ REL::VariantID(75591, 77226, 0xDC4B90) };  // D71F00, DA3850, DC4B90
			const REL::Relocation<uintptr_t> created3d11Hook{ REL::VariantID(75595, 77226, 0xDC5530) };     // D72810, DA3850, DC5530
			const REL::Relocation<uintptr_t> presentHook{ REL::VariantID(75461, 77246, 0xDBBDD0) };         // D6A2B0, DA5BE0, DBBDD0

			auto& trampoline = SKSE::GetTrampoline();

			SKSE::AllocTrampoline(14);
			_InputFunc = trampoline.write_call<5>(inputHook.address() + REL::VariantOffset(0x7B, 0x7B, 0x81).offset(), InputFunc);

			SKSE::AllocTrampoline(14);
			_RegisterClassA_Hook = *(uintptr_t*)trampoline.write_call<6>(registerWindowHook.address() + REL::VariantOffset(0x8E, 0x15C, 0x99).offset(), RegisterClassA_Hook);

			SKSE::AllocTrampoline(14);
			_CreateD3D11 = trampoline.write_call<5>(created3d11Hook.address() + REL::VariantOffset(0x9, 0x275, 0x9).offset(), CreateD3D11);

			SKSE::AllocTrampoline(14);
			_Present = trampoline.write_call<5>(presentHook.address() + REL::VariantOffset(0x9, 0x9, 0x15).offset(), Present);
		}

	private:
		static void InputFunc(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events);
		static ATOM RegisterClassA_Hook(WNDCLASSA* a_wndClass);
		static LRESULT WndProcHandler(HWND a_hwnd, UINT a_msg, WPARAM a_wParam, LPARAM a_lParam);

		static void CreateD3D11();
		static void Present(uint32_t a1);

		static inline REL::Relocation<decltype(InputFunc)> _InputFunc;
		static inline REL::Relocation<decltype(RegisterClassA_Hook)> _RegisterClassA_Hook;
		static inline REL::Relocation<decltype(CreateD3D11)> _CreateD3D11;
		static inline REL::Relocation<decltype(Present)> _Present;

		static inline REL::Relocation<decltype(WndProcHandler)> _WndProcHandler;
	};

	void Install();
}
