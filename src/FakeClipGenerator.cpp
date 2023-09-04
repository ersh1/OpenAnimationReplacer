#include "FakeClipGenerator.h"

#include "Havok/Havok.h"
#include "Offsets.h"
#include "ReplacementAnimation.h"
#include "Settings.h"

FakeClipGenerator::FakeClipGenerator(RE::hkbClipGenerator* a_clipGenerator) :
	//userData(a_clipGenerator->userData),
	cropStartAmountLocalTime(a_clipGenerator->cropStartAmountLocalTime),
	cropEndAmountLocalTime(a_clipGenerator->cropEndAmountLocalTime),
	startTime(a_clipGenerator->startTime),
	playbackSpeed(a_clipGenerator->playbackSpeed),
	enforcedDuration(a_clipGenerator->enforcedDuration),
	userControlledTimeFraction(a_clipGenerator->userControlledTimeFraction),
	animationBindingIndex(a_clipGenerator->animationBindingIndex),
	mode(a_clipGenerator->mode),
	flags(a_clipGenerator->flags),
	animationControl(a_clipGenerator->animationControl),
	mapperData(a_clipGenerator->mapperData),
	binding(a_clipGenerator->binding),
	extractedMotion(a_clipGenerator->extractedMotion),
	localTime(a_clipGenerator->localTime),
	time(a_clipGenerator->time),
	previousUserControlledTimeFraction(a_clipGenerator->previousUserControlledTimeFraction),
	atEnd(a_clipGenerator->atEnd),
	ignoreStartTime(a_clipGenerator->ignoreStartTime),
	pingPongBackward(a_clipGenerator->pingPongBackward)
{}

FakeClipGenerator::FakeClipGenerator(RE::hkbBehaviorGraph* a_behaviorGraph, const ReplacementAnimation* a_replacementAnimation, std::string_view a_syncAnimationPrefix, std::optional<uint16_t> a_variantIndex /* = std::nullopt*/)
{
	syncAnimationPrefix = a_syncAnimationPrefix;
	animationBindingIndex = a_variantIndex.has_value() ? *a_variantIndex : a_replacementAnimation->GetIndex();
	mode = RE::hkbClipGenerator::PlaybackMode::kModeLooping;

	const RE::BShkbAnimationGraph* graph = reinterpret_cast<RE::BShkbAnimationGraph*>(a_behaviorGraph->userData);
	const RE::hkbCharacter* character = &graph->characterInstance;
	const RE::hkbContext* context = reinterpret_cast<RE::hkbContext*>(&character);
	Activate(*context);
}

FakeClipGenerator::~FakeClipGenerator()
{
	struct FakeCharacter
	{
		FakeCharacter(RE::hkbBehaviorGraph* a_graph) :
			graph(a_graph) {}

		uint64_t pad00;
		uint64_t pad08;
		uint64_t pad10;
		uint64_t pad18;
		uint64_t pad20;
		uint64_t pad28;
		uint64_t pad30;
		uint64_t pad38;
		uint64_t pad40;
		uint64_t pad48;
		uint64_t pad50;
		RE::hkbBehaviorGraph* graph;
	};

	struct FakeContext
	{
		FakeContext(FakeCharacter* a_character) :
			character(a_character) {}

		FakeCharacter* character;
	};

	if (userData) {
		FakeCharacter fakeCharacter(behaviorGraph.get());
		FakeContext fakeContext(&fakeCharacter);
		const auto& fakeContextPtr = reinterpret_cast<RE::hkbContext&>(fakeContext);
		const auto fakeClipGeneratorPtr = reinterpret_cast<RE::hkbClipGenerator*>(this);
		RE::AnimationFileManagerSingleton::GetSingleton()->Unload(fakeContextPtr, fakeClipGeneratorPtr, nullptr);
	}
}

