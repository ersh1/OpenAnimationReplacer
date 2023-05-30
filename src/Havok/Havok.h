#pragma once

namespace RE
{
	struct hkbGeneratorOutput
	{
		enum class StandardTracks
		{
			TRACK_WORLD_FROM_MODEL,                       // 00
			TRACK_EXTRACTED_MOTION,                       // 01
			TRACK_POSE,                                   // 02
			TRACK_FLOAT_SLOTS,                            // 03
			TRACK_RIGID_BODY_RAGDOLL_CONTROLS,            // 04
			TRACK_RIGID_BODY_RAGDOLL_BLEND_TIME,          // 05
			TRACK_POWERED_RAGDOLL_CONTROLS,               // 06
			TRACK_POWERED_RAGDOLL_WORLD_FROM_MODEL_MODE,  // 07
			TRACK_KEYFRAMED_RAGDOLL_BONES,                // 08
			TRACK_KEYFRAME_TARGETS,                       // 09
			TRACK_ANIMATION_BLEND_FRACTION,               // 0A
			TRACK_ATTRIBUTES,                             // 0B
			TRACK_FOOT_IK_CONTROLS,                       // 0C
			TRACK_CHARACTER_CONTROLLER_CONTROLS,          // 0D
			TRACK_HAND_IK_CONTROLS_0,                     // 0E
			TRACK_HAND_IK_CONTROLS_1,                     // 0F
			TRACK_HAND_IK_CONTROLS_2,                     // 10
			TRACK_HAND_IK_CONTROLS_3,                     // 11
			TRACK_HAND_IK_CONTROLS_NON_BLENDABLE_0,       // 12
			TRACK_HAND_IK_CONTROLS_NON_BLENDABLE_1,       // 13
			TRACK_HAND_IK_CONTROLS_NON_BLENDABLE_2,       // 14
			TRACK_HAND_IK_CONTROLS_NON_BLENDABLE_3,       // 15
			TRACK_DOCKING_CONTROLS,                       // 16
			TRACK_AI_CONTROL_CONTROLS_BLENDABLE,          // 17
			TRACK_AI_CONTROL_CONTROLS_NON_BLENDABLE,      // 18
			NUM_STANDARD_TRACKS,                          // 19
		};

		enum class TrackTypes
		{
			TRACK_TYPE_REAL,         // 0
			TRACK_TYPE_QSTRANSFORM,  // 1
			TRACK_TYPE_BINARY,       // 2
		};

		enum class TrackFlags
		{
			TRACK_FLAG_ADDITIVE_POSE = 1,
			TRACK_FLAG_PALETTE = 1 << 1,
			TRACK_FLAG_SPARSE = 1 << 2,
		};

		struct TrackHeader
		{
			int16_t capacity;                            // 00
			int16_t numData;                             // 02
			int16_t dataOffset;                          // 04
			int16_t elementSizeBytes;                    // 06
			float onFraction;                            // 08
			stl::enumeration<TrackFlags, int8_t> flags;  // 0C
			stl::enumeration<TrackTypes, int8_t> type;   // 0D
		};
		static_assert(sizeof(TrackHeader) == 0x10);

		struct TrackMasterHeader
		{
			int32_t numBytes;   // 00
			int32_t numTracks;  // 04
			int8_t unused[8];   // 08
		};

		struct Tracks
		{
			struct TrackMasterHeader masterHeader;  // 00
			struct TrackHeader trackHeaders[(int)RE::hkbGeneratorOutput::StandardTracks::NUM_STANDARD_TRACKS];  // 10
		};

		struct Track
		{
			TrackHeader* header;  // 00
			float* data;          // 08
		};

		struct Tracks* tracks;  // 00
		bool deleteTracks;      // 08
	};

	class hkbAnimationBindingWithTriggers : public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI = RTTI_hkbAnimationBindingWithTriggers;
		inline static constexpr auto VTABLE = VTABLE_hkbAnimationBindingWithTriggers;

		struct Trigger
		{
			float time;
			int32_t eventId;
		};

