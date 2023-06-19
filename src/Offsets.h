#pragma once

static float& g_deltaTime = *(float*)REL::VariantID(523660, 410199, 0x30C3A08).address();                                       // 2F6B948, 30064C8, 30C3A08
static float& g_deltaTimeRealTime = *(float*)REL::VariantID(523661, 410200, 0x30C3A0C).address();                               // 2F6B94C, 30064CC, 30C3A0C
static uint32_t& g_durationOfApplicationRunTimeMS = *(uint32_t*)REL::VariantID(523662, 410201, 0x30C3A10).address();            // 2F6B950, 30064D0, 30C3A10
static float& g_globalTimeMultiplier = *(float*)REL::VariantID(511882, 388442, 0x1EC5698).address();                            // 1E05B28, 1E99FD0, 1EC5698
static RE::ObjectRefHandle& g_nullObjectRefHandle = *(RE::ObjectRefHandle*)REL::VariantID(514164, 400312, 0x1F8319C).address(); // 1EBEABC, 1F592BC, 1F8319C
static int32_t* g_RelationshipRankTypeIdsByIndex = (int32_t*)REL::VariantID(502260, 369311, 0x1E911A0).address();            // 1DD3EF8, 1E67FE8, 1E911A0


using thkbBlendPoses = RE::hkVector4& (*)(uint32_t numData, const RE::hkQsTransform* src, const RE::hkQsTransform* dst, float amount, RE::hkQsTransform* out);
static REL::Relocation<thkbBlendPoses> hkbBlendPoses{ REL::VariantID(63192, 64112, 0xB4DD80) }; // B12FA0, B38110, B4DD80

using tActor_GetEquippedShout = RE::TESShout* (*)(RE::Actor* a_this);
static REL::Relocation<tActor_GetEquippedShout> Actor_GetEquippedShout{ REL::VariantID(37822, 38771, 0x63B4E0) }; // 632610, 6584A0, 63B4E0

using tTESObjectREFR_GetInventoryChanges = RE::InventoryChanges* (*)(RE::TESObjectREFR* a_this);
static REL::Relocation<tTESObjectREFR_GetInventoryChanges> TESObjectREFR_GetInventoryChanges{ REL::VariantID(15801, 16039, 0x1E99C0) }; // 1D8E20, 1E4860, 1E99C0

using tTESObjectREFR_GetLocationRefType = RE::BGSLocationRefType* (*)(RE::TESObjectREFR* a_this);
static REL::Relocation<tTESObjectREFR_GetLocationRefType> TESObjectREFR_GetLocationRefType{ REL::VariantID(19843, 20248, 0x2B9790) }; // 2A8020, 2B9E50, 2B9790

using tInventoryChanges_WornHasKeyword = bool(*)(RE::InventoryChanges* a_this, RE::BGSKeyword* a_keyword);
static REL::Relocation<tInventoryChanges_WornHasKeyword> InventoryChanges_WornHasKeyword{ REL::VariantID(15808, 16046, 0x1E9D10) }; // 1D9170, 1E4C70, 1E9D10

using tActor_HasMagicEffectWithKeyword = bool(*)(RE::Actor* a_this, RE::BGSKeyword* a_keyword);
static REL::Relocation<tActor_HasMagicEffectWithKeyword> Actor_HasMagicEffectWithKeyword{ REL::VariantID(19220, 19646, 0x29DB70) }; // 28C440, 29E660, 29DB70

using tMagicTarget_HasMagicEffectWithKeyword = bool(*)(RE::MagicTarget* a_this, RE::BGSKeyword* a_keyword, void* a3);
static REL::Relocation<tMagicTarget_HasMagicEffectWithKeyword> MagicTarget_HasMagicEffectWithKeyword{ REL::VariantID(33734, 34518, 0x557400) }; // 553160, 56E7A0, 557400

using tGetCurrentGameTime = float(*)();
static REL::Relocation<tGetCurrentGameTime> GetCurrentGameTime{ REL::VariantID(56475, 56832, 0x9F3290) }; // 9B8740, 9DC890, 9F3290

