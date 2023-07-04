#pragma once

// a fake clip generator to blend from when transitioning between replacer animations of the same clip (on interrupt etc), or to preview animations
// replicates the relevant parts of hkbClipGenerator so we basically can do the same thing as the update function does, to progress the clip naturally just like the game does
class FakeClipGenerator
{
public:
	FakeClipGenerator(RE::hkbClipGenerator* a_clipGenerator);
	FakeClipGenerator(RE::hkbBehaviorGraph* a_behaviorGraph, class ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex = std::nullopt);
	~FakeClipGenerator();

	void Activate(const RE::hkbContext& a_context);
	void Update(const RE::hkbContext& a_context, float a_deltaTime);

	uint64_t pad00 = 0;
	uint64_t pad08 = 0;
	uint64_t pad10 = 0;
	uint64_t pad18 = 0;
	uint64_t pad20 = 0;
	uint64_t pad28 = 0;
	uint64_t userData = 0;
	uint64_t pad38 = 0;
	uint64_t pad40 = 0;
	uint64_t pad48 = 0;
	uint64_t pad50 = 0;
	float cropStartAmountLocalTime = 0.f;                                      // 058
	float cropEndAmountLocalTime = 0.f;                                        // 05C
	float startTime = 0.f;                                                     // 060
	float playbackSpeed = 1.f;                                                 // 064
	float enforcedDuration = 0.f;                                              // 068
	float userControlledTimeFraction = 0.f;                                    // 06C
	uint16_t animationBindingIndex;                                            // 070
	SKSE::stl::enumeration<RE::hkbClipGenerator::PlaybackMode, uint8_t> mode;  // 072
	uint8_t flags = 0;                                                         // 073
	uint32_t pad74 = 0;
	uint64_t pad78 = 0;
	uint64_t pad80 = 0;
	RE::hkRefPtr<RE::hkaDefaultAnimationControl> animationControl;  // 088
	uint64_t pad90 = 0;
	RE::hkaDefaultAnimationControlMapperData* mapperData = nullptr;  // 98
	RE::hkaAnimationBinding* binding = nullptr;                      // 0A0
	uint64_t padA8 = 0;
	RE::hkQsTransform extractedMotion;  // B0
	uint64_t padE0 = 0;
	uint64_t padE8 = 0;
	float localTime = 0.f;                           // 0F0
	float time = 0.f;                                // 0F4
	float previousUserControlledTimeFraction = 0.f;  // 0F8
	uint32_t padFC = 0;
	uint32_t pad100 = 0;
	bool atEnd = false;             // 104
	bool ignoreStartTime = false;   // 105
	bool pingPongBackward = false;  // 106

	RE::hkRefPtr<RE::hkbBehaviorGraph> behaviorGraph = nullptr;
};