		// members
		hkRefPtr<hkaAnimationBinding> binding;  // 10
		hkArray<Trigger> triggers;              // 18
	};
	static_assert(sizeof(hkbAnimationBindingWithTriggers) == 0x28);

	class BSSynchronizedClipGenerator : public hkbGenerator
	{
	public:
		class hkbDefaultSynchronizedScene : public hkReferencedObject
		{
			inline static constexpr auto RTTI = RTTI_BSSynchronizedClipGenerator__hkbDefaultSynchronizedScene;
			inline static constexpr auto VTABLE = VTABLE_BSSynchronizedClipGenerator__hkbDefaultSynchronizedScene;

			~hkbDefaultSynchronizedScene() override;  // 00

			// members
			BSReadWriteLock lock;  // 10
		};
		static_assert(sizeof(hkbDefaultSynchronizedScene) == 0x18);

		class hkbSynchronizedAnimationScene : public hkbDefaultSynchronizedScene
		{
		public:
			inline static constexpr auto RTTI = RTTI_BSSynchronizedClipGenerator__hkbSynchronizedAnimationScene;
			inline static constexpr auto VTABLE = VTABLE_BSSynchronizedClipGenerator__hkbSynchronizedAnimationScene;

			~hkbSynchronizedAnimationScene() override;  // 00

		};
		static_assert(sizeof(hkbSynchronizedAnimationScene) == 0x18);

		inline static constexpr auto RTTI = RTTI_BSSynchronizedClipGenerator;
		inline static constexpr auto VTABLE = VTABLE_BSSynchronizedClipGenerator;

		~BSSynchronizedClipGenerator() override;  // 00

		// members
		uint32_t unk_48;
		uint32_t unk_4C;
		hkbClipGenerator* clipGenerator;  // 50
		uint64_t unk_58;
		uint8_t unk_60;
		float unk_64;
		uint32_t unk_68;
		uint8_t unk_6C;
		uint8_t unk_6D;
		uint8_t unk_6E;
		class BGSSynchronizedAnimationInstance* synchronizedScene;  // 70
		uint32_t unk_78;
		uint32_t unk_7C;
		hkVector4 unk_80;
		hkVector4 unk_90;
		hkVector4 unk_A0;
		hkVector4 unk_B0;
		hkVector4 unk_C0;
		hkVector4 unk_D0;
		hkVector4 unk_E0;
		hkVector4 unk_F0;
		hkVector4 unk_100;
		float unk_110;  // 110
		hkaAnimationBinding* binding_118;  // 118
		void* unk_120;  // 120
		uint16_t synchronizedAnimIndex;  // 128
		bool unk_12A;  // 12A
		uint8_t unk_12B;  // 12B
		uint8_t unk_12C;  // 12C
	};
	static_assert(sizeof(BSSynchronizedClipGenerator) == 0x130);
	static_assert(offsetof(BSSynchronizedClipGenerator, synchronizedAnimIndex) == 0x128);

	class BGSSynchronizedAnimationInstance : public BSSynchronizedClipGenerator::hkbSynchronizedAnimationScene
	{
	public:
		inline static constexpr auto RTTI = RTTI_BGSSynchronizedAnimationInstance;
		inline static constexpr auto VTABLE = VTABLE_BGSSynchronizedAnimationInstance;

		~BGSSynchronizedAnimationInstance() override;  // 00

		// members
		uint64_t unk18;                                     // 18
		uint64_t unk20;                                     // 20
		uint64_t unk28;                                     // 28
		BSSynchronizedClipGenerator* sourceClipGenerator;   // 30
		hkbCharacter* sourceCharacter;                      // 38
		uint32_t unk40;                                     // 40
		uint32_t unk44;                                     // 44
		BSSynchronizedClipGenerator* sourceClipGenerator2;  // 48
		hkbCharacter* sourceCharacter2;                     // 50
		uint32_t unk58;                                     // 58
		uint32_t unk5C;                                     // 5C
		BSSynchronizedClipGenerator* targetClipGenerator;   // 60
		hkbCharacter* targetCharacter;                      // 68
		uint64_t unk70;                                     // 70
		uint64_t unk78;                                     // 78
		uint64_t unk80;                                     // 80
		uint64_t unk88;                                     // 88
		uint64_t unk90;                                     // 90
		uint64_t unk98;                                     // 98
	};
	static_assert(sizeof(BGSSynchronizedAnimationInstance) == 0xA0);

	class hkResource : public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI = RTTI_hkResource;
		inline static constexpr auto VTABLE = VTABLE_hkResource;
	};
	static_assert(sizeof(hkResource) == 0x10);

	class hkLoader : public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI = RTTI_hkLoader;
		inline static constexpr auto VTABLE = VTABLE_hkLoader;

		// members
		hkArray<hkResource*> loadedData;  // 10
	};
	static_assert(sizeof(hkLoader) == 0x20);
	
	namespace BShkbHkxDB
	{
		struct DBData : public hkLoader
		{
		public:
			inline static constexpr auto RTTI = RTTI_BShkbHkxDB__DBData;
			inline static constexpr auto VTABLE = VTABLE_BShkbHkxDB__DBData;

			// members
			hkArray<hkResource*> loadedData;  // 20
		};

		struct HashedData
		{
			RE::AnimationFileManagerSingleton::AnimationFileInfo animationFileInfo;  // 00
			uint32_t unk0C;   // 0C
			uint32_t unk10;   // 10
			uint32_t unk14;   // 14
			uint32_t unk18;   // 18
			uint32_t unk1C;   // 1C
		};
		static_assert(sizeof(HashedData) == 0x20);

		struct HashedBehaviorData
		{
			HashedData hashedAnimData;  // 00
			BSResource::Stream* stream;     // 20
			DBData* data;                   // 28
		};
		static_assert(sizeof(HashedBehaviorData) == 0x30);		

		struct ProjectDBData : public hkLoader
		{
		public:
			inline static constexpr auto RTTI = RTTI_BShkbHkxDB__ProjectDBData;
			inline static constexpr auto VTABLE = VTABLE_BShkbHkxDB__ProjectDBData;

			// members
			uint32_t unk20;
			uint32_t unk24;
			BSTArray<HashedBehaviorData*> hashedBehaviors;  // 28
			BSTArray<HashedData> hashedAnimations;          // 40
			uint32_t unk58;
			uint32_t unk5C;
			uint32_t unk60;
			uint32_t unk64;
			uint32_t unk68;
			uint32_t unk6C;
			uint8_t variableNamesToIds[0x30];               // 70 - BSTHashMap<char *, hkInt32> // variable name -> event id
			uint8_t eventNamesToIds[0x30];                  // A0 - BSTHashMap<char *, hkInt32> // event name -> event id. This one is read from when handling anim events.
			BSTArray<char*> eventNames;                     // D0 - all anim events (~2000 total)
			BSTArray<char*> unkE8;                          // E8 - state names?
			uint64_t unk100;                                // 100
			uint64_t unk108;                                // 108
			uint64_t unk110;                                // 110
			uint64_t unk118;                                // 118
			uint64_t unk120;                                // 120
			uint64_t unk128;                                // 128
			uint64_t unk130;                                // 130
			uint64_t unk138;                                // 138
			uint64_t unk140;                                // 140
			uint64_t unk148;                                // 148
			hkbAnimationBindingSet* bindings;               // 150
			hkbSymbolIdMap* characterPropertyIdMap;         // 158
			hkbProjectData* projectData;                    // 160
			uint32_t unk168;                                // 168
			uint32_t unk16C;                                // 16C
			uint64_t unk170;                                // 170
			uint64_t unk178;                                // 178
		};
		static_assert(offsetof(ProjectDBData, eventNamesToIds) == 0xA0);
		static_assert(offsetof(ProjectDBData, projectData) == 0x160);
	}
}