using tActor_GetFactionRank = int32_t(*)(RE::Actor* a_this, const RE::TESFaction* a_faction, bool bIsPlayer);
static REL::Relocation<tActor_GetFactionRank> Actor_GetFactionRank{ REL::VariantID(36668, 37676, 0x600190) }; // 5F7A20, 61E7B0, 600190

using tActor_IsMoving = bool(*)(RE::Actor* a_this);
static REL::Relocation<tActor_IsMoving> Actor_IsMoving{ REL::VariantID(36928, 37953, 0x6116C0) }; // 608B30, 630330, 6116C0

using tActor_GetMovementDirectionRelativeToFacing = float(*)(RE::Actor* a_this);
static REL::Relocation<tActor_GetMovementDirectionRelativeToFacing> Actor_GetMovementDirectionRelativeToFacing{ REL::VariantID(36935, 37960, 0x611900) }; // 608D40, 630540, 611900

using tCharacter_ctor = RE::Character* (*)(RE::Character* a_this);
static REL::Relocation<tCharacter_ctor> Character_ctor{ REL::VariantID(39171, 40245, 0x69BEC0) }; // 6928C0, 6BA510, 69BEC0

using tTESForm_MakeTemporary = void(*)(RE::TESForm* a_this);
static REL::Relocation<tTESForm_MakeTemporary> TESForm_MakeTemporary{ REL::VariantID(14485, 14642, 0x1A4A50) }; // 194D20, 19FA90, 1A4A50

using thkbCharacter_GetAnimationBindingSet = RE::hkbAnimationBindingSet* (*)(RE::hkbCharacter* a_this);
static REL::Relocation<thkbCharacter_GetAnimationBindingSet> hkbCharacter_GetAnimationBindingSet{ REL::VariantID(57886, 58459, 0xA368F0) }; // 9FBDA0, A205B0, A368F0

using thkaDefaultAnimationControl_SetAnimationBinding = void(*)(RE::hkaDefaultAnimationControl* a_this, RE::hkaAnimationBinding* a_binding);
static REL::Relocation<thkaDefaultAnimationControl_SetAnimationBinding> hkaDefaultAnimationControl_SetAnimationBinding{ REL::VariantID(63423, 64390, 0xB595A0) }; // B1E7C0,  B43930, B595A0

using thkaDefaultAnimationControl_ctor = void(*)(RE::hkaDefaultAnimationControl* a_this, RE::hkaDefaultAnimationControl* a_other);
static REL::Relocation<thkaDefaultAnimationControl_ctor> hkaDefaultAnimationControl_ctor{ REL::VariantID(63231, 64171, 0xB51D90) }; // B16FB0,  B3C120, B51D90

using thkbCharacterSetup_GetMirroredSkeleton = RE::hkaMirroredSkeleton* (*)(RE::hkbCharacterSetup* a_this);
static REL::Relocation<thkbCharacterSetup_GetMirroredSkeleton> hkbCharacterSetup_GetMirroredSkeleton{ REL::VariantID(58817, 59480, 0xA5E8C0) }; // A23F10,  A48720, A5E8C0

using thkbClipGenerator_UnkUpdateFunc1 = void(*)(RE::hkbClipGenerator* a_this);
static REL::Relocation<thkbClipGenerator_UnkUpdateFunc1> hkbClipGenerator_UnkUpdateFunc1{ REL::VariantID(58628, 59279, 0xA49FD0) }; // A0F480, A33C90, A49FD0

using thkbClipGenerator_UnkUpdateFunc2 = void(*)(RE::hkbClipGenerator* a_this);
static REL::Relocation<thkbClipGenerator_UnkUpdateFunc2> hkbClipGenerator_UnkUpdateFunc2{ REL::VariantID(58630, 59281, 0xA4A170) }; // A0F620, A33E30, A4A170

