#pragma once

#include "BaseFunctions.h"

namespace Functions
{
	[[nodiscard]] std::unique_ptr<IFunction> CreateFunctionFromJson(rapidjson::Value& a_value, FunctionSet* a_parentFunctionSet = nullptr);
	[[nodiscard]] std::unique_ptr<IFunction> CreateFunction(std::string_view a_functionName);
	[[nodiscard]] std::unique_ptr<IFunction> DuplicateFunction(const std::unique_ptr<IFunction>& a_functionToDuplicate);
	[[nodiscard]] std::unique_ptr<FunctionSet> DuplicateFunctionSet(FunctionSet* a_functionSetToDuplicate);

	class InvalidFunction : public FunctionBase
	{
	public:
		InvalidFunction() = default;

		InvalidFunction(std::string_view a_argument)
		{
			_argument = a_argument;
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return _argument.data(); }
		[[nodiscard]] RE::BSString GetName() const override { return "! INVALID !"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "The function was not found!"sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 0, 0, 0 }; }

		[[nodiscard]] bool IsValid() const override { return false; }

	protected:
		bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Trigger*) const override { return false; }
		std::string _argument;
	};

	class InvalidNonEssentialFunction : public FunctionBase
	{
	public:
		InvalidNonEssentialFunction() = default;

		InvalidNonEssentialFunction(std::string_view a_argument, std::string_view a_name, const rapidjson::Value& a_json)
		{
			_argument = a_argument;
			_name = a_name;
			_json.CopyFrom(a_json, _json.GetAllocator());
		}

		void Serialize(void* a_value, void* a_allocator, IFunction* a_outerCustomFunction = nullptr) override;

		[[nodiscard]] RE::BSString GetArgument() const override { return _argument.data(); }
		[[nodiscard]] RE::BSString GetName() const override;
		[[nodiscard]] RE::BSString GetDescription() const override;
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 0, 0, 0 }; }

		[[nodiscard]] bool IsValid() const override { return true; }

	protected:
		bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Trigger*) const override;
		std::string _argument;
		std::string _name;
		rapidjson::Document _json;
	};

	class CONDITIONFunction : public FunctionBase
	{
	public:
		CONDITIONFunction()
		{
			conditionComponent = AddComponent<ConditionFunctionComponent>("Conditions");
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return conditionComponent->GetArgument(); };

		[[nodiscard]] RE::BSString GetName() const override { return "CONDITION"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Run a set of functions only if the specified conditions evaluate to true."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		ConditionFunctionComponent* conditionComponent;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;
	};

	class RANDOMFunctionComponent : public MultiFunctionComponent
	{
	public:
		RANDOMFunctionComponent(const IFunction* a_parentFunction, const char* a_name, const char* a_description = "") :
			MultiFunctionComponent(a_parentFunction, a_name, a_description)
		{
			std::function<void()> fun = std::bind(&RANDOMFunctionComponent::UpdateWeightCache, this);
			functionSet->AddOnDirtyCallback(fun);
		}

		void InitializeComponent(void* a_value) override;
		void SerializeComponent(void* a_value, void* a_allocator) override;

		bool DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent) override;

		bool Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, void* a_trigger) const override;

		std::vector<float> weights;

	protected:
		void UpdateWeightCache();

		mutable SharedLock _lock;
		std::vector<IFunction*> _functions;
		std::discrete_distribution<> _dist;
		std::vector<float> _cumulativeWeights;
	};

	class RANDOMFunction : public FunctionBase
	{
	public:
		RANDOMFunction()
		{
			randomFunctionComponent = AddComponent<RANDOMFunctionComponent>("Functions");
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return randomFunctionComponent->GetArgument(); }

		[[nodiscard]] RE::BSString GetName() const override { return "RANDOM"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Runs one random function from the contained function set."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		RANDOMFunctionComponent* randomFunctionComponent;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;
	};

	class ONEFunction : public FunctionBase
	{
	public:
		ONEFunction()
		{
			multiFunctionComponent = AddComponent<MultiFunctionComponent>("Functions");
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return multiFunctionComponent->GetArgument(); }

		[[nodiscard]] RE::BSString GetName() const override { return "ONE"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Attempts to run functions from the contained function set in top-down order, until the first one succeeds. Mostly intended to be used with CONDITION functions inside, or other functions that contain an internal check before running."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		MultiFunctionComponent* multiFunctionComponent;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;
	};

	class PlaySoundFunction : public FunctionBase
	{
	public:
		PlaySoundFunction()
		{
			formComponent = AddComponent<FormFunctionComponent>("Sound FormID");
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }

		[[nodiscard]] RE::BSString GetName() const override { return "PlaySound"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Plays a sound at the ref's location."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		FormFunctionComponent* formComponent;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;
	};

	class ModActorValueFunction : public FunctionBase
	{
	public:
		ModActorValueFunction()
		{
			actorValueComponent = AddComponent<NumericFunctionComponent>("Actor value");
			actorValueComponent->SetForcedType(Components::NumericValue::Type::kActorValue);
			valueToWriteComponent = AddComponent<NumericFunctionComponent>("Value to modify by");
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return actorValueComponent->GetArgument(); }

		[[nodiscard]] RE::BSString GetName() const override { return "ModActorValue"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Modifies an actor value by a given value."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		NumericFunctionComponent* actorValueComponent;
		NumericFunctionComponent* valueToWriteComponent;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;
	};

	class SetGraphVariableFunction : public FunctionBase
	{
	public:
		SetGraphVariableFunction()
		{
			graphVariableComponent = AddComponent<NumericFunctionComponent>("Graph variable");
			graphVariableComponent->SetForcedType(Components::NumericValue::Type::kGraphVariable);
			valueToWriteComponent = AddComponent<NumericFunctionComponent>("Value to set");
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return graphVariableComponent->GetArgument(); }

		[[nodiscard]] RE::BSString GetName() const override { return "SetGraphVariable"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Sets a graph variable."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		NumericFunctionComponent* graphVariableComponent;
		NumericFunctionComponent* valueToWriteComponent;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;
	};

	class SendAnimEventFunction : public FunctionBase
	{
	public:
		SendAnimEventFunction()
		{
			eventNameComponent = AddComponent<TextFunctionComponent>("Event name");
			eventNameComponent->SetAllowSpaces(false);
		}

		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] RE::BSString GetName() const override { return "SendAnimEvent"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Sends a behavior graph event."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		TextFunctionComponent* eventNameComponent;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;
	};

	class CastSpellFunction : public FunctionBase
	{
	public:
		CastSpellFunction()
		{
			formComponent = AddComponent<FormFunctionComponent>("Spell FormID");
			selfTarget = AddComponent<BoolFunctionComponent>("Self target");
			effectiveness = AddComponent<NumericFunctionComponent>("Effectiveness");
			magnitude = AddComponent<NumericFunctionComponent>("Magnitude");
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }

		[[nodiscard]] RE::BSString GetName() const override { return "CastSpell"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Casts a spell."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		FormFunctionComponent* formComponent;
		BoolFunctionComponent* selfTarget;
		NumericFunctionComponent* effectiveness;
		NumericFunctionComponent* magnitude;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;
	};

	class DispelSpellFunction : public FunctionBase
	{
	public:
		DispelSpellFunction()
		{
			formComponent = AddComponent<FormFunctionComponent>("Spell FormID");
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return formComponent->GetArgument(); }

		[[nodiscard]] RE::BSString GetName() const override { return "DispelSpell"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Dispels a spell."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		FormFunctionComponent* formComponent;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;
	};

	class SpawnParticleFunction : public FunctionBase
	{
	public:
		SpawnParticleFunction()
		{
			path = AddComponent<TextFunctionComponent>("Particle .nif path", "Relative to the meshes/ folder");
			actorNodeIndex = AddComponent<NumericFunctionComponent>("Actor node index");
			scale = AddComponent<NumericFunctionComponent>("Scale");
			playTime = AddComponent<NumericFunctionComponent>("Play time");
			offset = AddComponent<NiPoint3FunctionComponent>("Offset");

			scale->SetStaticValue(1);

			actorNodeIndex->SetForcedType(Components::NumericValue::Type::kStaticValue);
			actorNodeIndex->SetStaticRange(0, static_cast<float>(RE::BIPED_OBJECT::kTotal - 1));
			actorNodeIndex->SetStaticInteger(true);
		}

		[[nodiscard]] RE::BSString GetArgument() const override { return path->GetArgument(); }

		[[nodiscard]] RE::BSString GetName() const override { return "SpawnParticle"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Spawns a particle."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		TextFunctionComponent* path;
		NumericFunctionComponent* actorNodeIndex;
		NumericFunctionComponent* scale;
		NumericFunctionComponent* playTime;
		NiPoint3FunctionComponent* offset;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;
	};

	class UnequipSlotFunction : public FunctionBase
	{
	public:
		UnequipSlotFunction()
		{
			slotComponent = AddComponent<NumericFunctionComponent>("Slot", "The equipment slot.");
		}

		void PostInitialize() override;
		[[nodiscard]] RE::BSString GetArgument() const override;

		[[nodiscard]] RE::BSString GetName() const override { return "UnequipSlot"sv.data(); }
		[[nodiscard]] RE::BSString GetDescription() const override { return "Unequips an item from the specified slot."sv.data(); }
		[[nodiscard]] constexpr REL::Version GetRequiredVersion() const override { return { 3, 0, 0 }; }

		NumericFunctionComponent* slotComponent;

	protected:
		bool RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const override;

		static std::map<int32_t, std::string_view> GetEnumMap();
	};
}
