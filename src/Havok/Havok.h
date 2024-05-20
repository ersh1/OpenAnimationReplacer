#pragma once

static uint32_t& g_dwTlsIndex = *(uint32_t*)REL::VariantID(520865, 407383, 0x30A8C04).address();  // 2F50B74, 2FEB6F4, 30A8C04

namespace RE
{
	class hkClass
	{
	public:
		enum class FlagValues
		{
			kNone = 0,
			kNotSerializable = 1
		};

		const char* name;                              // 00
		const hkClass* parent;                         // 08
		int32_t objectSize;                            // 10
		int32_t numImplementedInterfaces;              // 14
		class hkClassEnum* declaredEnums;              // 18
		int32_t numDeclaredEnums;                      // 20
		uint32_t pad24;                                // 24
		class hkClassMember* declaredMembers;          // 28
		int32_t numDeclaredMembers;                    // 30
		uint32_t pad34;                                // 34
		void* defaults;                                // 38
		class hkCustomAttributes* attributes;          // 40
		stl::enumeration<FlagValues, uint32_t> flags;  // 08
		int32_t describedVersion;                      // 4C
	};
	static_assert(sizeof(hkClass) == 0x50);

	template <class Key, class Value>
	class hkMapBase
	{
	public:
		struct Pair
		{
			Key key;
			Value value;
		};

		Pair* _data;            // 00
		int32_t _sizeAndFlags;  // 08
		int32_t _hashMod;       // 0C
	};

	template <class Key, class Value>
	class hkMap : public hkMapBase<Key, Value>
	{
	public:
	};

	template <class Key, class Value, class Allocator = void>
	class hkPointerMap
	{
	protected:
		hkMap<Key, Value> _map;  // 00
	};
	static_assert(sizeof(hkPointerMap<void*, void*>) == 0x10);