inline float* Track_getData(const RE::hkbGeneratorOutput& a_output, RE::hkbGeneratorOutput::TrackHeader& a_header)
{
	return reinterpret_cast<float*>(reinterpret_cast<char*>(a_output.tracks) + a_header.dataOffset);
}

inline int8_t* Track_getIndices(RE::hkbGeneratorOutput& a_output, RE::hkbGeneratorOutput::TrackHeader& a_header)
{
	auto NextMultipleOf = [](auto a_alignment, auto a_value) { return ((a_value) + ((a_alignment)-1)) & (~((a_alignment)-1)); };

	// must be sparse or pallette track
	int numDataBytes = NextMultipleOf(16, a_header.elementSizeBytes * a_header.capacity);
	return reinterpret_cast<int8_t*>(Track_getData(a_output, a_header)) + numDataBytes;
}

inline RE::hkbGeneratorOutput::TrackHeader* GetTrackHeader(const RE::hkbGeneratorOutput& a_generatorOutput, RE::hkbGeneratorOutput::StandardTracks a_track)
{
	int trackId = (int)a_track;
	int32_t numTracks = a_generatorOutput.tracks->masterHeader.numTracks;
	return numTracks > trackId ? &(a_generatorOutput.tracks->trackHeaders[trackId]) : nullptr;
}