using thkbClipGenerator_UnkUpdateFunc3_CalcLocalTime = void(*)(RE::hkbClipGenerator* a_this, float a_deltaTime, float* a_outPrevLocalTime, float* a_outLocalTime, int32_t* a_outLoops, bool* a_outAtEnd);
static REL::Relocation<thkbClipGenerator_UnkUpdateFunc3_CalcLocalTime> hkbClipGenerator_UnkUpdateFunc3_CalcLocalTime{ REL::VariantID(58622, 59273, 0xA48D60) }; // A0E210, A32A20, A48D60

using thkbClipGenerator_UnkUpdateFunc4 = void(*)(RE::hkbClipGenerator* a_this, float a_prevLocalTime, float a_newLocalTime, int32_t a_loops, RE::hkbEventQueue* a_eventQueue);
static REL::Relocation<thkbClipGenerator_UnkUpdateFunc4> hkbClipGenerator_UnkUpdateFunc4{ REL::VariantID(58619, 59270, 0xA48710) }; // A0DBC0, A323D0, A48710

using tTESObjectREFR_GetSubmergeLevel = float(*)(RE::TESObjectREFR* a_this, float a_zPos, RE::TESObjectCELL* a_parentCell);
static REL::Relocation<tTESObjectREFR_GetSubmergeLevel> TESObjectREFR_GetSubmergeLevel{ REL::VariantID(36452, 37448, 0x5E9B60) }; // 5E1510, 607080, 5E9B60

using tbhkCharacterController_CalcFallDistance = float(*)(RE::bhkCharacterController* a_this);
static REL::Relocation<tbhkCharacterController_CalcFallDistance> bhkCharacterController_CalcFallDistance{ REL::VariantID(76430, 78269, 0xE142F0) }; // DBF370, DFFB90, E142F0

using tTESObjectREFR_CalcFallDamage = float(*)(RE::TESObjectREFR* a_this, float a_fallDamage, float a_mult);
static REL::Relocation<tTESObjectREFR_CalcFallDamage> TESObjectREFR_CalcFallDamage{ REL::VariantID(37294, 38273, 0x6214D0) };  // 618920, 63E9B0, 6214D0

using tActor_GetCombatState = RE::ACTOR_COMBAT_STATE (*)(RE::Actor* a_this);
static REL::Relocation<tActor_GetCombatState> Actor_GetCombatState{ REL::VariantID(37603, 38556, 0x62DD00) }; // 624E90, 64A520, 62DD00

using tTESNPC_GetRelationship = RE::BGSRelationship* (*)(RE::TESNPC* a_npc1, RE::TESNPC* a_npc2);
static REL::Relocation<tTESNPC_GetRelationship> TESNPC_GetRelationship{ REL::VariantID(23632, 24084, 0x355D30) };  // 346470, 35C700, 355D30

using tTESNPC_GetRelationshipRankIndex = int32_t (*)(RE::TESNPC* a_npc1, RE::TESNPC* a_npc2);
static REL::Relocation<tTESNPC_GetRelationshipRankIndex> TESNPC_GetRelationshipRankIndex{ REL::VariantID(23624, 24076, 0x355790) };  // 345ED0, 35C270, 355790

using tActor_IsInDialogue = bool (*)(RE::Actor* a_this);
static REL::Relocation<tActor_IsInDialogue> Actor_IsInDialogue{ REL::VariantID(36727, 37739, 0x604230) };  // 5FBA40, 6229C0, 604230

using tActor_IsTalking = bool (*)(RE::Actor* a_this);
static REL::Relocation<tActor_IsTalking> Actor_IsTalking{ REL::VariantID(36277, 37266, 0x5DA8F0) };  // 5D2330, 5F69A0, 5DA8F0

using tTESQuest_GetStageDone = bool (*)(RE::TESQuest* a_this, uint16_t a_stageIndex);
static REL::Relocation<tTESQuest_GetStageDone> TESQuest_GetStageDone{ REL::VariantID(24483, 25011, 0x3804C0) };  // 370B20, 3881F0, 3804C0