void FakeClipGenerator::Activate(const RE::hkbContext& a_context)
{
	const auto fakeClipGeneratorPtr = reinterpret_cast<RE::hkbClipGenerator*>(this);  // pretend this is an actual hkbClipGenerator

	behaviorGraph = a_context.character->behaviorGraph;

	// replicate the relevant part of hkbClipGenerator::Activate

	if (!userData) {
		const auto animationFileManagerSingleton = RE::AnimationFileManagerSingleton::GetSingleton();
		animationFileManagerSingleton->Queue(a_context, fakeClipGeneratorPtr, nullptr);
	}

	if (userData == 8) {
		return;
	}

	const RE::BShkbAnimationGraph* graph = reinterpret_cast<RE::BShkbAnimationGraph*>(behaviorGraph->userData);
	const auto& setup = graph->characterInstance.setup;
	auto animationBindingSet = static_cast<RE::hkbAnimationBindingSet*>(graph->characterInstance.animationBindingSet.get());
	if (!animationBindingSet) {
		animationBindingSet = setup->animationBindingSet.get();
	}

	const auto animationBindingWithTriggers = animationBindingSet->bindings[animationBindingIndex];

	// for synchronized animations - create new animation binding with transformTrackToBoneIndices filled out based on the prefix
	if (!syncAnimationPrefix.empty()) {
		std::string animPrefix;
		if (syncAnimationPrefix == Settings::synchronizedClipSourcePrefix) {
			animPrefix = "";  // the function expects it to be empty
		} else {
			animPrefix = syncAnimationPrefix;
		}
		binding = hkaSkeleton_CreateSynchronizedClipBinding(setup->animationSkeleton.get(), animationBindingWithTriggers->binding.get(), animPrefix.data());
	} else {
		binding = animationBindingWithTriggers->binding.get();
	}

	// create new animation control
	auto animControl = static_cast<RE::hkaDefaultAnimationControl*>(hkHeapAlloc(sizeof(RE::hkaDefaultAnimationControl)));
	RE::hkRefPtr animControlPtr(animControl);
	hkaDefaultAnimationControl_ctor(animControl, binding, true, 2);
	animationControl = animControlPtr;
}

void FakeClipGenerator::Update(const RE::hkbContext& a_context, float a_deltaTime)
{
	auto fakeClipGeneratorPtr = reinterpret_cast<RE::hkbClipGenerator*>(this);  // pretend this is an actual hkbClipGenerator

	// replicate what happens in hkbClipGenerator::Update. Not sure everything here is necessary but there might be rare edge cases where it matters (looping etc)

	if (userData == 8) {
		const auto animationFileManagerSingleton = RE::AnimationFileManagerSingleton::GetSingleton();
		if (animationFileManagerSingleton->Load(a_context, fakeClipGeneratorPtr, nullptr)) {
			Activate(a_context);
		}
	}

	if (fabs(playbackSpeed) <= 0.001f) {
		playbackSpeed = playbackSpeed <= 0.f ? 0.001f : -0.001f;
	}

	animationControl->localTime = localTime;
	if (atEnd && mode) {
		hkbClipGenerator_UnkUpdateFunc1(fakeClipGeneratorPtr);
	}

	if (mode == RE::hkbClipGenerator::PlaybackMode::kModeUserControlled) {
		userControlledTimeFraction = std::clamp(userControlledTimeFraction, 0.f, 1.f);
	}

	hkbClipGenerator_UnkUpdateFunc2(fakeClipGeneratorPtr);
	float prevLocalTime = 0.f;
	float newLocalTime = 0.f;
	int32_t loops = 0;
	bool newAtEnd = false;
	hkbClipGenerator_UnkUpdateFunc3_CalcLocalTime(fakeClipGeneratorPtr, a_deltaTime, &prevLocalTime, &newLocalTime, &loops, &newAtEnd);
	if (!atEnd) {
		localTime = newLocalTime;
		animationControl->localTime = newLocalTime;

		atEnd = newAtEnd;
		RE::hkbEventQueue* eventQueue = a_context.eventQueue;
		if (!eventQueue) {
			eventQueue = *reinterpret_cast<RE::hkbEventQueue**>(&a_context.character->eventQueue);
		}
		hkbClipGenerator_UnkUpdateFunc4(fakeClipGeneratorPtr, prevLocalTime, newLocalTime, loops, eventQueue);
		if ((loops & 1) != 0) {
			if (mode != RE::hkbClipGenerator::PlaybackMode::kModeUserControlled) {
				pingPongBackward = !pingPongBackward;
			}
		}
	}

	if (mode != RE::hkbClipGenerator::PlaybackMode::kModeUserControlled) {
		time += a_deltaTime * animationControl->playbackSpeed;
	}

	previousUserControlledTimeFraction = userControlledTimeFraction;
}
