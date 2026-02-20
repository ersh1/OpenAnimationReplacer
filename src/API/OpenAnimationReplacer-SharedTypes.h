#pragma once

namespace Components
{
	enum class ActorValueType : int
	{
		kActorValue,
		kBase,
		kMax,
		kPercentage
	};

	enum class GraphVariableType : int
	{
		kFloat,
		kInt,
		kBool
	};
}

class IStateData
{
public:
	virtual ~IStateData() = default;
	virtual bool Update([[maybe_unused]] float a_deltaTime) { return false; }
	virtual void OnLoopOrEcho([[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] bool a_bIsEcho) {}
	virtual bool ShouldResetOnLoopOrEcho([[maybe_unused]] RE::hkbClipGenerator* a_clipGenerator, [[maybe_unused]] bool a_bIsEcho) const { return false; }
};