	template <class T>
	class hkQueue
	{
	public:
		T* _data;                // 00
		int32_t _capacity;       // 08
		int32_t _head;           // 0C
		int32_t _tail;           // 10
		int32_t _elementsInUse;  // 14
	};
	static_assert(sizeof(hkQueue<void*>) == 0x18);

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
			TrackMasterHeader masterHeader;                                                                               // 00
			TrackHeader trackHeaders[static_cast<int32_t>(RE::hkbGeneratorOutput::StandardTracks::NUM_STANDARD_TRACKS)];  // 10
		};

		class Track
		{
		public:
			Track(TrackHeader* a_header, float* a_data) :
				header(a_header),
				data(a_data) {}

			[[nodiscard]] TrackHeader* GetTrackHeader() const { return header; }
			[[nodiscard]] float* GetDataReal() const { return data; }
			[[nodiscard]] hkQsTransform* GetDataQsTransform() const { return reinterpret_cast<hkQsTransform*>(data); }
			[[nodiscard]] const int8_t* GetIndices() const;
			[[nodiscard]] bool IsValid() const { return header->onFraction > 0.f; }
			[[nodiscard]] int16_t GetNumData() const { return header->numData; }
			[[nodiscard]] int16_t GetCapacity() const { return header->capacity; }
			[[nodiscard]] int16_t GetElementSizeBytes() const { return header->elementSizeBytes; }
			[[nodiscard]] TrackTypes GetType() const { return *header->type; }
			[[nodiscard]] float GetOnFraction() const { return header->onFraction; }
			[[nodiscard]] bool IsPalette() const { return header->flags.any(TrackFlags::TRACK_FLAG_PALETTE); }
			[[nodiscard]] bool IsSparse() const { return header->flags.any(TrackFlags::TRACK_FLAG_SPARSE); }

		protected:
			TrackHeader* header;  // 00
			float* data;          // 08
		};

		bool TrackExists(StandardTracks a_trackId) const { return tracks->masterHeader.numTracks > static_cast<int32_t>(a_trackId); }
		bool IsValid(StandardTracks a_trackId) const;
		Track GetTrack(StandardTracks a_trackId);

		Tracks* tracks;     // 00
		bool deleteTracks;  // 08
	};

	inline const int8_t* hkbGeneratorOutput::Track::GetIndices() const
	{
		auto nextMultipleOf = [](auto a_alignment, auto a_value) { return a_value + (a_alignment - 1) & ~(a_alignment - 1); };

		// must be sparse or palette track
		const int numDataBytes = nextMultipleOf(16, header->elementSizeBytes * header->capacity);
		return reinterpret_cast<int8_t*>(data) + numDataBytes;
	}

	inline bool hkbGeneratorOutput::IsValid(StandardTracks a_trackId) const
	{
		if (TrackExists(a_trackId)) {
			return tracks->trackHeaders[static_cast<int32_t>(a_trackId)].onFraction > 0.f;
		}

		return false;
	}

	inline hkbGeneratorOutput::Track hkbGeneratorOutput::GetTrack(StandardTracks a_trackId)
	{
		TrackHeader* header = &tracks->trackHeaders[static_cast<int32_t>(a_trackId)];
		auto data = reinterpret_cast<float*>(reinterpret_cast<char*>(tracks) + tracks->trackHeaders[static_cast<int32_t>(a_trackId)].dataOffset);
		return { header, data };
	}

	class hkbAnimationBindingWithTriggers : public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI = RTTI_hkbAnimationBindingWithTriggers;
		inline static constexpr auto VTABLE = VTABLE_hkbAnimationBindingWithTriggers;

		struct Trigger
		{
			Trigger(float a_time, int32_t a_eventId, hkbEventPayload* a_payload) :
				time(a_time),
				eventId(a_eventId),
				payload(a_payload)
			{}

			float time = 0.f;                             // 00
			uint32_t pad04 = 0;                           // 04
			int32_t eventId = -1;                         // 08
			hkRefPtr<hkbEventPayload> payload = nullptr;  // 10
		};
		static_assert(offsetof(Trigger, eventId) == 0x8);
		static_assert(sizeof(Trigger) == 0x18);

		// members
		hkRefPtr<hkaAnimationBinding> binding;  // 10
		hkArray<Trigger> triggers;              // 18
	};
	static_assert(sizeof(hkbAnimationBindingWithTriggers) == 0x28);

	class hkbSymbolIdMap : public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI = RTTI_hkbSymbolIdMap;
		inline static constexpr auto VTABLE = VTABLE_hkbSymbolIdMap;

		hkArray<int32_t> internalToExternalMap;
		hkPointerMap<int32_t, int32_t> externalToInternalMap;
	};

	class BSSynchronizedClipGenerator : public hkbGenerator
	{
	public:
		class hkbDefaultSynchronizedScene : public hkReferencedObject
		{
		public:
			inline static constexpr auto RTTI = RTTI_BSSynchronizedClipGenerator__hkbDefaultSynchronizedScene;
			inline static constexpr auto VTABLE = VTABLE_BSSynchronizedClipGenerator__hkbDefaultSynchronizedScene;

			~hkbDefaultSynchronizedScene() override;  // 00

			// add
			virtual bool AreAllClipsActivated();          // 03
			virtual bool IsDoneReorientingSupportChar();  // 04
			virtual float GetUnk18();                     // 05
			virtual void UpdateUnk94(float a1);           // 06
			virtual void OnSynchronizedClipDeactivate();  // 07
			virtual void OnSynchronizedClipActivate();    // 08
			virtual void Unk_09(void);                    // 09

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
		uint64_t pad48;                                             // 48
		hkbClipGenerator* clipGenerator;                            // 50
		hkStringPtr syncAnimPrefix;                                 // 58
		bool syncClipIgnoreMarkPlacement;                           // 60
		float getToMarkTime;                                        // 64
		float markErrorThreshold;                                   // 68
		bool leadCharacter;                                         // 6C
		bool reorientSupportChar;                                   // 6D
		bool applyMotionFromRoot;                                   // 6E
		class BGSSynchronizedAnimationInstance* synchronizedScene;  // 70
		uint32_t unk78;                                             // 78
		uint32_t unk7C;                                             // 7C
		hkQsTransform startMarkWS;                                  // 80
		hkQsTransform endMarkWS;                                    // B0
		hkQsTransform startMarkMS;                                  // E0
		float currentLerp;                                          // 110
		hkaAnimationBinding* localSyncBinding;                      // 118
		hkPointerMap<int32_t, int32_t>* eventMap;                   // 120  hkPointerMap<int, int>*
		uint16_t animationBindingIndex;                             // 128
		bool atMark;                                                // 12A
		bool allCharactersInScene;                                  // 12B
		bool allCharactersAtMarks;                                  // 12C
	};
	static_assert(sizeof(BSSynchronizedClipGenerator) == 0x130);
	static_assert(offsetof(BSSynchronizedClipGenerator, animationBindingIndex) == 0x128);

	namespace BSSynchronizedClipGeneratorUtils
	{
		class FindEventFunctor
		{
		public:
			inline static constexpr auto RTTI = RTTI_BSSynchronizedClipGeneratorUtils__FindEventFunctor;
			inline static constexpr auto VTABLE = VTABLE_BSSynchronizedClipGeneratorUtils__FindEventFunctor;

			virtual ~FindEventFunctor() = default;  // 00

			virtual void Unk_01([[maybe_unused]] const char* a_key) {}  // 01
		};
		static_assert(sizeof(FindEventFunctor) == 0x8);

		class BSHashMapEventFindFunctor : public FindEventFunctor
		{
		public:
			inline static constexpr auto RTTI = RTTI___BSHashMapEventFindFunctor;
			inline static constexpr auto VTABLE = VTABLE___BSHashMapEventFindFunctor;

			BSHashMapEventFindFunctor() { stl::emplace_vtable(this); }

			~BSHashMapEventFindFunctor() override {}  // 00

			// override (FindEventFunctor)
			void Unk_01([[maybe_unused]] const char* a_key) override {}  // 01

			// members
			BSTHashMap<BSFixedString, uint32_t>* map;  // 08
		};
		static_assert(sizeof(BSHashMapEventFindFunctor) == 0x10);
	}

	class BGSSynchronizedAnimationInstance : public BSSynchronizedClipGenerator::hkbSynchronizedAnimationScene
	{
	public:
		struct ActorSyncInfo
		{
			ActorHandle refHandle;                                   // 00
			uint32_t pad04;                                          // 04
			BSSynchronizedClipGenerator* synchronizedClipGenerator;  // 08
			hkbCharacter* character;                                 // 10
		};
		static_assert(sizeof(ActorSyncInfo) == 0x18);

		inline static constexpr auto RTTI = RTTI_BGSSynchronizedAnimationInstance;
		inline static constexpr auto VTABLE = VTABLE_BGSSynchronizedAnimationInstance;

		~BGSSynchronizedAnimationInstance() override;  // 00

		bool HasRef(const ObjectRefHandle& a_handle)
		{
			using func_t = decltype(&BGSSynchronizedAnimationInstance::HasRef);
			REL::Relocation<func_t> func{ REL::VariantID(32031, 32784, 0x4FB080) };  // 4EAE10, 503680, 4FB080
			return func(this, a_handle);
		}

		ActorHandle& GetLeadRef(ActorHandle& a_outHandle)
		{
			using func_t = decltype(&BGSSynchronizedAnimationInstance::GetLeadRef);
			REL::Relocation<func_t> func{ REL::VariantID(32032, 32785, 0x4FB140) };  // 4EAED0, 503680, 4FB140
			return func(this, a_outHandle);
		}

		float unk18;                                     // 18  // nothing reads this?
		uint32_t numActors;                              // 1C
		BSTSmallArray<ActorSyncInfo, 3> actorSyncInfos;  // 20
		BSTSmallArray<ObjectRefHandle, 2> refHandles;    // 78
		uint32_t numActivated;                           // 90
		uint32_t currentGenerator;                       // 94  // Generate calls a function that constantly cycles this from 0 to numActivated, on 0 sets unk18. nothing else seems to access them
		uint32_t lastTimestampMs;                        // 98
		uint32_t pad9C;                                  // 9C
	};
	static_assert(sizeof(BGSSynchronizedAnimationInstance) == 0xA0);

	class SynchronizedAnimationManager
	{
	public:
		// members
		uint64_t unk00;                                                             // 00
		BSTArray<BGSSynchronizedAnimationInstance> synchronizedAnimationInstances;  // 08
		BSReadWriteLock lock;                                                       // 20
		uint64_t unk28;                                                             // 28
	};

	class hkbEventPayload : public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI = RTTI_hkbEventPayload;
		inline static constexpr auto VTABLE = VTABLE_hkbEventPayload;
	};
	static_assert(sizeof(hkbEventPayload) == 0x10);

	class hkbStringEventPayload : public hkbEventPayload
	{
	public:
		inline static constexpr auto RTTI = RTTI_hkbStringEventPayload;
		inline static constexpr auto VTABLE = VTABLE_hkbStringEventPayload;

		// members
		hkStringPtr data;  // 10
	};
	static_assert(sizeof(hkbStringEventPayload) == 0x18);

	class hkbEventQueue
	{
	public:
		hkQueue<hkbEvent> queue;      // 00
		hkbSymbolIdMap* symbolIdMap;  // 18
		bool isInternal;              // 20
	};
	static_assert(sizeof(hkbEventQueue) == 0x28);

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
			uint64_t unk20;  // 20
		};
		static_assert(sizeof(DBData) == 0x28);

		struct HashedData
		{
			RE::AnimationFileManagerSingleton::AnimationFileInfo animationFileInfo;  // 00
			uint32_t unk0C;                                                          // 0C
			uint32_t unk10;                                                          // 10
			uint32_t unk14;                                                          // 14
			uint32_t unk18;                                                          // 18
			uint32_t unk1C;                                                          // 1C
		};
		static_assert(sizeof(HashedData) == 0x20);

		struct HashedBehaviorData
		{
			HashedData hashedAnimData;   // 00
			BSResource::Stream* stream;  // 20
			DBData* data;                // 28
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
			BSTHashMap<BSFixedString, uint32_t> variableNamesToIds;  // variable name -> event id
			BSTHashMap<BSFixedString, uint32_t> eventNamesToIds;     // event name -> event id. This one is read from when handling anim events.
			BSTArray<char*> eventNames;                              // D0 - all anim events (~2000 total)
			BSTArray<char*> unkE8;                                   // E8 - state names?
			uint64_t unk100;                                         // 100
			uint64_t unk108;                                         // 108
			uint64_t unk110;                                         // 110
			uint64_t unk118;                                         // 118
			uint64_t unk120;                                         // 120
			uint64_t unk128;                                         // 128
			uint64_t unk130;                                         // 130
			uint64_t unk138;                                         // 138
			uint64_t unk140;                                         // 140
			uint64_t unk148;                                         // 148
			hkbAnimationBindingSet* bindings;                        // 150
			hkbSymbolIdMap* characterPropertyIdMap;                  // 158
			hkbProjectData* projectData;                             // 160
			uint32_t unk168;                                         // 168
			uint32_t unk16C;                                         // 16C
			uint64_t unk170;                                         // 170
			uint64_t unk178;                                         // 178
		};
		static_assert(offsetof(ProjectDBData, eventNamesToIds) == 0xA0);
		static_assert(offsetof(ProjectDBData, projectData) == 0x160);
		static_assert(sizeof(ProjectDBData) == 0x180);
	}

	struct UnkIteratorStruct
	{
		ScrapHeap* scrapHeap_00;
		uint64_t unk_08;
		uint64_t unk_10;
		uint64_t unk_18;
		void* unk_20;
		uint64_t unk_28;
		uint64_t unk_30;
		uint32_t unk_38;
		uint32_t unk_3C;
		uint64_t unk_40;
		void* unk_48;
		uint64_t unk_50;
		uint32_t getChildrenFlags_58;
		uint32_t unk_5C;
	};

	class hkMemoryRouter
	{
	public:
		uint64_t unk00;             // 00
		uint64_t unk08;             // 08
		uint64_t unk10;             // 10
		uint64_t unk18;             // 18
		uint64_t unk20;             // 20
		uint64_t unk28;             // 28
		uint64_t unk30;             // 30
		uint64_t unk38;             // 38
		uint64_t unk40;             // 40
		uint64_t unk48;             // 48
		hkMemoryAllocator* temp;    // 50
		hkMemoryAllocator* heap;    // 58
		hkMemoryAllocator* debug;   // 60
		hkMemoryAllocator* solver;  // 68
		void* userData;             // 70
	};
	static_assert(offsetof(hkMemoryRouter, heap) == 0x58);

	class AnimationClipDataSingleton :
		public BSTSingletonSDM<AnimationClipDataSingleton>
	{
	public:
		inline static auto RTTI = RTTI_AnimationClipDataSingleton;
		inline static auto VTABLE = VTABLE_AnimationClipDataSingleton;

		class AnimationClipData : public BSIntrusiveRefCounted
		{
		public:
			uint32_t pad04;                        // 04
			BSTHashMap<uint64_t, uint64_t> map08;  // 08
			BSTArray<uint64_t> array38;            // 38
			BSTArray<uint64_t> array50;            // 50
		};
		static_assert(sizeof(AnimationClipData) == 0x68);

		virtual ~AnimationClipDataSingleton();  // 00

		[[nodiscard]] static AnimationClipDataSingleton* GetSingleton()
		{
			REL::Relocation<AnimationClipDataSingleton**> singleton{ RELOCATION_ID(515414, 401553) };
			return *singleton;
		}

		// members
		//uint64_t unk08;                            // 08
		BSTHashMap<BSFixedString, NiPointer<AnimationClipData>> hashMap;  // 10
		BSTArray<uint64_t> array;                                         // 40
		uint64_t unk58;                                                   // 58
		uint64_t unk60;                                                   // 60
		uint64_t unk68;                                                   // 68
		uint64_t unk70;                                                   // 70
		uint64_t unk78;                                                   // 78
		uint64_t unk80;                                                   // 80
	};
	static_assert(sizeof(AnimationClipDataSingleton) == 0x88);

	class AnimationSetDataSingleton :
		public BSTSingletonSDM<AnimationSetDataSingleton>
	{
	public:
		struct AnimationSetData
		{
			BSTSmallArray<BSFixedString> triggerEvents;  // 00
			BSTArray<void*> variableExpressions;         // 18
			uint64_t counter30;                          // 30
			void* unk38;                                 // array? it seems to be a pointer to array contents, a few CRCs
			uint64_t counter40;                          // 40
			uint64_t counter48;                          // 48
			BSTArray<void*>* cacheInfos;                 // 50, pointer to array of CRCs? list of anim files?
			uint64_t unk58;                              // always 0?
			uint64_t unk60;                              // always 0?
			uint64_t counter68;                          // all 4 counters have the same value. seem relative to the amount of data in unk38
			BSTArray<void*> attackEntries;               // 70
		};
		static_assert(sizeof(AnimationSetData) == 0x88);

		[[nodiscard]] static AnimationSetDataSingleton* GetSingleton()
		{
			REL::Relocation<AnimationSetDataSingleton**> singleton{ RELOCATION_ID(515415, 401554) };
			return *singleton;
		}

		// members
		BSTHashMap<BSFixedString, BSTArray<AnimationSetData>*> hashMap;  // 08
	};
	static_assert(sizeof(AnimationSetDataSingleton) == 0x38);

	struct PathingData
	{
		NiPoint3 unk00;
		uint32_t unk0C;
		BSNavmeshInfo* navmeshInfo;  // can get BSNavmesh with GetBSNavmesh_1410D7C40
		BSTArray<BSNavmeshInfo*>* navmeshInfos;  // array of same type as unk10? can be null, possibly only filled in queries and not in the stored one in aiprocess
		PathingCell* unk20;
		uint16_t navmeshTriangleId;
		uint8_t unk2A;
		uint8_t unk2B;
	};
}

inline RE::hkMemoryRouter& hkGetMemoryRouter() { return *(RE::hkMemoryRouter*)(uintptr_t)SKSE::WinAPI::TlsGetValue(g_dwTlsIndex); }
inline void* hkHeapAlloc(int numBytes) { return hkGetMemoryRouter().heap->BlockAlloc(numBytes); }
